/*
 * Copyright (C) 2015, 2016, 2017, 2018  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * DRCPD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * DRCPD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DRCPD.  If not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cstring>
#include <iostream>

#include <glib-object.h>
#include <glib-unix.h>

#include "configuration.hh"
#include "configuration_i18n.hh"
#include "i18n.h"
#include "view_error_sink.hh"
#include "view_inactive.hh"
#include "view_filebrowser.hh"
#include "view_filebrowser_airable.hh"
#include "view_src_app.hh"
#include "view_src_roon.hh"
#include "view_manager.hh"
#include "view_play.hh"
#include "view_search.hh"
#include "dbus_iface.h"
#include "dbus_handlers.hh"
#include "busy.hh"
#include "messages.h"
#include "messages_glib.h"
#include "fdstreambuf.hh"
#include "timeout.hh"
#include "os.h"
#include "versioninfo.h"

struct files_t
{
    struct fifo_pair dcp_fifo;
    const char *dcp_fifo_in_name;
    const char *dcp_fifo_out_name;
    guint dcp_fifo_in_event_source_id;
};

struct ui_events_processing_data_t
{
    ViewManager::Manager *vm;
};

struct dcp_fifo_dispatch_data_t
{
    files_t *files;
    ViewManager::VMIface *vm;
    Timeout::Timer timeout;

    explicit dcp_fifo_dispatch_data_t(files_t &f):
        files(&f),
        vm(nullptr)
    {}
};

struct parameters
{
    enum MessageVerboseLevel verbose_level;
    bool run_in_foreground;
    bool connect_to_session_dbus;
};

using I18nConfigMgr = Configuration::ConfigManager<Configuration::I18nValues>;

ssize_t (*os_read)(int fd, void *dest, size_t count) = read;
ssize_t (*os_write)(int fd, const void *buf, size_t count) = write;

static void show_version_info(void)
{
    printf("%s\n"
           "Revision %s%s\n"
           "         %s+%d, %s\n",
           PACKAGE_STRING,
           VCS_FULL_HASH, VCS_WC_MODIFIED ? " (tainted)" : "",
           VCS_TAG, VCS_TICK, VCS_DATE);
}

static void log_version_info(void)
{
    msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "Rev %s%s, %s+%d, %s",
              VCS_FULL_HASH, VCS_WC_MODIFIED ? " (tainted)" : "",
              VCS_TAG, VCS_TICK, VCS_DATE);
}

static bool try_reopen_fd(int *fd, const char *devname, const char *errorname)
{
    if(fifo_reopen(fd, devname, false))
        return true;

    msg_error(EPIPE, LOG_EMERG,
              "Failed reopening %s connection, unable to recover. "
              "Terminating", errorname);

    return false;
}

static DCP::Transaction::Result read_transaction_result(int fd)
{
    uint8_t result[3];
    size_t dummy = 0;

    if(fifo_try_read_to_buffer(result, sizeof(result), &dummy, fd) != 1)
        return DCP::Transaction::IO_ERROR;

    static const char ok_result[] = "OK\n";
    static const char error_result[] = "FF\n";

    if(memcmp(result, ok_result, sizeof(result)) == 0)
        return DCP::Transaction::OK;
    else if(memcmp(result, error_result, sizeof(result)) == 0)
        return DCP::Transaction::FAILED;

    msg_error(EINVAL, LOG_ERR,
              "Received bad data from DCPD: 0x%02x 0x%02x 0x%02x",
              result[0], result[1], result[2]);

    return DCP::Transaction::INVALID_ANSWER;
}

static bool watch_in_fd(struct dcp_fifo_dispatch_data_t *dispatch_data);

static gboolean dcp_fifo_in_dispatch(int fd, GIOCondition condition,
                                     gpointer user_data)
{
    struct dcp_fifo_dispatch_data_t *const data =
        static_cast<struct dcp_fifo_dispatch_data_t *>(user_data);

    log_assert(data != nullptr);

    if(data->files->dcp_fifo.in_fd < 0)
        return G_SOURCE_REMOVE;

    log_assert(fd == data->files->dcp_fifo.in_fd);

    gboolean return_value = G_SOURCE_CONTINUE;

    if((condition & G_IO_IN) != 0)
        data->vm->serialization_result(read_transaction_result(fd));

    if((condition & G_IO_HUP) != 0)
    {
        msg_error(EPIPE, LOG_ERR, "DCP daemon died, need to reopen");

        if(try_reopen_fd(&data->files->dcp_fifo.in_fd,
                         data->files->dcp_fifo_in_name, "DCP"))
        {
            if(data->files->dcp_fifo.in_fd != fd)
            {
                if(!watch_in_fd(data))
                    raise(SIGTERM);

                return_value = G_SOURCE_REMOVE;
            }
        }
        else
            raise(SIGTERM);
    }

    if((condition & ~(G_IO_IN | G_IO_HUP)) != 0)
    {
        msg_error(EINVAL, LOG_WARNING,
                  "Unexpected poll() events on DCP fifo %d: %04x",
                  fd, condition);
    }

    return return_value;
}

static bool watch_in_fd(struct dcp_fifo_dispatch_data_t *dispatch_data)
{
    dispatch_data->files->dcp_fifo_in_event_source_id =
        g_unix_fd_add(dispatch_data->files->dcp_fifo.in_fd,
                      GIOCondition(G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL),
                      dcp_fifo_in_dispatch, dispatch_data);

    if(dispatch_data->files->dcp_fifo_in_event_source_id == 0)
        msg_error(ENOMEM, LOG_EMERG, "Failed adding DCP in-fd event to main loop");

    return dispatch_data->files->dcp_fifo_in_event_source_id != 0;
}

static std::chrono::milliseconds
transaction_timeout_exceeded(ViewManager::VMIface *const vm)
{
    msg_error(ETIMEDOUT, LOG_CRIT, "DCPD answer timeout exceeded");
    log_assert(vm != nullptr);

    vm->serialization_result(DCP::Transaction::TIMEOUT);

    return std::chrono::milliseconds::min();
}

static void timeout_config(bool start_timeout_timer,
                           struct dcp_fifo_dispatch_data_t *dispatch_data)
{
    if(start_timeout_timer)
        dispatch_data->timeout.start(std::chrono::seconds(4),
            std::bind(transaction_timeout_exceeded, dispatch_data->vm));
    else
        dispatch_data->timeout.stop();
}

/*!
 * Set up logging, daemonize.
 */
static int setup(const struct parameters *parameters,
                 struct dcp_fifo_dispatch_data_t *dispatch_data,
                 GMainLoop **loop)
{
    msg_enable_syslog(!parameters->run_in_foreground);
    msg_enable_glib_message_redirection();
    msg_set_verbose_level(parameters->verbose_level);

    if(!parameters->run_in_foreground)
        openlog("drcpd", LOG_PID, LOG_DAEMON);

    if(!parameters->run_in_foreground)
    {
        if(daemon(0, 0) < 0)
        {
            msg_error(errno, LOG_EMERG, "Failed to run as daemon");
            return -1;
        }
    }

    log_version_info();

    msg_vinfo(MESSAGE_LEVEL_DEBUG, "Attempting to open named pipes");

    struct files_t *const files = dispatch_data->files;

    files->dcp_fifo.out_fd = fifo_open(files->dcp_fifo_out_name, true);
    if(files->dcp_fifo.out_fd < 0)
        goto error_dcp_fifo_out;

    files->dcp_fifo.in_fd = fifo_open(files->dcp_fifo_in_name, false);
    if(files->dcp_fifo.in_fd < 0)
        goto error_dcp_fifo_in;

    *loop = g_main_loop_new(NULL, FALSE);
    if(*loop == NULL)
    {
        msg_error(ENOMEM, LOG_EMERG, "Failed creating GLib main loop");
        goto error_main_loop_new;
    }

    if(!watch_in_fd(dispatch_data))
        goto error_unix_fd_add;

    return 0;

error_unix_fd_add:
    g_main_loop_unref(*loop);

error_main_loop_new:
    fifo_close(&files->dcp_fifo.in_fd);

error_dcp_fifo_in:
    fifo_close(&files->dcp_fifo.out_fd);

error_dcp_fifo_out:
    *loop = NULL;
    return -1;
}

static void shutdown(ViewManager::VMIface &view_manager, struct files_t *files)
{
    view_manager.shutdown();

    fifo_close(&files->dcp_fifo.in_fd);
    fifo_close(&files->dcp_fifo.out_fd);
}

static void usage(const char *program_name)
{
    std::cout <<
        "Usage: " << program_name << " [options]\n"
        "\n"
        "Options:\n"
        "  --help         Show this help.\n"
        "  --version      Print version information to stdout.\n"
        "  --verbose lvl  Set verbosity level to given level.\n"
        "  --quiet        Short for \"--verbose quite\".\n"
        "  --fg           Run in foreground, don't run as daemon.\n"
        "  --idcp name    Name of the named pipe the DCP daemon writes to.\n"
        "  --odcp name    Name of the named pipe the DCP daemon reads from.\n"
        "  --session-dbus Connect to session D-Bus.\n"
        "  --system-dbus  Connect to system D-Bus.\n"
        ;
}

static bool check_argument(int argc, char *argv[], int &i)
{
    if(i + 1 >= argc)
    {
        std::cerr << "Option " << argv[i] << " requires an argument.\n";
        return false;
    }

    ++i;

    return true;
}

static int process_command_line(int argc, char *argv[],
                                struct parameters *parameters,
                                struct files_t *files)
{
    parameters->verbose_level = MESSAGE_LEVEL_NORMAL;
    parameters->run_in_foreground = false;
    parameters->connect_to_session_dbus = true;

    files->dcp_fifo_out_name = "/tmp/drcpd_to_dcpd";
    files->dcp_fifo_in_name = "/tmp/dcpd_to_drcpd";

    for(int i = 1; i < argc; ++i)
    {
        if(strcmp(argv[i], "--help") == 0)
            return 1;
        else if(strcmp(argv[i], "--version") == 0)
            return 2;
        else if(strcmp(argv[i], "--fg") == 0)
            parameters->run_in_foreground = true;
        else if(strcmp(argv[i], "--verbose") == 0)
        {
            if(!check_argument(argc, argv, i))
                return -1;

            parameters->verbose_level = msg_verbose_level_name_to_level(argv[i]);

            if(parameters->verbose_level == MESSAGE_LEVEL_IMPOSSIBLE)
            {
                fprintf(stderr,
                        "Invalid verbosity \"%s\". "
                        "Valid verbosity levels are:\n", argv[i]);

                const char *const *names = msg_get_verbose_level_names();

                for(const char *name = *names; name != NULL; name = *++names)
                    fprintf(stderr, "    %s\n", name);

                return -1;
            }
        }
        else if(strcmp(argv[i], "--quiet") == 0)
            parameters->verbose_level = MESSAGE_LEVEL_QUIET;
        else if(strcmp(argv[i], "--idcp") == 0)
        {
            if(!check_argument(argc, argv, i))
                return -1;
            files->dcp_fifo_in_name = argv[i];
        }
        else if(strcmp(argv[i], "--odcp") == 0)
        {
            if(!check_argument(argc, argv, i))
                return -1;
            files->dcp_fifo_out_name = argv[i];
        }
        else if(strcmp(argv[i], "--session-dbus") == 0)
            parameters->connect_to_session_dbus = true;
        else if(strcmp(argv[i], "--system-dbus") == 0)
            parameters->connect_to_session_dbus = false;
        else
        {
            std::cerr << "Unknown option \"" << argv[i]
                      << "\". Please try --help.\n";
            return -1;
        }
    }

    return 0;
}

static gboolean do_call_in_main_context(gpointer user_data)
{
    auto *fn = static_cast<std::function<void()> *>(user_data);
    log_assert(fn != nullptr);

    try
    {
        (*fn)();
    }
    catch(...)
    {
        /* doesn't really matter, but don't mess up the GLib path */
    }

    return G_SOURCE_REMOVE;
}

static void do_call_in_main_context_dtor(gpointer user_data)
{
    auto *fn = static_cast<std::function<void()> *>(user_data);
    delete fn;
}

/*!
 * Call given function in main context.
 *
 * For functions that must not be called from threads other than the main
 * thread.
 *
 * \param fn_object
 *     Dynamically allocated function object that is called from the main
 *     thread's main loop. If \c nullptr, then an out-of-memory error message
 *     is emitted (the caller may therefore conveniently pass the result of
 *     \c new directly to this function). The object is freed via \c delete
 *     after the function object has been called.
 *
 * \param allow_direct_call
 *     If this is true, then the function is called directly in case the
 *     current context is the main context. Note that this may lead to
 *     deadlocks.
 */
static void call_in_main_context(std::function<void()> *fn_object,
                                 bool allow_direct_call)
{
    if(fn_object == nullptr)
    {
        msg_out_of_memory("function object");
        return;
    }

    if(allow_direct_call)
        g_main_context_invoke_full(NULL, G_PRIORITY_DEFAULT,
                                   do_call_in_main_context, fn_object,
                                   do_call_in_main_context_dtor);
    else
    {
        GSource *const source = g_idle_source_new();

        g_source_set_priority(source, G_PRIORITY_DEFAULT);
        g_source_set_callback(source, do_call_in_main_context, fn_object,
                              do_call_in_main_context_dtor);
        g_source_attach(source, g_main_context_default());
        g_source_unref(source);
    }
}

static void defer_ui_event_processing(struct ui_events_processing_data_t *data)
{
    log_assert(data != nullptr);

    auto *fn_object =
        new std::function<void()>(std::bind(&ViewManager::Manager::process_pending_events,
                                            data->vm));

    call_in_main_context(fn_object, false);
}

/*!
 * Process DCP transaction queue in the main loop of the main context.
 *
 * To avoid the grief of adding proper locking to all view implementations in
 * order to make them thread-safe and reentrant, we resort to asynchronous
 * function calls via GLib mechanisms. The views may continue to assume they
 * are being used in a single-threaded process.
 */
static void defer_dcp_transfer(DCP::Queue *queue)
{
    log_assert(queue != nullptr);

    auto *fn_object =
        new std::function<void()>(std::bind(&DCP::Queue::process_pending_transactions, queue));

    call_in_main_context(fn_object, false);
}

static void language_changed(const I18nConfigMgr &config_manager,
                             ViewManager::VMIface &view_manager,
                             bool is_first_call = false)
{
    const Configuration::I18nValues &values(config_manager.values());
    const char *lang_id;
    std::string temp;

    if(values.language_code_.empty() || values.country_code_.empty())
        lang_id = "en_US.UTF-8";
    else
    {
        temp = values.language_code_ + '_' + values.country_code_ + ".UTF-8";
        lang_id = temp.c_str();
    }

    msg_info("Setting system language \"%s\"", lang_id);

    if(is_first_call)
        i18n_init(lang_id);
    else
        i18n_switch_language(lang_id);

    view_manager.language_settings_changed_notification();
}

static void connect_everything(ViewManager::Manager &views,
                               DBus::SignalData &dbus_data,
                               const Configuration::DrcpdValues &config,
                               I18nConfigMgr &i18n_config_manager)
{
    static ViewErrorSink::View error_sink(N_("Error"), &views);
    static ViewInactive::View inactive("Inactive", &views);
    static ViewFileBrowser::View fs(ViewNames::BROWSER_FILESYSTEM,
                                    N_("USB mass storage devices"), 1,
                                    views.NUMBER_OF_LINES_ON_DISPLAY,
                                    DBUS_LISTBROKER_ID_FILESYSTEM,
                                    Playlist::CrawlerIface::RecursiveMode::DEPTH_FIRST,
                                    Playlist::CrawlerIface::ShuffleMode::FORWARD,
                                    "strbo.usb",
                                    &views);
    static ViewFileBrowser::AirableView tunein(ViewNames::BROWSER_INETRADIO,
                                               N_("Airable internet radio"), 3,
                                               views.NUMBER_OF_LINES_ON_DISPLAY,
                                               DBUS_LISTBROKER_ID_TUNEIN,
                                               Playlist::CrawlerIface::RecursiveMode::DEPTH_FIRST,
                                               Playlist::CrawlerIface::ShuffleMode::FORWARD,
                                               &views);
    static ViewFileBrowser::View upnp(ViewNames::BROWSER_UPNP,
                                      N_("UPnP media servers"), 4,
                                      views.NUMBER_OF_LINES_ON_DISPLAY,
                                      DBUS_LISTBROKER_ID_UPNP,
                                      Playlist::CrawlerIface::RecursiveMode::DEPTH_FIRST,
                                      Playlist::CrawlerIface::ShuffleMode::FORWARD,
                                      "strbo.upnpcm",
                                      &views);
    static ViewSourceApp::View app("TA Control", &views);
    static ViewSourceRoon::View roon("Roon Ready", &views);
    static ViewPlay::View play(N_("Stream information"),
                               views.NUMBER_OF_LINES_ON_DISPLAY,
                               config.maximum_bitrate_,
                               &views);
    static ViewSearch::View search(N_("Search parameters"),
                                   views.NUMBER_OF_LINES_ON_DISPLAY, &views);

    if(!error_sink.init())
        return;

    if(!fs.init())
        return;

    if(!tunein.init())
        return;

    if(!upnp.init())
        return;

    if(!app.init())
        return;

    if(!roon.init())
        return;

    if(!play.init())
        return;

    if(!search.init())
        return;

    views.add_view(&error_sink);
    views.add_view(&inactive);
    views.add_view(&fs);
    views.add_view(&tunein);
    views.add_view(&upnp);
    views.add_view(&app);
    views.add_view(&roon);
    views.add_view(&play);
    views.add_view(&search);

    if(!views.invoke_late_init_functions())
        return;

    Busy::init(std::bind(&ViewManager::Manager::busy_state_notification,
                         &views, std::placeholders::_1));

    i18n_config_manager.set_updated_notification_callback(
        [&i18n_config_manager, &views]
        (const char *origin,
         const std::array<bool, Configuration::I18nValues::NUMBER_OF_KEYS> &changed)
        {
            language_changed(i18n_config_manager, views);
        });

    views.sync_activate_view_by_name(ViewNames::INACTIVE, true);

    inactive.enable_deselect_notifications();
}

static gboolean signal_handler(gpointer user_data)
{
    g_main_loop_quit(static_cast<GMainLoop *>(user_data));
    return G_SOURCE_REMOVE;
}

int main(int argc, char *argv[])
{
    static struct parameters parameters;
    static struct files_t files;

    int ret = process_command_line(argc, argv, &parameters, &files);

    if(ret == -1)
        return EXIT_FAILURE;
    else if(ret == 1)
    {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }
    else if(ret == 2)
    {
        show_version_info();
        return EXIT_SUCCESS;
    }

    static GMainLoop *loop = NULL;

    static struct ui_events_processing_data_t ui_events_processing_data;
    static struct dcp_fifo_dispatch_data_t dcp_dispatch_data(files);

    if(setup(&parameters, &dcp_dispatch_data, &loop) < 0)
        return EXIT_FAILURE;

    static const char configuration_file_name[] = "/var/local/etc/drcpd.ini";
    static const char resume_config_file_name[] = "/var/local/etc/resume.ini";

    static const Configuration::DrcpdValues default_drcpd_settings(0);
    ViewManager::Manager::ConfigMgr
        drcpd_config_manager(configuration_file_name, default_drcpd_settings);
    drcpd_config_manager.load();

    static const Configuration::I18nValues
        default_i18n_settings(std::move(std::string("en")),
                              std::move(std::string("US")));
    I18nConfigMgr i18n_config_manager(configuration_file_name, default_i18n_settings);
    i18n_config_manager.load();

    static UI::EventQueue ui_event_queue(
        std::bind(defer_ui_event_processing, &ui_events_processing_data));
    static DCP::Queue dcp_transaction_queue(
        std::bind(timeout_config, std::placeholders::_1, &dcp_dispatch_data),
        std::bind(defer_dcp_transfer, &dcp_transaction_queue));
    static FdStreambuf fd_sbuf(files.dcp_fifo.out_fd);
    static std::ostream fd_out(&fd_sbuf);
    static ViewManager::Manager view_manager(ui_event_queue,
                                             dcp_transaction_queue,
                                             drcpd_config_manager);

    static DBus::SignalData dbus_signal_data(view_manager,
                                             drcpd_config_manager,
                                             i18n_config_manager);

    view_manager.set_output_stream(fd_out);
    view_manager.set_debug_stream(std::cout);
    view_manager.set_resume_playback_configuration_file(resume_config_file_name);

    language_changed(i18n_config_manager, view_manager, true);
    ui_events_processing_data.vm = &view_manager;
    dcp_dispatch_data.vm = &view_manager;

    if(dbus_setup(parameters.connect_to_session_dbus, &dbus_signal_data) < 0)
        return EXIT_FAILURE;

    g_unix_signal_add(SIGINT, signal_handler, loop);
    g_unix_signal_add(SIGTERM, signal_handler, loop);

    connect_everything(view_manager, dbus_signal_data,
                       drcpd_config_manager.values(), i18n_config_manager);

    g_main_loop_run(loop);

    msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "Shutting down");

    fd_sbuf.set_fd(-1);
    shutdown(view_manager, &files);
    dbus_shutdown();

    return EXIT_SUCCESS;
}

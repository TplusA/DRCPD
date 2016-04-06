/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
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

#include "i18n.h"
#include "view_filebrowser.hh"
#include "view_config.hh"
#include "view_manager.hh"
#include "view_play.hh"
#include "view_search.hh"
#include "player.hh"
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

struct dcp_fifo_dispatch_data_t
{
    files_t *files;
    ViewManager::VMIface *vm;
    Timeout::Timer timeout;
};

struct parameters
{
    bool run_in_foreground;
    bool connect_to_session_dbus;
};

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
    msg_info("Rev %s%s, %s+%d, %s",
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
        dispatch_data->timeout.start(std::chrono::seconds(2),
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

    msg_info("Attempting to open named pipes");

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

static void shutdown(struct files_t *files)
{
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
        "  --fg           Run in foreground, don't run as daemon.\n"
        "  --idcp name    Name of the named pipe the DCP daemon writes to.\n"
        "  --odcp name    Name of the named pipe the DCP daemon reads from.\n"
        "  --session-dbus Connect to session D-Bus.\n"
        "  --system-dbus  Connect to system D-Bus."
        << std::endl;
}

static bool check_argument(int argc, char *argv[], int &i)
{
    if(i + 1 >= argc)
    {
        std::cerr << "Option " << argv[i] << " requires an argument." << std::endl;
        return false;
    }

    ++i;

    return true;
}

static int process_command_line(int argc, char *argv[],
                                struct parameters *parameters,
                                struct files_t *files)
{
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
                      << "\". Please try --help." << std::endl;
            return -1;
        }
    }

    return 0;
}

gboolean do_call_in_main_context(gpointer user_data)
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

    delete fn;

    return G_SOURCE_REMOVE;
}

/*!
 * Call given function in main context.
 *
 * To avoid the grief of adding proper locking to all view implementations in
 * order to make them thread-safe, we resort to asynchronous function calls via
 * GLib mechanisms. The views may continue to assume they are being used in a
 * single-threaded process.
 */
static void call_in_main_context(const std::function<void(bool)> &fn, bool arg)
{
    auto *fn_object = new std::function<void()>(std::bind(fn, arg));

    if(fn_object != nullptr)
        g_main_context_invoke(NULL, do_call_in_main_context, fn_object);
    else
        msg_out_of_memory("function object");
}

static void connect_everything(ViewManager::Manager &views,
                               Playback::Player &player,
                               DBusSignalData &dbus_data)
{
    static const unsigned int number_of_lines_on_display = 3;

    static ViewConfig::View cfg(N_("Configuration"), number_of_lines_on_display);
    static ViewFileBrowser::View fs(ViewNames::BROWSER_FILESYSTEM,
                                    N_("USB devices"), 1,
                                    number_of_lines_on_display,
                                    DBUS_LISTBROKER_ID_FILESYSTEM,
                                    player, Playback::Mode::LINEAR,
                                    &views);
    static ViewFileBrowser::View tunein(ViewNames::BROWSER_INETRADIO,
                                        N_("Airable internet radio"), 3,
                                        number_of_lines_on_display,
                                        DBUS_LISTBROKER_ID_TUNEIN,
                                        player, Playback::Mode::LINEAR,
                                        &views);
    static ViewFileBrowser::View upnp(ViewNames::BROWSER_UPNP,
                                      N_("UPnP media servers"), 4,
                                      number_of_lines_on_display,
                                      DBUS_LISTBROKER_ID_UPNP,
                                      player, Playback::Mode::LINEAR,
                                      &views);
    static ViewPlay::View play(N_("Stream information"),
                               number_of_lines_on_display, player, &views);
    static ViewSearch::View search(N_("Search parameters"),
                                   number_of_lines_on_display, &views);

    if(!cfg.init())
        return;

    if(!fs.init())
        return;

    if(!tunein.init())
        return;

    if(!upnp.init())
        return;

    if(!play.init())
        return;

    if(!search.init())
        return;

    views.add_view(&cfg);
    views.add_view(&fs);
    views.add_view(&tunein);
    views.add_view(&upnp);
    views.add_view(&play);
    views.add_view(&search);

    if(!views.invoke_late_init_functions())
        return;

    dbus_data.play_view_ = views.get_view_by_name(ViewNames::PLAYER);

    Busy::init(std::bind(call_in_main_context,
                         std::function<void(bool)>(
                             std::bind(&ViewManager::Manager::busy_state_notification,
                                       &views, std::placeholders::_1)),
                         std::placeholders::_1));

    views.activate_view_by_name(ViewNames::BROWSER_UPNP);
}

static gboolean signal_handler(gpointer user_data)
{
    g_main_loop_quit(static_cast<GMainLoop *>(user_data));
    return G_SOURCE_REMOVE;
}

int main(int argc, char *argv[])
{
    i18n_init("en_US.UTF-8");

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

    static struct dcp_fifo_dispatch_data_t dcp_dispatch_data =
    {
        .files = &files,
    };

    if(setup(&parameters, &dcp_dispatch_data, &loop) < 0)
        return EXIT_FAILURE;

    static DCP::Queue dcp_transaction_queue(
        std::bind(timeout_config, std::placeholders::_1, &dcp_dispatch_data));
    static FdStreambuf fd_sbuf(files.dcp_fifo.out_fd);
    static std::ostream fd_out(&fd_sbuf);
    static ViewManager::Manager view_manager(dcp_transaction_queue);

    static Playback::Player player_singleton(ViewPlay::meta_data_reformatters);
    static DBusSignalData dbus_signal_data(view_manager, player_singleton, player_singleton);

    view_manager.set_output_stream(fd_out);
    view_manager.set_debug_stream(std::cout);

    dcp_dispatch_data.vm = &view_manager;

    player_singleton.start();

    if(dbus_setup(loop, parameters.connect_to_session_dbus, &dbus_signal_data) < 0)
        return EXIT_FAILURE;

    g_unix_signal_add(SIGINT, signal_handler, loop);
    g_unix_signal_add(SIGTERM, signal_handler, loop);

    connect_everything(view_manager, player_singleton, dbus_signal_data);

    g_main_loop_run(loop);

    msg_info("Shutting down");

    fd_sbuf.set_fd(-1);
    shutdown(&files);
    dbus_shutdown(loop);

    player_singleton.shutdown();

    return EXIT_SUCCESS;
}

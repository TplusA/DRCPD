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
#include "view_signals_glib.hh"
#include "player.hh"
#include "dbus_iface.h"
#include "dbus_handlers.hh"
#include "messages.h"
#include "fdstreambuf.hh"
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
    ViewManagerIface *vm;
    guint timeout_event_source_id;
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

static DcpTransaction::Result read_transaction_result(int fd)
{
    uint8_t result[3];
    size_t dummy = 0;

    if(fifo_try_read_to_buffer(result, sizeof(result), &dummy, fd) != 1)
        return DcpTransaction::IO_ERROR;

    static const char ok_result[] = "OK\n";
    static const char error_result[] = "FF\n";

    if(memcmp(result, ok_result, sizeof(result)) == 0)
        return DcpTransaction::OK;
    else if(memcmp(result, error_result, sizeof(result)) == 0)
        return DcpTransaction::FAILED;

    msg_error(EINVAL, LOG_ERR,
              "Received bad data from DCPD: 0x%02x 0x%02x 0x%02x",
              result[0], result[1], result[2]);

    return DcpTransaction::INVALID_ANSWER;
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

static gboolean transaction_timeout_exceeded(gpointer user_data)
{
    msg_error(ETIMEDOUT, LOG_CRIT, "DCPD answer timeout exceeded");

    struct dcp_fifo_dispatch_data_t *dispatch_data =
        static_cast<struct dcp_fifo_dispatch_data_t *>(user_data);
    log_assert(dispatch_data != NULL);

    dispatch_data->timeout_event_source_id = 0;
    dispatch_data->vm->serialization_result(DcpTransaction::TIMEOUT);

    return G_SOURCE_REMOVE;
}

static void add_timeout(dcp_fifo_dispatch_data_t *dispatch_data,
                        guint timeout_ms)
{
    log_assert(dispatch_data->timeout_event_source_id == 0);

    GSource *src = g_timeout_source_new(timeout_ms);

    if(src == NULL)
        msg_error(ENOMEM, LOG_EMERG, "Failed allocating timeout event source");
    else
    {
        g_source_set_callback(src, transaction_timeout_exceeded,
                              dispatch_data, NULL);
        dispatch_data->timeout_event_source_id = g_source_attach(src, NULL);
    }
}

static void dcp_transaction_observer(DcpTransaction::state state,
                                     void *user_data)
{
    struct dcp_fifo_dispatch_data_t *dispatch_data =
        static_cast<struct dcp_fifo_dispatch_data_t *>(user_data);

    switch(state)
    {
      case DcpTransaction::IDLE:
        if(dispatch_data->timeout_event_source_id != 0)
        {
            g_source_remove(dispatch_data->timeout_event_source_id);
            dispatch_data->timeout_event_source_id = 0;
        }
        break;

      case DcpTransaction::WAIT_FOR_COMMIT:
        /* we are not going to consider this case because this state is left by
         * our own internal actions pretty quickly---we assume here that the
         * views commit their stuff */
        return;

      case DcpTransaction::WAIT_FOR_ANSWER:
        add_timeout(dispatch_data, 2U * 1000U);
        break;
    }
}

/*!
 * Set up logging, daemonize.
 */
static int setup(const struct parameters *parameters,
                 struct dcp_fifo_dispatch_data_t *dispatch_data,
                 GMainLoop **loop)
{
    msg_enable_syslog(!parameters->run_in_foreground);

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

static void connect_everything(ViewManager &views, ViewSignalsIface *view_signals,
                               Playback::Player &player)
{
    static const unsigned int number_of_lines_on_display = 3;

    static ViewConfig::View cfg(N_("Configuration"), number_of_lines_on_display, view_signals);
    static ViewFileBrowser::View fs(ViewNames::BROWSER_FILESYSTEM,
                                    N_("Local file system"), 1,
                                    number_of_lines_on_display,
                                    DBUS_LISTBROKER_ID_FILESYSTEM,
                                    player, Playback::Mode::LINEAR,
                                    view_signals);
    static ViewFileBrowser::View tunein(ViewNames::BROWSER_INETRADIO,
                                        N_("TuneIn internet radio"), 3,
                                        number_of_lines_on_display,
                                        DBUS_LISTBROKER_ID_TUNEIN,
                                        player, Playback::Mode::SINGLE_TRACK,
                                        view_signals);
    static ViewFileBrowser::View upnp(ViewNames::BROWSER_UPNP,
                                      N_("UPnP media servers"), 4,
                                      number_of_lines_on_display,
                                      DBUS_LISTBROKER_ID_UPNP,
                                      player, Playback::Mode::LINEAR,
                                      view_signals);
    static ViewPlay::View play(N_("Stream information"),
                               number_of_lines_on_display, player,
                               view_signals);

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

    views.add_view(&cfg);
    views.add_view(&fs);
    views.add_view(&tunein);
    views.add_view(&upnp);
    views.add_view(&play);

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

    static const std::function<void(DcpTransaction::state)> transaction_observer =
        std::bind(dcp_transaction_observer, std::placeholders::_1, &dcp_dispatch_data);
    static DcpTransaction dcp_transaction(transaction_observer);
    static FdStreambuf fd_sbuf(files.dcp_fifo.out_fd);
    static std::ostream fd_out(&fd_sbuf);
    static ViewManager view_manager(dcp_transaction);

    static Playback::Player player_singleton(ViewPlay::meta_data_reformatters);
    static DBusSignalData dbus_signal_data(view_manager, player_singleton, player_singleton);

    view_manager.set_output_stream(fd_out);
    view_manager.set_debug_stream(std::cout);

    dcp_dispatch_data.vm = &view_manager;
    dcp_dispatch_data.timeout_event_source_id = 0;

    if(dbus_setup(loop, parameters.connect_to_session_dbus, &dbus_signal_data) < 0)
        return EXIT_FAILURE;

    g_unix_signal_add(SIGINT, signal_handler, loop);
    g_unix_signal_add(SIGTERM, signal_handler, loop);

    ViewSignalsGLib view_signals(view_manager);
    view_signals.connect_to_main_loop(loop);

    connect_everything(view_manager, &view_signals, player_singleton);

    g_main_loop_run(loop);

    msg_info("Shutting down");

    fd_sbuf.set_fd(-1);
    shutdown(&files);
    dbus_shutdown(loop);

    return EXIT_SUCCESS;
}

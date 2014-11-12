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
#include "dbus_iface.h"
#include "messages.h"
#include "fdstreambuf.hh"
#include "os.h"


struct files_t
{
    struct fifo_pair dcp_fifo;
    const char *dcp_fifo_in_name;
    const char *dcp_fifo_out_name;
};

struct parameters
{
    bool run_in_foreground;
};

ssize_t (*os_read)(int fd, void *dest, size_t count) = read;
ssize_t (*os_write)(int fd, const void *buf, size_t count) = write;

/*!
 * Set up logging, daemonize.
 */
static int setup(const struct parameters *parameters, struct files_t *files,
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

    msg_info("Attempting to open named pipes");

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

    return 0;

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
        "  --fg           Run in foreground, don't run as daemon.\n"
        "  --idcp name    Name of the named pipe the DCP daemon writes to.\n"
        "  --odcp name    Name of the named pipe the DCP daemon reads from."
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

    files->dcp_fifo_out_name = "/tmp/drcpd_to_dcpd";
    files->dcp_fifo_in_name = "/tmp/dcpd_to_drcpd";

    for(int i = 1; i < argc; ++i)
    {
        if(strcmp(argv[i], "--help") == 0)
            return 1;
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
        else
        {
            std::cerr << "Unknown option \"" << argv[i]
                      << "\". Please try --help." << std::endl;
            return -1;
        }
    }

    return 0;
}

static void testing(ViewManager &views)
{
    static const unsigned int number_of_lines_on_display = 3;

    static ViewConfig::View cfg(number_of_lines_on_display);
    static ViewFileBrowser::View fs("Filesystem", number_of_lines_on_display,
                                    DBUS_LISTBROKER_ID_FILESYSTEM);
    static ViewFileBrowser::View tunein("TuneIn", number_of_lines_on_display,
                                        DBUS_LISTBROKER_ID_TUNEIN);

    if(!cfg.init())
        return;

    if(!fs.init())
        return;

    if(!tunein.init())
        return;

    views.add_view(&cfg);
    views.add_view(&fs);
    views.add_view(&tunein);

    views.activate_view_by_name("TuneIn");
}

static gboolean signal_handler(gpointer user_data)
{
    g_main_loop_quit(static_cast<GMainLoop *>(user_data));
    return G_SOURCE_REMOVE;
}

int main(int argc, char *argv[])
{
    i18n_init();

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

    static GMainLoop *loop = NULL;

    if(setup(&parameters, &files, &loop) < 0)
        return EXIT_FAILURE;

    static DcpTransaction dcp_transaction;
    static FdStreambuf fd_sbuf(files.dcp_fifo.out_fd);
    static std::ostream fd_out(&fd_sbuf);
    static ViewManager view_manager(dcp_transaction);

    view_manager.set_output_stream(fd_out);
    view_manager.set_debug_stream(std::cout);

    if(dbus_setup(loop, true, static_cast<ViewManagerIface *>(&view_manager)) < 0)
        return EXIT_FAILURE;

    g_unix_signal_add(SIGINT, signal_handler, loop);
    g_unix_signal_add(SIGTERM, signal_handler, loop);

    testing(view_manager);

    g_main_loop_run(loop);

    msg_info("Shutting down");

    fd_sbuf.set_fd(-1);
    shutdown(&files);
    dbus_shutdown(loop);

    return EXIT_SUCCESS;
}

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cstring>
#include <iostream>

#include <glib-object.h>
#include <glib-unix.h>

#include "i18n.h"
#include "view_manager.hh"
#include "dbus_iface.h"
#include "messages.h"

static struct
{
    GMainLoop *loop;
}
globals;

struct parameters
{
    bool run_in_foreground;
};

/*!
 * Set up logging, daemonize.
 */
static int setup(const struct parameters *parameters)
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

    return 0;
}

static void usage(const char *program_name)
{
    std::cout <<
        "Usage: " << program_name << " [options]\n"
        "\n"
        "Options:\n"
        "  --help         Show this help.\n"
        "  --fg           Run in foreground, don't run as daemon."
        << std::endl;
}

static int process_command_line(int argc, char *argv[],
                                struct parameters *parameters)
{
    parameters->run_in_foreground = false;

    for(int i = 1; i < argc; ++i)
    {
        if(strcmp(argv[i], "--help") == 0)
            return 1;
        else if(strcmp(argv[i], "--fg") == 0)
            parameters->run_in_foreground = true;
        else
        {
            std::cerr << "Unknown option \"" << argv[i]
                      << "\". Please try --help." << std::endl;
            return -1;
        }
    }

    return 0;
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

    int ret = process_command_line(argc, argv, &parameters);

    if(ret == -1)
        return EXIT_FAILURE;
    else if(ret == 1)
    {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if(setup(&parameters) < 0)
        return EXIT_FAILURE;

    globals.loop = g_main_loop_new(NULL, FALSE);
    if(globals.loop == NULL)
    {
        msg_error(ENOMEM, LOG_EMERG, "Failed creating GLib main loop");
        return -1;
    }

    static ViewManager view_manager;

    if(dbus_setup(globals.loop, true, static_cast<ViewManagerIface *>(&view_manager)) < 0)
        return EXIT_FAILURE;

    g_unix_signal_add(SIGINT, signal_handler, globals.loop);
    g_unix_signal_add(SIGTERM, signal_handler, globals.loop);

    g_main_loop_run(globals.loop);

    msg_info("Shutting down");

    dbus_shutdown(globals.loop);

    return EXIT_SUCCESS;
}

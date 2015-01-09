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
};

ssize_t (*os_read)(int fd, void *dest, size_t count) = read;
ssize_t (*os_write)(int fd, const void *buf, size_t count) = write;
void (*os_abort)(void) = abort;

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

    assert(data != nullptr);
    assert(fd == data->files->dcp_fifo.in_fd);

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
    assert(dispatch_data != NULL);

    dispatch_data->timeout_event_source_id = 0;
    dispatch_data->vm->serialization_result(DcpTransaction::TIMEOUT);

    return G_SOURCE_REMOVE;
}

static void add_timeout(dcp_fifo_dispatch_data_t *dispatch_data,
                        guint timeout_ms)
{
    assert(dispatch_data->timeout_event_source_id == 0);

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

    static ViewConfig::View cfg(N_("Configuration"), number_of_lines_on_display);
    static ViewFileBrowser::View fs("Filesystem", N_("Local file system"), 1,
                                    number_of_lines_on_display,
                                    DBUS_LISTBROKER_ID_FILESYSTEM);
    static ViewFileBrowser::View tunein("TuneIn", N_("TuneIn internet radio"), 3,
                                        number_of_lines_on_display,
                                        DBUS_LISTBROKER_ID_TUNEIN);
    static ViewFileBrowser::View upnp("UPnP", N_("UPnP media servers"), 4,
                                      number_of_lines_on_display,
                                      DBUS_LISTBROKER_ID_UPNP);

    if(!cfg.init())
        return;

    if(!fs.init())
        return;

    if(!tunein.init())
        return;

    if(!upnp.init())
        return;

    views.add_view(&cfg);
    views.add_view(&fs);
    views.add_view(&tunein);
    views.add_view(&upnp);

    views.activate_view_by_name("Filesystem");
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

    view_manager.set_output_stream(fd_out);
    view_manager.set_debug_stream(std::cout);

    dcp_dispatch_data.vm = &view_manager;
    dcp_dispatch_data.timeout_event_source_id = 0;

    if(dbus_setup(loop, true, &view_manager) < 0)
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

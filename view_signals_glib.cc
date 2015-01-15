#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "view_signals_glib.hh"
#include "messages.h"

void ViewSignalsGLib::remove_from_main_loop()
{
    if(source_id_ == 0)
        return;

    g_source_remove(source_id_);
    source_id_ = 0;
}

bool ViewSignalsGLib::check() const
{
    return (signal_ != NONE) ? TRUE : FALSE;
}

void ViewSignalsGLib::dispatch()
{
    switch(signal_)
    {
      case NONE:
        BUG("Attempted to dispatch empty signal");
        break;

      case DISPLAY_UPDATE_REQUEST:
        vm_.update_view_if_active(view_);
        break;

      case HIDE_VIEW_REQUEST:
        vm_.hide_view_if_active(view_);
        break;
    }

    reset();
}

void ViewSignalsGLib::request_display_update(ViewIface *view)
{
    log_assert(view != nullptr);

    if(!vm_.is_active_view(view))
        return;

    send(view, DISPLAY_UPDATE_REQUEST);
}

void ViewSignalsGLib::request_hide_view(ViewIface *view)
{
    log_assert(view != nullptr);

    if(!vm_.is_active_view(view))
        return;

    send(view, HIDE_VIEW_REQUEST);
}

struct ViewSignalSource
{
    GSource source;
    ViewSignalsGLib *object;
};

static inline ViewSignalsGLib *get_object(GSource *source)
{
    return reinterpret_cast<ViewSignalSource *>(source)->object;
}

static gboolean src_prepare(GSource *source, gint *timeout_)
{
    if(timeout_ != NULL)
        *timeout_ = -1;

    return get_object(source)->check() ? TRUE: FALSE;
}

static gboolean src_check(GSource *source)
{
    return get_object(source)->check() ? TRUE: FALSE;
}

static gboolean src_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
    get_object(source)->dispatch();
    return TRUE;
}

void ViewSignalsGLib::connect_to_main_loop(GMainLoop *loop)
{
    static GSourceFuncs funs =
    {
        src_prepare,
        src_check,
        src_dispatch,
    };

    GSource *source = g_source_new(&funs, sizeof(ViewSignalSource));
    log_assert(source != NULL);
    reinterpret_cast<ViewSignalSource *>(source)->object = this;

    ctx_ = g_main_loop_get_context(loop);
    source_id_ = g_source_attach(source, ctx_);
    log_assert(source_id_ > 0);
    g_source_unref(source);
}

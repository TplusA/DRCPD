#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cassert>

#include "view_filebrowser.hh"
#include "dbus_iface_deep.h"
#include "messages.h"

class FileItem: public List::TextItem
{
  private:
    bool is_directory_;

  public:
    FileItem(const FileItem &) = delete;
    FileItem &operator=(const FileItem &) = delete;
    explicit FileItem(FileItem &&) = default;

    explicit FileItem(const char *text, unsigned int flags,
                      bool item_is_directory):
        List::Item(flags),
        List::TextItem(text, true, flags),
        is_directory_(item_is_directory)
    {}

    bool is_directory() const
    {
        return is_directory_;
    }
};

bool ViewFileBrowser::View::init()
{
    fill_list_from_root();
    return true;
}

void ViewFileBrowser::View::focus()
{
    if(current_list_id_ == 0)
        fill_list_from_root();
}

void ViewFileBrowser::View::defocus()
{
}

ViewIface::InputResult ViewFileBrowser::View::input(DrcpCommand command)
{
    switch(command)
    {
      case DrcpCommand::SELECT_ITEM:
      case DrcpCommand::KEY_OK_ENTER:
        if(file_list_.empty())
            return InputResult::OK;

        if(auto item = dynamic_cast<const FileItem *>(file_list_.get_item(navigation_.get_cursor())))
        {
            if(item->is_directory())
            {
                if(fill_list_from_selected_line())
                    return InputResult::UPDATE_NEEDED;
            }
            else
                msg_info("Should add file \"%s\" to Streamplayer queue",
                         item->get_text());
        }

        return InputResult::OK;

      case DrcpCommand::GO_BACK_ONE_LEVEL:
        return fill_list_from_parent_link() ? InputResult::UPDATE_NEEDED : InputResult::OK;

      case DrcpCommand::SCROLL_DOWN_ONE:
        return navigation_.down() ? InputResult::UPDATE_NEEDED : InputResult::OK;

      case DrcpCommand::SCROLL_UP_ONE:
        return navigation_.up() ? InputResult::UPDATE_NEEDED : InputResult::OK;

      case DrcpCommand::SCROLL_PAGE_DOWN:
      {
        bool moved =
            ((navigation_.distance_to_bottom() == 0)
             ? navigation_.down(navigation_.maximum_number_of_displayed_lines_)
             : navigation_.down(navigation_.distance_to_bottom()));
        return moved ? InputResult::UPDATE_NEEDED : InputResult::OK;
      }

      case DrcpCommand::SCROLL_PAGE_UP:
      {
        bool moved =
            ((navigation_.distance_to_top() == 0)
             ? navigation_.up(navigation_.maximum_number_of_displayed_lines_)
             : navigation_.up(navigation_.distance_to_top()));
        return moved ? InputResult::UPDATE_NEEDED : InputResult::OK;
      }

      default:
        break;
    }

    return InputResult::OK;
}

void ViewFileBrowser::View::serialize(std::ostream &os)
{
    for(auto it : navigation_)
    {
        auto item = dynamic_cast<const FileItem *>(file_list_.get_item(it));
        assert(item != nullptr);

        if(it == navigation_.get_cursor())
            os << "--> ";
        else
            os << "    ";

        os << (item->is_directory() ? "Dir " : "File") << " " << it << ": "
           << item->get_text() << std::endl;
    }
}

void ViewFileBrowser::View::update(std::ostream &os)
{
    serialize(os);
}

bool ViewFileBrowser::View::fill_list_from_root()
{
    file_list_.clear();

    guint list_id;

    if(!tdbus_lists_navigation_call_get_list_id_sync(dbus_get_filebroker_lists_navigation_iface(),
                                                     0, 0, &list_id,
                                                     NULL, NULL))
    {
        /* this is not a hard error, it may only mean that the file broker
         * hasn't started up yet */
        msg_info("Failed obtaining ID for root list");
        current_list_id_ = 0;
        return false;
    }

    current_list_id_ = list_id;

    return fill_list_from_current_list_id();
}

bool ViewFileBrowser::View::fill_list_from_current_list_id()
{
    assert(current_list_id_ != 0);

    file_list_.clear();
    item_flags_.list_content_changed();
    navigation_.check_selection();

    guchar error_code;
    guint first_item;
    GVariant *out_list;

    if(!tdbus_lists_navigation_call_get_range_sync(dbus_get_filebroker_lists_navigation_iface(),
                                                   current_list_id_, 0, 0,
                                                   &error_code, &first_item, &out_list,
                                                   NULL, NULL))
    {
        /* D-Bus error, pending */
        msg_error(EAGAIN, LOG_NOTICE,
                  "Failed obtaining contents of list %u", current_list_id_);
        return false;
    }

    if(error_code != 0)
    {
        /* method error, stop trying */
        msg_error((error_code == 2) ? EIO : EINVAL, LOG_INFO,
                  "Error reading list %u", current_list_id_);
        current_list_id_ = 0;
        g_variant_unref(out_list);
        return false;
    }

    assert(g_variant_type_is_array(g_variant_get_type(out_list)));

    GVariantIter iter;
    if(g_variant_iter_init(&iter, out_list) > 0)
    {
        gchar *name;
        gboolean is_directory;

        while(g_variant_iter_next(&iter, "(sb)", &name, &is_directory))
        {
            List::append(&file_list_, FileItem(name, 0, !!is_directory));
            g_free(name);
        }

        assert(file_list_.get_number_of_items() == g_variant_n_children(out_list));

        item_flags_.list_content_changed();
        navigation_.check_selection();
    }

    /*!
     * \todo At this point, the \b complete file list is an RAM at least
     *     \e three times: once in our RAM list we have just filled here, once
     *     in the \c GVariant \c out_list we got over D-Bus, and once inside
     *     the \e FileBroker process. There is also a chance that the list also
     *     resides in the Linux VFS cache. This is clearly unnecessary. Instead
     *     of a simple RAM list, a list with partial content should be used
     *     here. The list should contain the displayed part, plus some extra
     *     entries before and after the displayed part to reduce
     *     user-expeciencable latencies.
     */

    g_variant_unref(out_list);

    return true;
}

bool ViewFileBrowser::View::fill_list_from_selected_line()
{
    if(file_list_.empty())
        return false;

    guint list_id;

    if(!tdbus_lists_navigation_call_get_list_id_sync(dbus_get_filebroker_lists_navigation_iface(),
                                                     current_list_id_, navigation_.get_cursor(),
                                                     &list_id, NULL, NULL))
    {
        msg_info("Failed obtaining ID for item %u in list %u",
                 navigation_.get_cursor(), current_list_id_);
        return false;
    }

    if(list_id == 0)
    {
        msg_error(EINVAL, LOG_NOTICE,
                  "Error obtaining ID for item %u in list %u",
                 navigation_.get_cursor(), current_list_id_);
        return false;
    }

    current_list_id_ = list_id;

    if(!fill_list_from_current_list_id())
        (void)fill_list_from_root();

    return true;
}

bool ViewFileBrowser::View::fill_list_from_parent_link()
{
    if(file_list_.empty())
        return false;

    guint list_id;
    guint item_id;

    if(!tdbus_lists_navigation_call_get_parent_link_sync(dbus_get_filebroker_lists_navigation_iface(),
                                                         current_list_id_, &list_id, &item_id,
                                                         NULL, NULL))
    {
        msg_info("Failed obtaining parent for list %u", current_list_id_);
        (void)fill_list_from_root();
        return true;
    }

    if(list_id == 0)
    {
        if(item_id == 1)
            return false;

        msg_error(EINVAL, LOG_NOTICE,
                  "Error obtaining parent for list %u", current_list_id_);
        (void)fill_list_from_root();
        return true;
    }

    current_list_id_ = list_id;

    if(fill_list_from_current_list_id())
        (void)navigation_.set_cursor(item_id);
    else
        (void)fill_list_from_root();

    return true;
}

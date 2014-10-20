#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cassert>

#include "view_config.hh"
#include "i18n.h"

class SettingItem: public List::TextItem
{
  private:
    SettingItem(const SettingItem &);
    SettingItem &operator=(const SettingItem &);

    ViewConfig::SettingBase *const setting_;
    bool is_editable_;

  public:
    explicit SettingItem(SettingItem &&) = default;

    explicit SettingItem(const char *text, unsigned int flags,
                         ViewConfig::SettingBase *setting,
                         bool setting_is_editable):
        List::Item(flags),
        List::TextItem(text, true, flags),
        setting_(setting),
        is_editable_(setting_is_editable)
    {
        assert(setting != nullptr);
    }

    bool is_editable() const
    {
        return is_editable_;
    }

    ViewConfig::SettingBase &get() const
    {
        return *setting_;
    }
};

class CallbackItem: public List::TextItem
{
  private:
    CallbackItem(const CallbackItem &);
    CallbackItem &operator=(const CallbackItem &);

    typedef void (*callback_t)(ViewConfig::View *);

    const callback_t fn_;

  public:
    explicit CallbackItem(CallbackItem &&) = default;

    explicit CallbackItem(const char *text, unsigned int flags,
                          const callback_t fn):
        List::Item(flags),
        List::TextItem(text, true, flags),
        fn_(fn)
    {}

    void activate(ViewConfig::View *view) const
    {
        if(fn_ != nullptr)
            fn_(view);
    }
};

static void save_and_exit(ViewConfig::View *view)
{
    view->apply_changed_settings();
}

/*!
 * \todo Need to read the settings from D-Bus. Currently, all settings contain
 *     some hardcoded values for testing.
 */
bool ViewConfig::View::init()
{
    List::append(&editable_menu_items_,
                 SettingItem(N_("MAC"), 0, &edit_settings_.mac_address_, false));
    List::append(&editable_menu_items_,
                 SettingItem(N_("DHCP"), 0, &edit_settings_.is_dhcp_on_, true));
    List::append(&editable_menu_items_,
                 SettingItem(N_("Proxy"), 0, &settings_.is_proxy_on_, true));
    List::append(&editable_menu_items_,
                 SettingItem(N_("Device name"), 0, &settings_.device_name_, true));
    List::append(&editable_menu_items_,
                 SettingItem(N_("Networking mode"), 0, &settings_.networking_mode_, true));
    List::append(&editable_menu_items_,
                 CallbackItem(N_("Save and restart"), 0, save_and_exit));
    List::append(&editable_menu_items_,
                 CallbackItem(N_("Exit without saving"), 0, nullptr));

    settings_.mac_address_ = "e0:3f:49:1a:70:45";
    settings_.device_name_ = "Test device";
    settings_.is_dhcp_on_ = true;
    settings_.is_proxy_on_ = false;
    settings_.networking_mode_ = Data::LAN_ONLY;

    return true;
}

/*!
 * \todo Need to read the settings from D-Bus here as well. It should be rather
 *     cheap (i.e., not really noticeable by the user) to do this, so for the
 *     sake of overall system stability and thus perceived quality we should
 *     probably do this here. We also don't need #ViewConfig::View::settings_
 *     then anymore.
 */
void ViewConfig::View::focus()
{
    edit_settings_ = settings_;
}

void ViewConfig::View::defocus()
{
}

ViewIface::InputResult ViewConfig::View::input(DrcpCommand command)
{
    switch(command)
    {
      case DrcpCommand::KEY_OK_ENTER:
        if(auto item = dynamic_cast<const CallbackItem *>(editable_menu_items_.get_item(navigation_.get_cursor())))
        {
            item->activate(this);
            return InputResult::SHOULD_HIDE;
        }

      case DrcpCommand::SCROLL_DOWN_ONE:
        return navigation_.down() ? InputResult::UPDATE_NEEDED : InputResult::OK;

      case DrcpCommand::SCROLL_UP_ONE:
        return navigation_.up() ? InputResult::UPDATE_NEEDED : InputResult::OK;

      default:
        break;
    }

    return InputResult::OK;
}

void ViewConfig::View::serialize(std::ostream &os)
{
    for(auto it : navigation_)
    {
        auto text_item = dynamic_cast<const List::TextItem *>(editable_menu_items_.get_item(it));

        assert(text_item != nullptr);

        if(it == navigation_.get_cursor())
            os << "--> ";
        else
            os << "    ";

        os << "Item " << it << ": " << text_item->get_text();

        auto setting = dynamic_cast<const SettingItem *>(text_item);

        if(setting != nullptr)
            os << "\t[" << (setting->is_editable() ? 'E' : 'S') << "] " << setting->get();

        os << std::endl;
    }
}

void ViewConfig::View::update(std::ostream &os)
{
    serialize(os);
}

/*!
 * \todo Need to distribute changed settings over D-Bus, then read back the
 *     settings to make sure we are in sync with the recipients.
 */
void ViewConfig::View::apply_changed_settings()
{
    settings_ = edit_settings_;
}

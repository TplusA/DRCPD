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

#include <iostream>
#include <iomanip>

#include "view_config.hh"
#include "ui_parameters_predefined.hh"
#include "i18n.h"

std::ostream &operator<<(std::ostream &os, const ViewConfig::MACAddr &addr)
{
    auto save_flags = os.flags(std::ios::hex);
    auto save_fill = os.fill('0');

    os << std::setw(2) << unsigned(addr.addr_[0]) << ':'
       << std::setw(2) << unsigned(addr.addr_[1]) << ':'
       << std::setw(2) << unsigned(addr.addr_[2]) << ':'
       << std::setw(2) << unsigned(addr.addr_[3]) << ':'
       << std::setw(2) << unsigned(addr.addr_[4]) << ':'
       << std::setw(2) << unsigned(addr.addr_[5]);

    os.flags(save_flags);
    os.fill(save_fill);

    return os;
}

std::ostream &operator<<(std::ostream &os, const ViewConfig::IPv4Addr &addr)
{
    auto save_flags = os.flags(std::ios::dec | std::ios::right);
    auto save_fill = os.fill(' ');

    os << std::setw(3) << unsigned(addr.addr_[0]) << '.'
       << std::setw(3) << unsigned(addr.addr_[1]) << '.'
       << std::setw(3) << unsigned(addr.addr_[2]) << '.'
       << std::setw(3) << unsigned(addr.addr_[3]);

    os.flags(save_flags);
    os.fill(save_fill);

    return os;
}

class SettingItem: public List::TextItem
{
  private:
    ViewConfig::SettingBase *const setting_;
    bool is_editable_;

  public:
    SettingItem(const SettingItem &) = delete;
    SettingItem &operator=(const SettingItem &) = delete;
    explicit SettingItem(SettingItem &&) = default;

    explicit SettingItem(const char *text, unsigned int flags,
                         ViewConfig::SettingBase *setting,
                         bool setting_is_editable):
        List::Item(flags),
        List::TextItem(text, true, flags),
        setting_(setting),
        is_editable_(setting_is_editable)
    {
        log_assert(setting != nullptr);
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
    typedef void (*callback_t)(ViewConfig::View *);

    const callback_t fn_;

  public:
    CallbackItem(const CallbackItem &) = delete;
    CallbackItem &operator=(const CallbackItem &) = delete;
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
                 SettingItem(N_("MAC"),
                             FilterFlags::item_is_not_selectable,
                             &edit_settings_.mac_address_, false));

    List::append(&editable_menu_items_,
                 SettingItem(N_("DHCP"), 0, &edit_settings_.is_dhcp_on_, true));
    List::append(&editable_menu_items_,
                 SettingItem(N_("Device IP"),
                             FilterFlags::item_invisible_if_dhcp_on,
                             &edit_settings_.device_ip_addr4_, true));
    List::append(&editable_menu_items_,
                 SettingItem(N_("IP mask"),
                             FilterFlags::item_invisible_if_dhcp_on,
                             &edit_settings_.device_subnet_mask4_, true));
    List::append(&editable_menu_items_,
                 SettingItem(N_("Gateway IP"),
                             FilterFlags::item_invisible_if_dhcp_on,
                             &edit_settings_.gateway_ip_addr4_, true));
    List::append(&editable_menu_items_,
                 SettingItem(N_("DNS 1"),
                             FilterFlags::item_invisible_if_dhcp_on,
                             &edit_settings_.dns_primary_ip_addr4_, true));
    List::append(&editable_menu_items_,
                 SettingItem(N_("DNS 2"),
                             FilterFlags::item_invisible_if_dhcp_on,
                             &edit_settings_.dns_secondary_ip_addr4_, true));

    List::append(&editable_menu_items_,
                 SettingItem(N_("Proxy"), 0, &edit_settings_.is_proxy_on_, true));
    List::append(&editable_menu_items_,
                 SettingItem(N_("Proxy IP"),
                             FilterFlags::item_invisible_if_proxy_off,
                             &edit_settings_.proxy_ip_addr4_, true));
    List::append(&editable_menu_items_,
                 SettingItem(N_("Proxy port"),
                             FilterFlags::item_invisible_if_proxy_off,
                             &edit_settings_.proxy_port_, true));

    List::append(&editable_menu_items_,
                 SettingItem(N_("Device name"), 0, &edit_settings_.device_name_, true));
    List::append(&editable_menu_items_,
                 SettingItem(N_("Networking mode"), 0, &edit_settings_.networking_mode_, true));
    List::append(&editable_menu_items_,
                 CallbackItem(N_("Save and restart"), 0, save_and_exit));
    List::append(&editable_menu_items_,
                 CallbackItem(N_("Exit without saving"), 0, nullptr));

    settings_.mac_address_ = MACAddr({0xe0, 0x3f, 0x49, 0x1a, 0x70, 0x45});
    settings_.device_name_ = "Test device";
    settings_.is_dhcp_on_ = true;
    settings_.is_proxy_on_ = false;
    settings_.networking_mode_ = Data::LAN_ONLY;

    update_visibility();

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

ViewIface::InputResult
ViewConfig::View::process_event(UI::ViewEventID event_id,
                                std::unique_ptr<const UI::Parameters> parameters)
{
    switch(event_id)
    {
      case UI::ViewEventID::NAV_SELECT_ITEM:
        if(auto item = dynamic_cast<const CallbackItem *>(editable_menu_items_.get_item(navigation_.get_cursor())))
        {
            item->activate(this);
            return InputResult::SHOULD_HIDE;
        }

        return InputResult::OK;

      case UI::ViewEventID::NAV_SCROLL_LINES:
      {
        const auto lines =
            UI::Events::downcast<UI::ViewEventID::NAV_SCROLL_LINES>(parameters);
        log_assert(lines != nullptr);

        if(lines->get_specific() < 0)
            return navigation_.down() ? InputResult::UPDATE_NEEDED : InputResult::OK;
        else
            return navigation_.up() ? InputResult::UPDATE_NEEDED : InputResult::OK;
      }

      case UI::ViewEventID::NAV_SCROLL_PAGES:
      {
        const auto pages =
            UI::Events::downcast<UI::ViewEventID::NAV_SCROLL_PAGES>(parameters);
        log_assert(pages != nullptr);

        bool moved;

        if(pages->get_specific() < 0)
            moved = ((navigation_.distance_to_bottom() == 0)
                     ? navigation_.down(navigation_.maximum_number_of_displayed_lines_)
                     : navigation_.down(navigation_.distance_to_bottom()));
        else
            moved = ((navigation_.distance_to_top() == 0)
                     ? navigation_.up(navigation_.maximum_number_of_displayed_lines_)
                     : navigation_.up(navigation_.distance_to_top()));

        return moved ? InputResult::UPDATE_NEEDED : InputResult::OK;
      }

      default:
        BUG("Unexpected view event 0x%08x for config view",
            static_cast<unsigned int>(event_id));

        break;
    }

    return InputResult::OK;
}

void ViewConfig::View::serialize(DCP::Queue &queue, DCP::Queue::Mode mode,
                                 std::ostream *debug_os)
{
    ViewSerializeBase::serialize(queue, mode);

    if(!debug_os)
        return;

    for(auto it : navigation_)
    {
        auto text_item = dynamic_cast<const List::TextItem *>(editable_menu_items_.get_item(it));

        log_assert(text_item != nullptr);

        if(it == navigation_.get_cursor())
            *debug_os << "--> ";
        else
            *debug_os << "    ";

        *debug_os << "Item " << it << ": " << text_item->get_text();

        auto setting = dynamic_cast<const SettingItem *>(text_item);

        if(setting != nullptr)
            *debug_os << "\t[" << (setting->is_editable() ? 'E' : 'S') << "] " << setting->get();

        *debug_os << std::endl;
    }
}

/*!
 * \todo Need to distribute changed settings over D-Bus, then read back the
 *     settings to make sure we are in sync with the recipients.
 */
void ViewConfig::View::apply_changed_settings()
{
    settings_ = edit_settings_;
}

void ViewConfig::View::update_visibility()
{
    unsigned int visibility_flags = 0;

    if(settings_.is_dhcp_on_.value())
        visibility_flags |= FilterFlags::item_invisible_if_dhcp_on;

    if(!settings_.is_proxy_on_.value())
        visibility_flags |= FilterFlags::item_invisible_if_proxy_off;

    item_flags_.set_visible_mask(visibility_flags);
    navigation_.check_selection();
}

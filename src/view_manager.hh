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

#ifndef VIEW_MANAGER_HH
#define VIEW_MANAGER_HH

#include <map>
#include <memory>
#include <algorithm>

#include "view.hh"
#include "dcp_transaction.hh"

/*!
 * \addtogroup view_manager Management of the various views
 */
/*!@{*/

namespace ViewManager
{

/*!
 * Helper class for constructing tables of input command redirections.
 */
class InputBouncer
{
  public:
    class Item
    {
      public:
        const DrcpCommand input_command_;
        const DrcpCommand xform_command_;
        const char *const view_name_;

        Item(const Item &) = delete;
        Item &operator=(const Item &) = delete;
        Item(Item &&) = default;

        constexpr explicit Item(DrcpCommand command,
                                const char *view_name) throw():
            input_command_(command),
            xform_command_(command),
            view_name_(view_name)
        {}

        constexpr explicit Item(DrcpCommand input_command,
                                DrcpCommand xform_command,
                                const char *view_name) throw():
            input_command_(input_command),
            xform_command_(xform_command),
            view_name_(view_name)
        {}
    };

  private:
    const Item *const items_;
    const size_t items_count_;

  public:
    InputBouncer(const InputBouncer &) = delete;
    InputBouncer &operator=(const InputBouncer &) = delete;

    template <size_t N>
    constexpr explicit InputBouncer(const Item (&items)[N]) throw():
        items_(items),
        items_count_(N)
    {}

    const Item *find(DrcpCommand command) const
    {
        const Item *it =
            std::find_if(items_, items_ + items_count_,
                         [command] (const Item &item) -> bool
                         {
                             return item.input_command_ == command;
                         });

        return (it < items_ + items_count_) ? it : nullptr;
    }
};

class VMIface
{
  protected:
    explicit VMIface() {}

  public:
    VMIface(const VMIface &) = delete;
    VMIface &operator=(const VMIface &) = delete;

    virtual ~VMIface() {}

    virtual bool add_view(ViewIface *view) = 0;
    virtual void set_output_stream(std::ostream &os) = 0;
    virtual void set_debug_stream(std::ostream &os) = 0;

    virtual void serialization_result(DcpTransaction::Result result) = 0;

    virtual void input(DrcpCommand command,
                       const UI::Parameters *parameters = nullptr) = 0;
    virtual ViewIface::InputResult
    input_bounce(const InputBouncer &bouncer, DrcpCommand command,
                 const UI::Parameters *parameters = nullptr) = 0;
    virtual void input_move_cursor_by_line(int lines) = 0;
    virtual void input_move_cursor_by_page(int pages) = 0;
    virtual ViewIface *get_view_by_name(const char *view_name) = 0;
    virtual ViewIface *get_view_by_dbus_proxy(const void *dbus_proxy) = 0;
    virtual ViewIface *get_playback_initiator_view() const = 0;
    virtual void activate_view_by_name(const char *view_name) = 0;
    virtual void toggle_views_by_name(const char *view_name_a,
                                      const char *view_name_b) = 0;
    virtual bool is_active_view(const ViewIface *view) const = 0;
    virtual bool serialize_view_if_active(const ViewIface *view) const = 0;
    virtual bool update_view_if_active(const ViewIface *view) const = 0;
    virtual void hide_view_if_active(const ViewIface *view) = 0;
};

class Manager: public VMIface
{
  public:
    using ViewsContainer = std::map<const std::string, ViewIface *>;

  private:
    ViewsContainer all_views_;

    ViewIface *active_view_;
    ViewIface *last_browse_view_;
    DcpTransaction &dcp_transaction_;
    std::ostream *debug_stream_;

  public:
    Manager(const Manager &) = delete;
    Manager &operator=(const Manager &) = delete;

    explicit Manager(DcpTransaction &dcpd);

    bool add_view(ViewIface *view) override;
    void set_output_stream(std::ostream &os) override;
    void set_debug_stream(std::ostream &os) override;

    void serialization_result(DcpTransaction::Result result) override;

    void input(DrcpCommand command, const UI::Parameters *parameters = nullptr) override;

    ViewIface::InputResult input_bounce(const InputBouncer &bouncer,
                                        DrcpCommand command,
                                        const UI::Parameters *parameters) override
    {
        (void)do_input_bounce(bouncer, command, parameters);
        return ViewIface::InputResult::OK;
    }

    void input_move_cursor_by_line(int lines) override;
    void input_move_cursor_by_page(int pages) override;
    ViewIface *get_view_by_name(const char *view_name) override;
    ViewIface *get_view_by_dbus_proxy(const void *dbus_proxy) override;
    ViewIface *get_playback_initiator_view() const override;
    void activate_view_by_name(const char *view_name) override;
    void toggle_views_by_name(const char *view_name_a,
                              const char *view_name_b) override;
    bool is_active_view(const ViewIface *view) const override;
    bool serialize_view_if_active(const ViewIface *view) const override;
    bool update_view_if_active(const ViewIface *view) const override;
    void hide_view_if_active(const ViewIface *view) override;

  private:
    void activate_view(ViewIface *view);
    void handle_input_result(ViewIface::InputResult result, ViewIface &view);

    bool do_input_bounce(const InputBouncer &bouncer, DrcpCommand command,
                         const UI::Parameters *parameters = nullptr);
};

}

/*!@}*/

#endif /* !VIEW_MANAGER_HH */

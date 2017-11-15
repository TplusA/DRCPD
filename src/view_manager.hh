/*
 * Copyright (C) 2015, 2016, 2017  T+A elektroakustik GmbH & Co. KG
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
#include "ui_event_queue.hh"
#include "dcp_transaction_queue.hh"
#include "configuration.hh"
#include "configuration_drcpd.hh"

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
        const UI::ViewEventID input_event_id_;
        const UI::ViewEventID xform_event_id_;
        const char *const view_name_;

        Item(const Item &) = delete;
        Item &operator=(const Item &) = delete;
        Item(Item &&) = default;

        constexpr explicit Item(UI::ViewEventID event_id,
                                const char *view_name) throw():
            input_event_id_(event_id),
            xform_event_id_(event_id),
            view_name_(view_name)
        {}

        constexpr explicit Item(UI::ViewEventID input_event_id,
                                UI::ViewEventID xform_event_id,
                                const char *view_name) throw():
            input_event_id_(input_event_id),
            xform_event_id_(xform_event_id),
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

    const Item *find(UI::ViewEventID event_id) const
    {
        const Item *it =
            std::find_if(items_, items_ + items_count_,
                         [event_id] (const Item &item) -> bool
                         {
                             return item.input_event_id_ == event_id;
                         });

        return (it < items_ + items_count_) ? it : nullptr;
    }
};

class VMIface
{
  protected:
    explicit VMIface() {}

  public:
    static constexpr const unsigned int NUMBER_OF_LINES_ON_DISPLAY = 3;

    VMIface(const VMIface &) = delete;
    VMIface &operator=(const VMIface &) = delete;

    virtual ~VMIface() {}

    virtual bool add_view(ViewIface *view) = 0;
    virtual bool invoke_late_init_functions() = 0;
    virtual void set_output_stream(std::ostream &os) = 0;
    virtual void set_debug_stream(std::ostream &os) = 0;
    virtual void set_resume_playback_configuration_file(const char *filename) = 0;

    virtual void deselected_notification() = 0;
    virtual void shutdown() = 0;

    virtual void language_settings_changed_notification() = 0;

    virtual const char *get_resume_url_by_audio_source_id(const std::string &id) const = 0;

    /*!
     * End of DCP transmission, callback from I/O layer.
     */
    virtual void serialization_result(DCP::Transaction::Result result) = 0;

    virtual ViewIface::InputResult
    input_bounce(const InputBouncer &bouncer, UI::ViewEventID event_id,
                 std::unique_ptr<const UI::Parameters> parameters = nullptr) = 0;
    virtual ViewIface *get_view_by_name(const char *view_name) = 0;

    /*
     * TODO: Maybe remove entirely and replace by events
     */
    virtual void sync_activate_view_by_name(const char *view_name,
                                            bool enforce_reactivation) = 0;
    /*
     * TODO: Maybe remove entirely and replace by events
     */
    virtual void sync_toggle_views_by_name(const char *view_name_a,
                                           const char *view_name_b,
                                           bool enforce_reactivation) = 0;
    virtual bool is_active_view(const ViewIface *view) const = 0;
    virtual void serialize_view_if_active(const ViewIface *view, DCP::Queue::Mode mode) const = 0;
    virtual void serialize_view_forced(const ViewIface *view, DCP::Queue::Mode mode) const = 0;
    virtual void update_view_if_active(const ViewIface *view, DCP::Queue::Mode mode) const = 0;
    virtual void hide_view_if_active(const ViewIface *view) = 0;

    virtual Configuration::ConfigChangedIface &get_config_changer() const = 0;
    virtual const Configuration::DrcpdValues &get_configuration() const = 0;
};

class Manager: public VMIface, public UI::EventStoreIface
{
  public:
    using ViewsContainer = std::map<const std::string, ViewIface *>;
    using ConfigMgr = Configuration::ConfigManager<Configuration::DrcpdValues>;

  private:
    ViewsContainer all_views_;

    UI::EventQueue &ui_events_;

    ConfigMgr &config_manager_;

    static const char RESUME_CONFIG_SECTION__AUDIO_SOURCES[];
    const char *resume_playback_config_filename_;
    struct ini_file resume_configuration_file_;

    ViewIface *active_view_;
    ViewIface *last_browse_view_;
    DCP::Queue &dcp_transaction_queue_;
    std::ostream *debug_stream_;

  public:
    Manager(const Manager &) = delete;
    Manager &operator=(const Manager &) = delete;

    explicit Manager(UI::EventQueue &event_queue, DCP::Queue &dcp_queue,
                     ConfigMgr &config_manager);

    virtual ~Manager();

    bool add_view(ViewIface *view) override;
    bool invoke_late_init_functions() override;
    void set_output_stream(std::ostream &os) override;
    void set_debug_stream(std::ostream &os) override;
    void set_resume_playback_configuration_file(const char *filename) override;
    void deselected_notification() override;
    void shutdown() override;

    void language_settings_changed_notification() override;

    const char *get_resume_url_by_audio_source_id(const std::string &id) const override;

    void serialization_result(DCP::Transaction::Result result) override;

    void store_event(UI::EventID event_id,
                     std::unique_ptr<const UI::Parameters> parameters = nullptr) override;

    ViewIface::InputResult input_bounce(const InputBouncer &bouncer,
                                        UI::ViewEventID event_id,
                                        std::unique_ptr<const UI::Parameters> parameters) override
    {
        (void)do_input_bounce(bouncer, event_id, parameters);
        return ViewIface::InputResult::OK;
    }

    ViewIface *get_view_by_name(const char *view_name) override;
    /*
     * TODO: Maybe remove entirely and replace by events
     */
    void sync_activate_view_by_name(const char *view_name,
                                    bool enforce_reactivation) override;
    /*
     * TODO: Maybe remove entirely and replace by events
     */
    void sync_toggle_views_by_name(const char *view_name_a,
                                   const char *view_name_b,
                                   bool enforce_reactivation) override;
    bool is_active_view(const ViewIface *view) const override;
    void serialize_view_if_active(const ViewIface *view, DCP::Queue::Mode mode) const override;
    void serialize_view_forced(const ViewIface *view, DCP::Queue::Mode mode) const override;
    void update_view_if_active(const ViewIface *view, DCP::Queue::Mode mode) const override;
    void hide_view_if_active(const ViewIface *view) override;

    void process_pending_events();

    void busy_state_notification(bool is_busy);

    Configuration::ConfigChangedIface &get_config_changer() const override
    {
        return config_manager_;
    }

    const Configuration::DrcpdValues &get_configuration() const override
    {
        return config_manager_.values();
    }

  private:
    void configuration_changed_notification(const char *origin,
                                            const std::array<bool, Configuration::DrcpdValues::NUMBER_OF_KEYS> &changed);

    ViewIface *get_view_by_dbus_proxy(const void *dbus_proxy);
    void activate_view(ViewIface *view, bool enforce_reactivation);
    void handle_input_result(ViewIface::InputResult result, ViewIface &view);

    bool do_input_bounce(const InputBouncer &bouncer, UI::ViewEventID event_id,
                         std::unique_ptr<const UI::Parameters> &parameters);

    void dispatch_event(UI::ViewEventID event_id,
                        std::unique_ptr<const UI::Parameters> parameters);
    void dispatch_event(UI::BroadcastEventID event_id,
                        std::unique_ptr<const UI::Parameters> parameters);
    void dispatch_event(UI::VManEventID event_id,
                        std::unique_ptr<const UI::Parameters> parameters);
};

}

/*!@}*/

#endif /* !VIEW_MANAGER_HH */

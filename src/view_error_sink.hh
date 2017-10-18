/*
 * Copyright (C) 2017  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_ERROR_SINK_HH
#define VIEW_ERROR_SINK_HH

#include <mutex>

#include "view.hh"
#include "view_serialize.hh"
#include "view_names.hh"
#include "error_sink.hh"

/*!
 * \addtogroup view_error_sink Error sink view
 * \ingroup views
 *
 * All errors emitted by the system are collected by this view and emitted to
 * the SPI slave at some time.
 */
/*!@{*/

namespace ViewErrorSink
{

class View: public ViewIface, public ViewSerializeBase, public Error::Sink
{
  private:
    mutable std::mutex errors_lock_;
    std::deque<Error::Error> errors_;

  public:
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    explicit View(const char *on_screen_name,
                  ViewManager::VMIface *view_manager):
        ViewIface(ViewNames::ERROR_SINK, false, view_manager),
        ViewSerializeBase(on_screen_name, ViewID::ERROR)
    {
        install_singleton(*this);
    }

    virtual ~View()
    {
        remove_singleton();
    }

    bool init() override
    {
        errors_.clear();
        return true;
    }

    void focus() override {}
    void defocus() override {}

    void update(DCP::Queue &queue, DCP::Queue::Mode mode,
                std::ostream *debug_os = nullptr) final override
    {
        serialize(queue, mode, debug_os);
    }

    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<const UI::Parameters> parameters) final override
    {
        return InputResult::SHOULD_HIDE;
    }

    void process_broadcast(UI::BroadcastEventID event_id,
                           const UI::Parameters *parameters) final override {}

  protected:
    void sink_error(Error::Error &&error) final override;

    uint32_t about_to_write_xml(const DCP::Queue::Data &data) const final override;

    std::pair<const ViewID, const ScreenID::id_t>
    get_dynamic_ids(uint32_t bits) const final override;

    bool write_xml(std::ostream &os, uint32_t bits,
                   const DCP::Queue::Data &data) final override;

    bool write_xml_end(std::ostream &os, uint32_t bits,
                       const DCP::Queue::Data &data) final override;
};

}

/*!@}*/

#endif /* !VIEW_ERROR_SINK_HH */

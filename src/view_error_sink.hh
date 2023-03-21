/*
 * Copyright (C) 2017--2021, 2023  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef VIEW_ERROR_SINK_HH
#define VIEW_ERROR_SINK_HH

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
                  ViewManager::VMIface &view_manager):
        ViewIface(ViewNames::ERROR_SINK, ViewIface::Flags(), view_manager),
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
                std::ostream *debug_os, const Maybe<bool> &is_busy) final override
    {
        serialize(queue, mode, debug_os, is_busy);
    }

    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<UI::Parameters> parameters) final override
    {
        return InputResult::SHOULD_HIDE;
    }

    void process_broadcast(UI::BroadcastEventID event_id,
                           UI::Parameters *parameters) final override {}

  protected:
    void sink_error(Error::Error &&error) final override;

    bool is_serialization_allowed() const final override { return true; }
    uint32_t about_to_write_xml(const DCP::Queue::Data &data) const final override;

    std::pair<const ViewID, const ScreenID::id_t>
    get_dynamic_ids(uint32_t bits) const final override;

    bool write_xml(std::ostream &os, uint32_t bits,
                   const DCP::Queue::Data &data,
                   bool &busy_state_triggered) final override;

    bool write_xml_end(std::ostream &os, uint32_t bits,
                       const DCP::Queue::Data &data,
                       bool busy_state_triggered) final override;
};

}

/*!@}*/

#endif /* !VIEW_ERROR_SINK_HH */

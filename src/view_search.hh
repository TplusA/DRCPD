/*
 * Copyright (C) 2016, 2017, 2018, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_SEARCH_HH
#define VIEW_SEARCH_HH

#include "view.hh"
#include "view_serialize.hh"
#include "view_names.hh"
#include "search_parameters.hh"

/*!
 * \addtogroup view_search Search for media data
 * \ingroup views
 *
 * An editor for all kinds of search queries.
 */
/*!@{*/

namespace ViewSearch
{

class View: public ViewIface, public ViewSerializeBase
{
  private:
    using ParamType = UI::SpecificParameters<SearchParameters>;
    std::unique_ptr<const UI::Parameters> query_;

    const ViewIface *request_view_;
    std::string request_context_;

  public:
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    explicit View(const char *on_screen_name, unsigned int max_lines,
                  ViewManager::VMIface *view_manager):
        ViewIface(ViewNames::SEARCH_OPTIONS, ViewIface::Flags(), view_manager),
        ViewSerializeBase(on_screen_name, ViewID::EDIT),
        request_view_(nullptr)
    {}

    virtual ~View() {}

    bool init() override;

    void focus() override;
    void defocus() override;

    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<const UI::Parameters> parameters) final override;
    void process_broadcast(UI::BroadcastEventID event_id,
                           const UI::Parameters *parameters) final override {}

    void serialize(DCP::Queue &queue, DCP::Queue::Mode mode,
                   std::ostream *debug_os = nullptr) override
    {
        if(can_serialize())
            ViewSerializeBase::serialize(queue, mode, debug_os);
    }

    void update(DCP::Queue &queue, DCP::Queue::Mode mode,
                std::ostream *debug_os = nullptr) override
    {
        if(can_serialize())
            ViewSerializeBase::update(queue, mode, debug_os);
    }

    void request_parameters_for_context(const ViewIface *view, const char *context)
    {
        request_view_ = view;
        request_context_ = context;
    }

    void request_parameters_for_context(const ViewIface *view, const std::string &context)
    {
        request_view_ = view;
        request_context_ = context;
    }

    const SearchParameters *get_parameters() const
    {
        return (query_ != nullptr)
            ? &(static_cast<const ParamType *>(query_.get())->get_specific())
            : nullptr;
    }

    const ViewIface *get_request_view() const { return request_view_; }

    void forget_parameters()
    {
        query_ = nullptr;
        request_view_ = nullptr;
        request_context_.clear();
    }

  private:
    bool write_xml(std::ostream &os, uint32_t bits,
                   const DCP::Queue::Data &data) override;

    bool can_serialize() const
    {
        return !request_context_.empty();
    }
};

}

/*!@}*/

#endif /* !VIEW_SEARCH_HH */

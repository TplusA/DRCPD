#ifndef VIEW_PLAY_HH
#define VIEW_PLAY_HH

#include "view.hh"
#include "playinfo.hh"

/*!
 * \addtogroup view_play Player view
 * \ingroup views
 *
 * Information about currently playing stream.
 */
/*!@{*/

namespace ViewPlay
{

class Iface
{
  protected:
    explicit Iface() {}

  public:
    Iface(const Iface &) = delete;
    Iface &operator=(const Iface &) = delete;

    virtual ~Iface() {}

    virtual void notify_stream_start(uint32_t id, const std::string &url,
                                     bool url_fifo_is_full) = 0;
    virtual void notify_stream_stop() = 0;
    virtual void notify_stream_pause() = 0;
    virtual void notify_stream_position_changed(const std::chrono::milliseconds &position,
                                                const std::chrono::milliseconds &duration) = 0;

    virtual void meta_data_add_begin() = 0;
    virtual void meta_data_add(const char *key, const char *value) = 0;
    virtual void meta_data_add_end() = 0;
};

extern const PlayInfo::Reformatters meta_data_reformatters;

class View: public ViewIface, public ViewPlay::Iface
{
  private:
    static constexpr uint16_t update_flags_need_full_update = 1U << 0;
    static constexpr uint16_t update_flags_stream_position  = 1U << 1;
    static constexpr uint16_t update_flags_playback_state   = 1U << 2;
    static constexpr uint16_t update_flags_meta_data        = 1U << 3;

    bool is_visible_;
    PlayInfo::Data info_;
    PlayInfo::MetaData incoming_meta_data_;
    uint16_t update_flags_;

  public:
    View(const View &) = delete;

    View &operator=(const View &) = delete;

    explicit View(const char *on_screen_name, unsigned int max_lines,
                  ViewSignalsIface *view_signals):
        ViewIface("Play", on_screen_name, "play", 109U, false, view_signals),
        is_visible_(false),
        update_flags_(0)
    {}

    virtual ~View() {}

    bool init() override;

    void focus() override;
    void defocus() override;

    InputResult input(DrcpCommand command) override;

    void notify_stream_start(uint32_t id, const std::string &url,
                             bool url_fifo_is_full) override;
    void notify_stream_stop() override;
    void notify_stream_pause() override;
    void notify_stream_position_changed(const std::chrono::milliseconds &position,
                                        const std::chrono::milliseconds &duration) override;

    void meta_data_add_begin() override
    {
        incoming_meta_data_.clear();
    }

    void meta_data_add(const char *key, const char *value) override
    {
        incoming_meta_data_.add(key, value, meta_data_reformatters);
    }

    void meta_data_add_end() override
    {
        if(incoming_meta_data_ == info_.meta_data_)
            incoming_meta_data_.clear();
        else
        {
            info_.meta_data_ = incoming_meta_data_;
            incoming_meta_data_.clear();
            display_update(update_flags_meta_data);
        }
    }

    bool serialize(DcpTransaction &dcpd, std::ostream *debug_os) override;
    bool update(DcpTransaction &dcpd, std::ostream *debug_os) override;

  private:
    /*!
     * Generate XML document from current state.
     */
    bool write_xml(std::ostream &os, bool is_full_view) override;

    void display_update(uint16_t update_flags)
    {
        update_flags_ |= update_flags;
        view_signals_->request_display_update(this);
    }
};

};

/*!@}*/

#endif /* !VIEW_PLAY_HH */


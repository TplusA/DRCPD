#ifndef VIEW_CONFIG_HH
#define VIEW_CONFIG_HH

#include "view.hh"
#include "ramlist.hh"
#include "listnav.hh"

/*!
 * \addtogroup view_config Device configuration menu
 * \ingroup views
 *
 * Configuration options that are visible and mostly changable by the user.
 */
/*!@{*/

namespace ViewConfig
{
    class MACAddr;
    class IPv4Addr;
};

std::ostream &operator<<(std::ostream &os, const ::ViewConfig::MACAddr &addr);
std::ostream &operator<<(std::ostream &os, const ::ViewConfig::IPv4Addr &addr);

namespace ViewConfig
{

class SettingBase
{
  protected:
    bool is_defined_;

  public:
    SettingBase(const SettingBase &) = delete;

    explicit SettingBase(): is_defined_(false) {}

    virtual ~SettingBase() {}

    friend std::ostream &operator<<(std::ostream &os, const SettingBase &s)
    {
        return s.write_to_stream(os);
    }

  protected:
    virtual std::ostream &write_to_stream(std::ostream &os) const = 0;
};

template <typename T>
class Setting: public SettingBase
{
  private:
    T data_;

  public:
    Setting(const Setting &) = delete;

    explicit Setting() {}

    Setting<T> &operator=(T &&src)
    {
        is_defined_ = true;
        data_ = src;
        return *this;
    }

    const T &value() const
    {
        return data_;
    }

  protected:
    std::ostream &write_to_stream(std::ostream &os) const override
    {
        if(is_defined_)
            return os << data_;
        else
            return os << "UNDEFINED";
    }
};

class MACAddr
{
  private:
    std::array<uint8_t, 6> addr_;

  public:
    MACAddr(const MACAddr &) = delete;
    MACAddr(std::array<uint8_t, 6> &&addr): addr_(addr) {}
    constexpr explicit MACAddr(): addr_{0U, 0U, 0U, 0U, 0U, 0U} {}
    friend std::ostream &::operator<<(std::ostream &os, const MACAddr &addr);
};

class IPv4Addr
{
  private:
    std::array<uint8_t, 4> addr_;

  public:
    IPv4Addr(const IPv4Addr &) = delete;
    IPv4Addr(std::array<uint8_t, 4> &&addr): addr_(addr) {}
    constexpr explicit IPv4Addr(): addr_{0U, 0U, 0U, 0U} {}
    friend std::ostream &::operator<<(std::ostream &os, const IPv4Addr &addr);
};

class Data
{
  public:
    enum NetworkingMode
    {
        AUTOMATIC,
        LAN_ONLY,
        WIFI_ONLY,
    };

  private:
    bool is_valid_;

  public:
    Setting<MACAddr> mac_address_;
    Setting<std::string> device_name_;
    Setting<bool> is_dhcp_on_;
    Setting<IPv4Addr> device_ip_addr4_;
    Setting<IPv4Addr> device_subnet_mask4_;
    Setting<IPv4Addr> gateway_ip_addr4_;
    Setting<IPv4Addr> dns_primary_ip_addr4_;
    Setting<IPv4Addr> dns_secondary_ip_addr4_;
    Setting<bool> is_proxy_on_;
    Setting<IPv4Addr> proxy_ip_addr4_;
    Setting<uint16_t> proxy_port_;
    Setting<NetworkingMode> networking_mode_;

    Data(const Data &) = delete;

    explicit Data(): is_valid_(false) {}
};

class FilterFlags: public List::NavItemFilterIface
{
  public:
    static constexpr unsigned int item_is_not_selectable      = (1U << 0);
    static constexpr unsigned int item_invisible_if_dhcp_on   = (1U << 16);
    static constexpr unsigned int item_invisible_if_proxy_off = (1U << 17);

  private:
    unsigned int visibility_flags_;

  public:
    FilterFlags(const FilterFlags &) = delete;
    FilterFlags &operator=(const FilterFlags &) = delete;

    constexpr explicit FilterFlags(const List::ListIface *list):
        NavItemFilterIface(list),
        visibility_flags_(0)
    {}

    void set_visible_mask(unsigned int flags) { visibility_flags_ = flags; }
    void list_content_changed() override {}
    bool ensure_consistency() const override { return false; }
    bool is_visible(unsigned int flags) const override { return !(flags & visibility_flags_); }
    bool is_selectable(unsigned int flags) const override { return !(flags & (item_is_not_selectable | visibility_flags_)); }
    unsigned int get_first_selectable_item() const override { return 1U; }
    unsigned int get_last_selectable_item() const override { return list_->get_number_of_items() - 1; }
    unsigned int get_first_visible_item() const override { return 0U; }
    unsigned int get_last_visible_item() const override { return list_->get_number_of_items() - 1; }
    unsigned int get_total_number_of_visible_items() const override { return list_->get_number_of_items(); }
    unsigned int get_flags_for_item(unsigned int item) const override { return list_->get_item(item)->get_flags(); }

    /*!
     * \todo Not implemented yet.
     */
    bool map_line_number_to_item(unsigned int line_number,
                                 unsigned int &item) const override
    {
        return false;
    }

    /*!
     * \todo Not implemented yet.
     */
    bool map_item_to_line_number(unsigned int item,
                                 unsigned int &line_number) const override
    {
        return false;
    }
};

class View: public ViewIface
{
  private:
    Data settings_;
    Data edit_settings_;

    List::RamList editable_menu_items_;
    FilterFlags item_flags_;
    List::Nav navigation_;

  public:
    View(const View &) = delete;

    View &operator=(const View &) = delete;

    explicit View(const char *on_screen_name, unsigned int max_lines):
        ViewIface("Config", on_screen_name, "config", 73U),
        item_flags_(&editable_menu_items_),
        navigation_(max_lines, item_flags_)
    {}

    bool init() override;

    void focus() override;
    void defocus() override;

    InputResult input(DrcpCommand command) override;

    void serialize(DcpTransaction &dcpd, std::ostream *debug_os) override;

    void apply_changed_settings();

  private:
    void update_visibility();
};

};

/*!@}*/

#endif /* !VIEW_CONFIG_HH */

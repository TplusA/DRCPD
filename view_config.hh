#ifndef VIEW_CONFIG_HH
#define VIEW_CONFIG_HH

#include "view.hh"
#include "ramlist.hh"
#include "listnav.hh"

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

  protected:
    std::ostream &write_to_stream(std::ostream &os) const override
    {
        if(is_defined_)
            return os << data_;
        else
            return os << "UNDEFINED";
    }
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
    Setting<std::string> mac_address_;
    Setting<std::string> device_name_;
    Setting<bool> is_dhcp_on_;
    Setting<bool> is_proxy_on_;
    Setting<NetworkingMode> networking_mode_;

    Data(const Data &) = delete;

    explicit Data(): is_valid_(false) {}
};

class View: public ViewIface
{
  private:
    Data settings_;
    Data edit_settings_;

    List::RamList editable_menu_items_;
    ListNav navigation;

  public:
    View(const View &) = delete;

    View &operator=(const View &) = delete;

    explicit View(unsigned int max_lines):
        navigation(1, 0, 7, max_lines)
    {}

    bool init() override;

    void focus() override;
    void defocus() override;

    InputResult input(DrcpCommand command) override;

    void serialize(std::ostream &os) override;
    void update(std::ostream &os) override;

    void apply_changed_settings();
};

};

#endif /* !VIEW_CONFIG_HH */

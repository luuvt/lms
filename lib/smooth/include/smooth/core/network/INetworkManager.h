#pragma once

#include <tuple>
#include <functional>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <esp_modem.h>
#pragma GCC diagnostic pop

namespace smooth::core::network
{
    class INetworkManager
    {
        public:
        virtual void init() {};
        virtual void stop() {};
        virtual bool connect() = 0;
        virtual void setConfig(const esp_modem_dte_config_t &config) {};
        virtual std::string getImei() { return {};};
        virtual std::string getImsi() { return {};};
        virtual std::string getCcid() { return {};};

        virtual void set_host_name(const std::string& name) {};
        virtual void set_ap_credentials(const std::string& wifi_ssid, const std::string& wifi_password) {};
        virtual void set_auto_connect(bool auto_connect) {};
        virtual void connect_to_ap() {};
        virtual void start_softap(uint8_t max_conn = 1) {};
        virtual void start_smartconfig() {};
        virtual void start_provision() {};
        virtual bool is_smart_config() { return false;};
        virtual std::tuple<bool, std::string, std::string> get_config() {return std::make_tuple( false, "", "");};
        virtual int get_rssi() {return 0;};
        [[nodiscard]] virtual std::string get_mac_address() { return {};};
        
        virtual bool listSms() { return false;};
        virtual bool readSms(unsigned int index) { return false;};
        virtual bool deleteSms(unsigned int index) { return false;};
        virtual bool deleteAllSms() { return false;};
        virtual bool queryNetRegistrationInfo() { return false;};
        
        virtual ~INetworkManager() = default;
    };
}
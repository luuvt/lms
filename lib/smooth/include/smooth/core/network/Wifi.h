/*
Smooth - A C++ framework for embedded programming on top of Espressif's ESP-IDF
Copyright 2019 Per Malmberg (https://gitbub.com/PerMalmberg)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once

#include <string>
#include <array>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <esp_wifi.h>
#pragma GCC diagnostic pop
#include "smooth/core/ipc/IEventListener.h"
#include "INetworkManager.h"

namespace smooth::core::network
{
    /// Wifi management class
    class Wifi : public INetworkManager
    {
        public:
            Wifi();

            Wifi(const Wifi&) = delete;

            Wifi(Wifi&&) = delete;

            Wifi& operator=(const Wifi&) = delete;

            Wifi& operator=(Wifi&&) = delete;

            ~Wifi();

            /// Sets the hostname
            /// \param name The name
            void set_host_name(const std::string& name) override;

            /// Sets the credentials for the Wifi network
            /// \param wifi_ssid The SSID
            /// \param wifi_password The password
            void set_ap_credentials(const std::string& wifi_ssid, const std::string& wifi_password) override;

            /// Enables, disables auto reconnect on loss of Wifi connection.
            /// \param auto_connect
            void set_auto_connect(bool auto_connect) override;

            /// Initiates the connection to the AP.
            void connect_to_ap() override;

            /// Returns a value indicating of currently connected to the access point.
            /// \return
            [[nodiscard]] bool is_connected_to_ap() const;

            /// Returns a value indicating if the required settings are set.
            /// \return true or false.
            [[nodiscard]] bool is_configured() const
            {
                return host_name.length() > 0 && ssid.length() > 0 && password.length() > 0;
            }

            static void wifi_event_callback(void* event_handler_arg,
                                            esp_event_base_t event_base,
                                            int32_t event_id,
                                            void* event_data);

            std::string get_mac_address() override;

            [[nodiscard]] static bool get_local_mac_address(std::array<uint8_t, 6>& m);

            [[nodiscard]] static uint32_t get_local_ip();

            /// Start providing an access point
            /// \param max_conn maximum number of clients to connect to this AP
            void start_softap(uint8_t max_conn = 1) override;

            // Initiates smartconfig wifi.
            void start_smartconfig() override;

            void start_provision() override;

            // Get is SmartConfig
            bool is_smart_config() override { return is_smartconfig; }
            
            // Get status config wifi
            std::tuple<bool, std::string, std::string> get_config() override;

            // Get rssi
            int get_rssi() override;
        private:
            bool connect() override;

            void close_if();

            static void publish_status(bool connected, bool ip_changed);

            bool auto_connect_to_ap = false;
            bool connected_to_ap = false;
            volatile bool is_smartconfig = false;

            std::string host_name = "Smooth-Wifi";
            std::string ssid{};

            std::string password{};
            static struct esp_ip4_addr ip;

            esp_netif_t* interface{ nullptr };
            esp_event_handler_instance_t instance_wifi_event{};
            esp_event_handler_instance_t instance_ip_event{};
    };
}

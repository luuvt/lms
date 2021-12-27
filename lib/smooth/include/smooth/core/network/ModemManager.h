#pragma once

#include <string>
#include <array>
#include "INetworkManager.h"
#include "smooth/core/ipc/IEventListener.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <esp_event_base.h>
#include <esp_modem.h>
#pragma GCC diagnostic pop

namespace smooth::core::network
{
  class ModemManager : public INetworkManager
  {
    static const char *TAG;
    esp_modem_dte_config_t config;

    modem_dte_t *dte;
    modem_dce_t *dce;

    bool keepRunning = true;
    public:
      ModemManager();

      void setConfig(const esp_modem_dte_config_t &config) override;

      void init() override;
      void stop() override;
      bool connect() override;
      bool sendSms(const std::string& phone, const std::string& text);
      bool listSms() override;
      bool readSms(unsigned int index) override;
      bool deleteSms(unsigned int index) override;
      bool deleteAllSms() override;
      bool queryNetRegistrationInfo() override;

      std::string getImei() override { return imei;};
      std::string getImsi() override { return imsi;};
      std::string getCcid() override { return ccid;};
      
    private:
      std::string imei{};
      std::string imsi{};
      std::string ccid{};
      void onStop();
      void disconnect();
      void publish_status(bool connected, bool ip_changed);
      
      bool setFormatSms();
      bool is_set_format = false;

      static esp_err_t defaultHandle(modem_dce_t *dce, const char *line);
      static esp_err_t handleCmgs(modem_dce_t *dce, const char *line);

      static void modemEventHandlerStatic(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
  };
}
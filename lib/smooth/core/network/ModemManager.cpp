#include "smooth/core/network/ModemManager.h"
#include "smooth/core/network/NetworkStatus.h"
#include "smooth/core/network/ModemMessage.h"
#include "smooth/core/ipc/Publisher.h"
#include "smooth/core/util/copy_min_to_buffer.h"
#include "smooth/core/logging/log.h"
#include <esp_modem.h>
#include <esp_modem_compat.h>
#include <n58.h>
#include "freertos/event_groups.h"


static EventGroupHandle_t modem_event_group;
const int STOP_RUNNING = BIT0;

namespace smooth::core::network
{
  const char *ModemManager::TAG = "ModemManager";
  #define MODEM_SMS_MAX_LENGTH (128)
  #define MODEM_COMMAND_TIMEOUT_SMS_MS (5000)
  #define MODEM_COMMAND_TIMEOUT_COMMON (300)
  #define MODEM_PROMPT_TIMEOUT_MS (10)

  ModemManager::ModemManager()
  {
  }

  void ModemManager::setConfig(const esp_modem_dte_config_t &config)
  {
    this->config = config;
  }

  void ModemManager::init()
  {
    esp_netif_init();

    modem_event_group = xEventGroupCreate(); 
    dte = esp_modem_dte_init(&config);
    
    /* Register event handler */
    ESP_ERROR_CHECK(esp_modem_add_event_handler(dte, ModemManager::modemEventHandlerStatic, this));
  }

  bool ModemManager::connect()
  {
    /* create dce object */
    dce = n58_init(dte);
    if(!dce){
        Log::error(TAG, "Initialization of N58 failed.");
        return false;
    }

    ESP_ERROR_CHECK(dce->set_flow_ctrl(dce, config.flow_control));
    ESP_ERROR_CHECK(dce->store_profile(dce));
    /* Print Module ID, Operator, IMEI, IMSI */
    Log::info(TAG, "Module: {}", dce->name);
    Log::info(TAG, "Operator: {}", dce->oper);
    this->imei = &dce->imei[0];
    this->imsi = &dce->imsi[0];
    this->ccid.assign(std::string(dce->ccid), 2 , 16);
    Log::info(TAG, "IMEI: {}", this->imei);
    Log::info(TAG, "IMSI: {}", this->imsi);
    Log::info(TAG, "CCID: {}", this->ccid);

    /* Get signal quality */
    uint32_t rssi = 0, ber = 0;
    ESP_ERROR_CHECK(dce->get_signal_quality(dce, &rssi, &ber));
    Log::info(TAG, "rssi: {0}, ber: {1}", rssi, ber);

    /* Setup PPP environment */
    auto result = esp_modem_setup_ppp(dte);

    return result == ESP_OK;
  }

  void ModemManager::disconnect()
  {
    /* Exit PPP mode */
    ESP_ERROR_CHECK(esp_modem_exit_ppp(dte));
  }

  void ModemManager::stop()
  {
    onStop();
  }

  void ModemManager::onStop()
  {
    disconnect();

    /* Power down module */
    ESP_ERROR_CHECK(dce->power_down(dce));
    Log::info(TAG, "Power down");
    ESP_ERROR_CHECK(dce->deinit(dce));
    ESP_ERROR_CHECK(dte->deinit(dte));
  }

  bool ModemManager::setFormatSms()
  {
    /* Set text mode */
    if (dte->send_cmd(dte, "AT+CMGF=1\r", MODEM_COMMAND_TIMEOUT_DEFAULT) != ESP_OK) {
      Log::error(TAG, "send command failed");
      return ESP_FAIL;
    }
    if (dce->state != MODEM_STATE_SUCCESS) {
      Log::error(TAG, "set message format failed");
      return ESP_FAIL;
    }
    Log::debug(TAG, "set message format ok");

    /* Specify character set */
    dce->handle_line = ModemManager::defaultHandle;
    if (dte->send_cmd(dte, "AT+CSCS=\"GSM\"\r", MODEM_COMMAND_TIMEOUT_DEFAULT) != ESP_OK) {
      Log::error(TAG, "send command failed");
      return ESP_FAIL;
    }
    if (dce->state != MODEM_STATE_SUCCESS) {
      Log::error(TAG, "set character set failed");
      return ESP_FAIL;
    }
    Log::debug(TAG, "set character set ok");
    return ESP_OK;
  }

  bool ModemManager::sendSms(const std::string& phone, const std::string& text)
  {
    modem_dte_t *dte = dce->dte;
    dce->handle_line = ModemManager::defaultHandle;
    if (!is_set_format) {
      if (setFormatSms() == ESP_OK) {
        is_set_format = true;
      }
      else {
        return ESP_FAIL;
      }
    }
    /* send message */
    char command[MODEM_SMS_MAX_LENGTH] = {0};
    int length = snprintf(command, MODEM_SMS_MAX_LENGTH, "AT+CMGS=\"%s\"\r", phone.c_str());
    /* set phone number and wait for "> " */
    dte->send_wait(dte, command, static_cast<uint32_t>(length), "\r\n> ", MODEM_PROMPT_TIMEOUT_MS);
    /* end with CTRL+Z */
    snprintf(command, MODEM_SMS_MAX_LENGTH, "%s\x1A", text.c_str());
    dce->handle_line = ModemManager::handleCmgs;
    if (dte->send_cmd(dte, command, MODEM_COMMAND_TIMEOUT_SMS_MS) != ESP_OK) {
      Log::error(TAG, "send command failed");
      return ESP_FAIL;
    }
    if (dce->state != MODEM_STATE_SUCCESS) {
      Log::error(TAG, "send message failed");
      return ESP_FAIL;
    }
    Log::debug(TAG, "send message ok");
    return ESP_OK;
  }

  bool ModemManager::listSms()
  {
    modem_dte_t *dte = dce->dte;
    dce->handle_line = ModemManager::defaultHandle;
    
    if (!is_set_format) {
      if (setFormatSms() == ESP_OK) {
        is_set_format = true;
      }
      else {
        return ESP_FAIL;
      }
    }
    /* read all messages */
    dce->handle_line = ModemManager::handleCmgs;
    if (dte->send_cmd(dte, "AT+CMGL=\"ALL\"\r", MODEM_COMMAND_TIMEOUT_COMMON) != ESP_OK) {
      Log::error(TAG, "send command failed");
      return ESP_FAIL;
    }
    if (dce->state != MODEM_STATE_SUCCESS) {
      Log::error(TAG, "read all message failed");
      return ESP_FAIL;
    }
    Log::debug(TAG, "read all message ok");
    return ESP_OK;
  }

  bool ModemManager::readSms(unsigned int index)
  {
    modem_dte_t *dte = dce->dte;
    dce->handle_line = ModemManager::defaultHandle;
    if (!is_set_format) {
      if (setFormatSms() == ESP_OK) {
        is_set_format = true;
      }
      else {
        return ESP_FAIL;
      }
    }
    /* read message */
    char command[9] = {0};
    snprintf(command, 9, "AT+CMGR=%d\r", index);
    dce->handle_line = ModemManager::handleCmgs;
    if (dte->send_cmd(dte, command, MODEM_COMMAND_TIMEOUT_COMMON) != ESP_OK) {
      Log::error(TAG, "send command failed");
      return ESP_FAIL;
    }
    if (dce->state != MODEM_STATE_SUCCESS) {
      Log::error(TAG, "read message failed");
      return ESP_FAIL;
    }
    Log::debug(TAG, "read message ok");
    return ESP_OK;
  }

  bool  ModemManager::deleteSms(unsigned int index)
  {
    modem_dte_t *dte = dce->dte;
    dce->handle_line = ModemManager::defaultHandle;
    /* delete message */
    char command[9] = {0};
    snprintf(command, 9, "AT+CMGD=%d\r", index);
    dce->handle_line = ModemManager::handleCmgs;
    if (dte->send_cmd(dte, command, MODEM_COMMAND_TIMEOUT_SMS_MS) != ESP_OK) {
      Log::error(TAG, "send command failed");
      return ESP_FAIL;
    }
    if (dce->state != MODEM_STATE_SUCCESS) {
      Log::error(TAG, "delete message failed");
      return ESP_FAIL;
    }
    Log::debug(TAG, "delete message ok");
    return ESP_OK;
  }

  bool  ModemManager::deleteAllSms()
  {
    modem_dte_t *dte = dce->dte;
    dce->handle_line = ModemManager::defaultHandle;
    /* delete message */
    dce->handle_line = ModemManager::handleCmgs;
    if (dte->send_cmd(dte, "AT+CMGD=1,3\r", MODEM_COMMAND_TIMEOUT_COMMON) != ESP_OK) {
      Log::error(TAG, "send command failed");
      return ESP_FAIL;
    }
    if (dce->state != MODEM_STATE_SUCCESS) {
      Log::error(TAG, "delete message failed");
      return ESP_FAIL;
    }
    Log::debug(TAG, "delete message ok");
    return ESP_OK;
  }
  
  bool ModemManager::queryNetRegistrationInfo()
  {
    modem_dte_t *dte = dce->dte;
    dce->handle_line = ModemManager::defaultHandle;
    /* delete message */
    dce->handle_line = ModemManager::handleCmgs;
    if (dte->send_cmd(dte, "AT+NETMSG\r", MODEM_COMMAND_TIMEOUT_COMMON) != ESP_OK) {
      Log::error(TAG, "send command failed");
      return ESP_FAIL;
    }
    if (dce->state != MODEM_STATE_SUCCESS) {
      Log::error(TAG, "query network message failed");
      return ESP_FAIL;
    }
    Log::debug(TAG, "query network message ok");
    return ESP_OK;
  }

  void ModemManager::publish_status(bool connected, bool ip_changed)
  {
    network::NetworkStatus status(connected
                                  ? network::NetworkEvent::GOT_IP : network::NetworkEvent::DISCONNECTED,
                                  ip_changed);
    core::ipc::Publisher<network::NetworkStatus>::publish(status);
  }

  void ModemManager::modemEventHandlerStatic(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
  {
    ModemManager* modem = reinterpret_cast<ModemManager*>(event_handler_arg);

    switch (event_id) {
      case MODEM_EVENT_PPP_START:
        Log::info(TAG, "Modem PPP Started");
        break;
      case MODEM_EVENT_PPP_CONNECT:
      {
          Log::info(TAG, "Modem Connect to PPP Server");
          ppp_client_ip_info_t *ipinfo = reinterpret_cast<ppp_client_ip_info_t *>(event_data);
          Log::info(TAG, "~~~~~~~~~~~~~~");
          // Log::info(TAG, "IP          : " IPSTR, IP2STR(&ipinfo->ip));
          // Log::info(TAG, "Netmask     : " IPSTR, IP2STR(&ipinfo->netmask));
          // Log::info(TAG, "Gateway     : " IPSTR, IP2STR(&ipinfo->gw));
          // Log::info(TAG, "Name Server1: " IPSTR, IP2STR(&ipinfo->ns1));
          // Log::info(TAG, "Name Server2: " IPSTR, IP2STR(&ipinfo->ns2));
          Log::info(TAG, "~~~~~~~~~~~~~~");
          modem->publish_status(true, true);
      }
          break;
      case MODEM_EVENT_PPP_DISCONNECT:
          Log::info(TAG, "Modem Disconnect from PPP Server");
          modem->publish_status(false, true);
          break;
      case MODEM_EVENT_PPP_STOP:
      {
          Log::info(TAG, "Modem PPP Stopped");
          modem->publish_status(false, true);
          modem->onStop();
      }
          break;
      case MODEM_EVENT_UNKNOWN:
          // Log::warning(TAG, "Unknow line received: {}", (char *)event_data);
          break;
      default:
          break;
    }
  }

  esp_err_t ModemManager::defaultHandle(modem_dce_t *dce, const char *line)
  {
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
      err = esp_modem_process_command_done(dce, MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
      err = esp_modem_process_command_done(dce, MODEM_STATE_FAIL);
    }
    return err;
  }

  esp_err_t ModemManager::handleCmgs(modem_dce_t *dce, const char *line)
  {
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
      err = esp_modem_process_command_done(dce, MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
      err = esp_modem_process_command_done(dce, MODEM_STATE_FAIL);
    } else if (!strncmp(line, "+CMGS", strlen("+CMGS"))) {
      err = ESP_OK;
    } else if (!strncmp(line, "+CMGR", strlen("+CMGR"))) { // read sms
      core::ipc::Publisher<network::ModemMessage>::publish(
        network::ModemMessage(line)
      );
      err = ESP_OK;
    } else if (!strncmp(line, "+CMGL", strlen("+CMGL"))) { // read list sms
      core::ipc::Publisher<network::ModemMessage>::publish(
        network::ModemMessage(line)
      );
      err = ESP_OK;
    } else if (!strncmp(line, "+NETMSG", strlen("+NETMSG"))) { // query network registration information
      core::ipc::Publisher<network::ModemMessage>::publish(
        network::ModemMessage(line)
      );
      err = ESP_OK;
    }
    return err;
  }
}
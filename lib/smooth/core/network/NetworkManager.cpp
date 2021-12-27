#include "smooth/core/network/NetworkManager.h"
#include "smooth/core/network/ModemManager.h"
#include "smooth/core/network/Wifi.h"

namespace smooth::core::network
{
  std::shared_ptr<INetworkManager> NetworkManager::getManager(const std::string& mode)
  {
    if (mode == "wifi") {
      return std::make_shared<network::Wifi>();
    }
    else {
      return std::make_shared<network::ModemManager>();
    }
  };
}
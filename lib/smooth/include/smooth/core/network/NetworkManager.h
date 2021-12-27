#pragma once

#include <string>
#include <memory>
#include "INetworkManager.h"

namespace smooth::core::network
{
    class NetworkManager
    {
        public:
        NetworkManager() {};
        static std::shared_ptr<INetworkManager> getManager(const std::string& mode);
    };
}
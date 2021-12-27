#pragma once

#include <string>

namespace smooth::core::network
{
    class ModemMessage
    {
        public:
            ModemMessage() = default;

            explicit ModemMessage(const std::string& buf)
                    : buffer(buf)
            {
            }

            const std::string get_buffer() const
            {
                return buffer;
            }

        private:
            std::string buffer;
    };
}

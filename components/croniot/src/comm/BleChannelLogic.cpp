#include "BleChannelLogic.h"

#include <cctype>

namespace croniot {
namespace BleChannelLogic {

bool parseStaticPasskey(const std::string& password, uint32_t& outPasskey) {
    if (password.size() != 6) return false;

    uint32_t passkey = 0;
    for (char c : password) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        passkey = (passkey * 10) + static_cast<uint32_t>(c - '0');
    }

    if (passkey > 999999) return false;
    outPasskey = passkey;
    return true;
}

uint32_t djb2Hash(const std::string& s) {
    uint32_t hash = 5381;
    for (unsigned char c : s) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

} // namespace BleChannelLogic
} // namespace croniot

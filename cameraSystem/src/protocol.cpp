#include "../include/protocol.hpp"
#include <cstring>

namespace Protocol {
    std::vector<uint8_t> serializeHeader(const MessageHeader& header) {
        std::vector<uint8_t> buf(sizeof(MessageHeader));
        std::memcpy(buf.data(), &header, sizeof(MessageHeader));
        return buf;
    }

    bool parseHeader(const std::vector<uint8_t>& data, MessageHeader& header) {
        if (data.size() < sizeof(MessageHeader)) return false;
        std::memcpy(&header, data.data(), sizeof(MessageHeader));
        return true;
    }
}

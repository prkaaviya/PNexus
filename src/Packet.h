#ifndef PACKET_H
#define PACKET_H

#include <array>
#include <cstdint>
#include <unistd.h>
#include <vector>

const int MAX_BUFFER_SIZE = 50 * 1000; // 50kb
const uint8_t PKT_VERSION = 1;

enum class packetType : uint8_t { TEXT, FILE };

struct Packet {
    uint8_t version;
    uint32_t sAddress; // IPV4 Address of original sender
    uint16_t sPort;    // Port of original sender
    uint32_t tAddress; // IPV4 Address of final receiver
    uint16_t tPort;    // Port of final receiver
    packetType type;
    uint16_t fragmentNumber;
    uint16_t fragmentCount;
    uint32_t errorCorrectionCode;

    std::array<uint8_t, MAX_BUFFER_SIZE> data;

    Packet();
    Packet(uint32_t sAddr, uint16_t sPort, uint32_t tAddr, uint16_t tPort, packetType type);

    std::vector<uint8_t> serialize();
    static Packet deserialize(const std::vector<uint8_t> &buffer);

    void computeCRC();
    bool verifyCRC();

private:
    static uint32_t calculateCRC(const std::vector<uint8_t> &data);
};

#endif
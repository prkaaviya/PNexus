#include <cstring>

#include "Packet.h"

Packet::Packet() : version{PKT_VERSION} { std::fill(data.begin(), data.end(), 0); }

Packet::Packet(uint32_t sAddr, uint16_t sPort, uint32_t tAddr, uint16_t tPort, packetType type)
    : version{PKT_VERSION}, sAddress{sAddr}, sPort{sPort}, tAddress{tAddr}, tPort{tPort},
      type{type}, errorCorrectionCode(0) {
  std::fill(data.begin(), data.end(), 0);
}

std::vector<uint8_t> Packet::serialize() {
  std::vector<uint8_t> buffer;
  buffer.push_back(version);

  const uint8_t *sAddress_ptr = reinterpret_cast<const uint8_t *>(&sAddress);
  buffer.insert(buffer.end(), sAddress_ptr, sAddress_ptr + sizeof(sAddress));

  const uint8_t *sPort_ptr = reinterpret_cast<const uint8_t *>(&sPort);
  buffer.insert(buffer.end(), sPort_ptr, sPort_ptr + sizeof(sPort));

  const uint8_t *tAddress_ptr = reinterpret_cast<const uint8_t *>(&tAddress);
  buffer.insert(buffer.end(), tAddress_ptr, tAddress_ptr + sizeof(tAddress));

  const uint8_t *tPort_ptr = reinterpret_cast<const uint8_t *>(&tPort);
  buffer.insert(buffer.end(), tPort_ptr, tPort_ptr + sizeof(tPort));

  uint8_t type_byte = static_cast<uint8_t>(type);
  buffer.push_back(type_byte);

  const uint8_t *fragmentNumber_ptr = reinterpret_cast<const uint8_t *>(&fragmentNumber);
  buffer.insert(buffer.end(), fragmentNumber_ptr, fragmentNumber_ptr + sizeof(fragmentNumber));

  const uint8_t *fragmentCount_ptr = reinterpret_cast<const uint8_t *>(&fragmentCount);
  buffer.insert(buffer.end(), fragmentCount_ptr, fragmentCount_ptr + sizeof(fragmentCount));

  const uint8_t *errorCorrectionCode_ptr = reinterpret_cast<const uint8_t *>(&errorCorrectionCode);
  buffer.insert(buffer.end(), errorCorrectionCode_ptr,
                errorCorrectionCode_ptr + sizeof(errorCorrectionCode));

  buffer.insert(buffer.end(), data.begin(), data.end());

  return buffer;
}

Packet Packet::deserialize(const std::vector<uint8_t> &buffer) {
  Packet packet;
  size_t offset = 0;

  packet.version = buffer[offset];
  offset += sizeof(packet.version);

  std::memcpy(&packet.sAddress, &buffer[offset], sizeof(packet.sAddress));
  offset += sizeof(packet.sAddress);

  std::memcpy(&packet.sPort, &buffer[offset], sizeof(packet.sPort));
  offset += sizeof(packet.sPort);

  std::memcpy(&packet.tAddress, &buffer[offset], sizeof(packet.tAddress));
  offset += sizeof(packet.tAddress);

  std::memcpy(&packet.tPort, &buffer[offset], sizeof(packet.tPort));
  offset += sizeof(packet.tPort);

  packet.type = static_cast<packetType>(buffer[offset]);
  offset += sizeof(packet.type);

  std::memcpy(&packet.fragmentNumber, &buffer[offset], sizeof(packet.fragmentNumber));
  offset += sizeof(packet.fragmentNumber);

  std::memcpy(&packet.fragmentCount, &buffer[offset], sizeof(packet.fragmentCount));
  offset += sizeof(packet.fragmentCount);

  std::memcpy(&packet.errorCorrectionCode, &buffer[offset], sizeof(packet.errorCorrectionCode));
  offset += sizeof(packet.errorCorrectionCode);

  std::fill(packet.data.begin(), packet.data.end(), 0);
  std::memcpy(packet.data.data(), &buffer[offset], packet.data.size());
  return packet;
}

uint32_t Packet::calculateCRC(const std::vector<uint8_t> &data) {
  uint32_t crc = 0xFFFFFFFF;
  for (auto byte : data) {
    crc ^= byte;
    for (int i = 0; i < 8; ++i) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xEDB88320;
      else
        crc >>= 1;
    }
  }
  return ~crc;
}

void Packet::computeCRC() {
  auto serializedData = serialize();
  errorCorrectionCode = calculateCRC(serializedData);
}

bool Packet::verifyCRC() {
  auto serializedData = serialize();
  uint32_t computedCRC = calculateCRC(serializedData);
  return computedCRC == errorCorrectionCode;
}

#pragma once
#include <cstdint>
#include <vector>

namespace esphome {
namespace sphero_bb8 {

// Sphero v1 packet checksum: ~(sum(DID + CID + SEQ + DLEN + DATA) % 256)
// DLEN counts DATA plus the checksum byte itself, hence (data.size() + 1).
// TX-side helper: matches gobot craftPacket (sums from index 2).
inline uint8_t packet_checksum(uint8_t did, uint8_t cid, uint8_t seq,
                               const std::vector<uint8_t> &data) {
  uint32_t sum = did + cid + seq + (data.size() + 1);
  for (uint8_t b : data) sum += b;
  return ~(sum % 256) & 0xFF;
}

// RX-side sibling: response/async frames carry no DID/CID, so callers pass the
// header bytes between SOP2 and the payload (MRP, SEQ, DLEN for sync frames;
// ID code and 16-bit DLEN for async frames) as `fields`, plus the payload.
// Formula identical to packet_checksum — see tests/host/test_protocol.cpp,
// which asserts the identity at runtime.
inline uint8_t checksum_bytes(const std::vector<uint8_t> &fields,
                              const std::vector<uint8_t> &payload) {
  uint32_t sum = 0;
  for (uint8_t b : fields) sum += b;
  for (uint8_t b : payload) sum += b;
  return static_cast<uint8_t>(~(sum % 256) & 0xFF);
}

}  // namespace sphero_bb8
}  // namespace esphome

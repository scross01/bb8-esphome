// Host-side protocol tests: pure C++, no ESPHome includes.
// Build & run:
//   g++ -std=c++17 -Icomponents -Itests/host tests/host/test_protocol.cpp -o /tmp/test_protocol && /tmp/test_protocol
//
// Covers:
//   1. packet_checksum (TX): hand-computed known vectors — typical, empty, wraparound.
//   2. checksum_bytes (RX): direct vector case.
//   3. Formula identity: packet_checksum(did,cid,seq,data) ==
//      checksum_bytes({did,cid,seq,DLEN}, data) for a spread of inputs — the
//      property the RX validation gate in process_packet_ relies on.

#include "sphero_bb8/protocol.h"

#include <cstdint>
#include <cstdio>
#include <vector>

static int failures = 0;
static int checks = 0;

#define CHECK_EQ(actual, expected, label)                                              \
  do {                                                                                 \
    ++checks;                                                                          \
    if ((actual) != (expected)) {                                                      \
      ++failures;                                                                      \
      std::printf("FAIL %s: got 0x%02X, expected 0x%02X\n", label,                     \
                  (unsigned)(uint8_t)(actual), (unsigned)(uint8_t)(expected));         \
    }                                                                                  \
  } while (0)

int main() {
  // ---- Case 1: typical packet. Hand arithmetic (recomputed independently; the
  // plan's own constant had a hex/decimal mismatch — 120 is 0x78, not 0x7C):
  //   did=0x02 (2) + cid=0x30 (48) + seq=0x01 (1) + DLEN=data.size()+1=5
  //   + data {0x30, 0x00, 0x1E, 0x01} = 48+0+30+1
  //   sum = 2+48+1+5+48+0+30+1 = 135; 255-135 = 120 = 0x78
  {
    const std::vector<uint8_t> data = {0x30, 0x00, 0x1E, 0x01};
    CHECK_EQ(esphome::sphero_bb8::packet_checksum(0x02, 0x30, 0x01, data), 0x78,
             "packet_checksum typical");
  }

  // ---- Case 2: empty data.
  //   sum = 0+1+1+DLEN(0+1=1) = 3; 255-3 = 252 = 0xFC
  {
    const std::vector<uint8_t> data = {};
    CHECK_EQ(esphome::sphero_bb8::packet_checksum(0x00, 0x01, 0x01, data), 0xFC,
             "packet_checksum empty");
  }

  // ---- Case 3: wraparound past 256.
  //   did=0xFF(255) + cid=0xFF(255) + seq=0xFF(255) + DLEN=7
  //   + data = six bytes of 0xFF (6*255=1530)
  //   sum = 255+255+255+7+1530 = 2302; 2302 % 256 = 254; 255-254 = 1 = 0x01
  {
    const std::vector<uint8_t> data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    CHECK_EQ(esphome::sphero_bb8::packet_checksum(0xFF, 0xFF, 0xFF, data), 0x01,
             "packet_checksum wraparound");
  }

  // ---- Case 4: checksum_bytes direct vector.
  //   fields {0x02, 0x30, 0x01, 0x05} = 2+48+1+5 = 56; payload {0x30,0x00,0x1E,0x01} = 79
  //   sum = 135; 255-135 = 120 = 0x78
  {
    const std::vector<uint8_t> fields = {0x02, 0x30, 0x01, 0x05};
    const std::vector<uint8_t> payload = {0x30, 0x00, 0x1E, 0x01};
    CHECK_EQ(esphome::sphero_bb8::checksum_bytes(fields, payload), 0x78,
             "checksum_bytes direct");
  }

  // ---- Case 5: TX/RX formula identity across a spread of inputs.
  // packet_checksum sums did+cid+seq+(size+1)+data; checksum_bytes sums
  // fields+payload. With fields = {did, cid, seq, data.size()+1} the sums are
  // term-by-term identical, so the bytes must match for every input.
  {
    const std::vector<std::vector<uint8_t>> payloads = {
        {}, {0x00}, {0x01, 0x02}, {0xFF}, {0x80, 0x7F, 0x01}, {0xFF, 0xFF, 0xFF, 0xFF},
    };
    const uint8_t ids[] = {0x00, 0x01, 0x02, 0xFF};
    for (uint8_t did : ids) {
      for (uint8_t cid : ids) {
        for (uint8_t seq : ids) {
          for (const auto &data : payloads) {
            const uint8_t tx = esphome::sphero_bb8::packet_checksum(did, cid, seq, data);
            const std::vector<uint8_t> fields = {did, cid, seq,
                                                 static_cast<uint8_t>(data.size() + 1)};
            const uint8_t rx = esphome::sphero_bb8::checksum_bytes(fields, data);
            ++checks;
            if (tx != rx) {
              ++failures;
              std::printf("FAIL identity: did=0x%02X cid=0x%02X seq=0x%02X size=%zu "
                          "tx=0x%02X rx=0x%02X\n",
                          did, cid, seq, data.size(), tx, rx);
            }
          }
        }
      }
    }
  }

  if (failures > 0) {
    std::printf("%d of %d protocol checks FAILED\n", failures, checks);
    return 1;
  }
  std::printf("All %d protocol tests passed\n", checks);
  return 0;
}

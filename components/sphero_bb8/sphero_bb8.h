#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include <vector>

namespace esphome {
namespace sphero_bb8 {

namespace espbt = esphome::esp32_ble_tracker;

class SpheroBB8 : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_rgb(uint8_t r, uint8_t g, uint8_t b);
  void set_back_led(uint8_t brightness);

  bool is_ready() const { return state_ == READY; }

 protected:
  void send_packet(uint8_t did, uint8_t cid, const std::vector<uint8_t> &data, bool wait_for_response = false);
  uint8_t calculate_checksum(uint8_t did, uint8_t cid, uint8_t seq, const std::vector<uint8_t> &data);

  enum State {
    DISCONNECTED,
    CONNECTING,
    SUBSCRIBE,
    ANTI_DOS,
    TX_POWER,
    WAKE,
    READY,
  } state_{DISCONNECTED};

  uint16_t char_handle_anti_dos_{0};
  uint16_t char_handle_tx_power_{0};
  uint16_t char_handle_wake_{0};
  uint16_t char_handle_commands_{0};
  uint16_t char_handle_responses_{0};

  uint32_t last_state_change_{0};
  uint32_t last_write_request_{0};
  uint32_t last_packet_sent_{0};
  uint8_t sequence_number_{0};
  bool write_in_progress_{false};

  uint8_t target_r_{0}, target_g_{0}, target_b_{0};
  uint8_t current_r_{0}, current_g_{0}, current_b_{0};
  uint8_t target_back_brightness_{0};
  uint8_t current_back_brightness_{0};
};

}  // namespace sphero_bb8
}  // namespace esphome

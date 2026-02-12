#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"

#include <vector>

namespace esphome {
namespace sphero_bb8 {

namespace espbt = esphome::esp32_ble_tracker;

class SpheroBB8Light;

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

  void set_status_sensor(text_sensor::TextSensor *sensor) { status_sensor_ = sensor; }
  void set_battery_sensor(sensor::Sensor *sensor) { battery_sensor_ = sensor; }
  void set_version_sensor(text_sensor::TextSensor *sensor) { version_sensor_ = sensor; }
  void set_charging_status_sensor(text_sensor::TextSensor *sensor) { charging_status_sensor_ = sensor; }
  void set_collision_sensor(binary_sensor::BinarySensor *sensor) { collision_sensor_ = sensor; }
  void set_collision_speed_sensor(sensor::Sensor *sensor) { collision_speed_sensor_ = sensor; }
  void set_collision_magnitude_sensor(sensor::Sensor *sensor) { collision_magnitude_sensor_ = sensor; }
  
  void set_auto_connect(bool auto_connect) { auto_connect_ = auto_connect; }
  void set_enabled(bool enabled) { enabled_ = enabled; }
  void register_light(SpheroBB8Light *light) { lights_.push_back(light); }

  void connect();
  void disconnect();
  void center_head();

  bool is_ready() const { return state_ == READY; }

 protected:
  uint8_t send_packet(uint8_t did, uint8_t cid, const std::vector<uint8_t> &data, bool wait_for_response = false);
  uint8_t calculate_checksum(uint8_t did, uint8_t cid, uint8_t seq, const std::vector<uint8_t> &data);
  void update_status_sensor_(const std::string &status);
  void force_lights_off_();
  void handle_packet_(const std::vector<uint8_t> &data);
  void process_packet_(const std::vector<uint8_t> &packet);
  void configure_collision_detection_();

  enum State {
    DISCONNECTED,
    CONNECTING,
    SUBSCRIBE,
    ANTI_DOS,
    TX_POWER,
    WAKE,
    READY_STABILIZE,
    READY,
    DISABLING,
  } state_{DISCONNECTED};

  uint16_t char_handle_anti_dos_{0};
  uint16_t char_handle_tx_power_{0};
  uint16_t char_handle_wake_{0};
  uint16_t char_handle_commands_{0};
  uint16_t char_handle_responses_{0};

  uint32_t last_state_change_{0};
  uint32_t last_write_request_{0};
  uint32_t last_packet_sent_{0};
  uint32_t last_power_check_{0};
  uint32_t last_collision_time_{0};
  uint8_t sequence_number_{0};
  bool write_in_progress_{false};
  bool version_requested_{false};
  bool power_notify_enabled_{false};
  bool collision_config_sent_{false};
  uint8_t version_req_seq_{0};
  uint8_t power_req_seq_{0};

  uint8_t target_r_{0}, target_g_{0}, target_b_{0};
  uint8_t current_r_{0}, current_g_{0}, current_b_{0};
  uint8_t target_back_brightness_{0};
  uint8_t current_back_brightness_{0};

  text_sensor::TextSensor *status_sensor_{nullptr};
  sensor::Sensor *battery_sensor_{nullptr};
  text_sensor::TextSensor *version_sensor_{nullptr};
  text_sensor::TextSensor *charging_status_sensor_{nullptr};
  binary_sensor::BinarySensor *collision_sensor_{nullptr};
  sensor::Sensor *collision_speed_sensor_{nullptr};
  sensor::Sensor *collision_magnitude_sensor_{nullptr};

  std::vector<SpheroBB8Light *> lights_;
  std::vector<uint8_t> packet_buffer_;
  bool auto_connect_{false};
  bool enabled_{true};
  std::string last_status_str_{""};
};

class SpheroBB8Button : public button::Button, public Component {
 public:
  void set_parent(SpheroBB8 *parent) { parent_ = parent; }
  void set_type(const std::string &type) { type_ = type; }
  void press_action() override {
    if (this->type_ == "CONNECT") {
      this->parent_->connect();
    } else if (this->type_ == "DISCONNECT") {
      this->parent_->disconnect();
    } else if (this->type_ == "CENTER_HEAD") {
      this->parent_->center_head();
    }
  }

 protected:
  SpheroBB8 *parent_;
  std::string type_;
};

}  // namespace sphero_bb8
}  // namespace esphome

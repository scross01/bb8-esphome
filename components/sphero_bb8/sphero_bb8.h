#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"

#include <vector>
#include "script_executor.h"

namespace esphome {
namespace sphero_bb8 {

namespace espbt = esphome::esp32_ble_tracker;

// Checksum helpers live in protocol.h (dependency-free, host-testable):
// packet_checksum (TX) and checksum_bytes (RX).

class SpheroBB8Light;

class SpheroBB8 : public Component, public ble_client::BLEClientNode {
  friend class ScriptExecutor;
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_rgb(uint8_t r, uint8_t g, uint8_t b);
  void set_back_led(uint8_t brightness);

  // Internal methods for ScriptExecutor to use without stopping scripts
  void set_rgb_internal(uint8_t r, uint8_t g, uint8_t b) {
    target_r_ = r; target_g_ = g; target_b_ = b;
  }
  void set_back_led_internal(uint8_t brightness) {
    target_back_brightness_ = brightness;
  }

  // Light-entity path: does NOT stop scripts (fades tick continuously; stopping
  // on every write tore scripts down mid-fade). Manual-command-stops-script
  // policy remains on set_rgb/set_back_led below.
  void set_rgb_from_light(uint8_t r, uint8_t g, uint8_t b);
  void set_back_led_from_light(uint8_t brightness);

  // Current targets (where the droid's LEDs are heading) for UI re-sync
  uint8_t target_r() const { return target_r_; }
  uint8_t target_g() const { return target_g_; }
  uint8_t target_b() const { return target_b_; }
  uint8_t target_back_brightness() const { return target_back_brightness_; }

  void set_status_sensor(text_sensor::TextSensor *sensor) { status_sensor_ = sensor; }
  void set_battery_sensor(sensor::Sensor *sensor) { battery_sensor_ = sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *sensor) { battery_voltage_sensor_ = sensor; }
  void set_version_sensor(text_sensor::TextSensor *sensor) { version_sensor_ = sensor; }
  void set_charging_status_sensor(text_sensor::TextSensor *sensor) { charging_status_sensor_ = sensor; }
  void set_collision_sensor(binary_sensor::BinarySensor *sensor) { collision_sensor_ = sensor; }
  void set_collision_speed_sensor(sensor::Sensor *sensor) { collision_speed_sensor_ = sensor; }
  void set_collision_magnitude_sensor(sensor::Sensor *sensor) { collision_magnitude_sensor_ = sensor; }
  
  void set_auto_connect(bool auto_connect) { auto_connect_ = auto_connect; }
  void set_wake_wait_ms(uint32_t wait_ms) { wake_wait_ms_ = wait_ms; }
  void set_stabilize_wait_ms(uint32_t wait_ms) { stabilize_wait_ms_ = wait_ms; }
  void set_restore_lights_on_connect(bool b) { restore_lights_on_connect_ = b; }
  void set_enabled(bool enabled) { enabled_ = enabled; }
  void register_light(SpheroBB8Light *light) { lights_.push_back(light); }

  void connect();
  void disconnect();
  void center_head();
  
  // Script methods
  void add_script(const Script &script);
  bool execute_script(const std::string &name);
  void stop_script();
  bool is_script_idle() const { return script_executor_.is_idle(); }
  bool is_script_running() const { return script_executor_.is_running(); }
  // Push the droid's actual LED targets into the HA light entities (UI re-sync:
  // after a script finishes, and after reconnect since force_lights_off_ turned
  // the UI off on disconnect while targets survive)
  void publish_led_state_to_lights_();

  bool is_ready() const { return state_ == READY; }
  
  // Make send_packet accessible to ScriptExecutor
  uint8_t send_packet(uint8_t did, uint8_t cid, const std::vector<uint8_t> &data, bool wait_for_response = false);

 protected:
  uint8_t calculate_checksum(uint8_t did, uint8_t cid, uint8_t seq, const std::vector<uint8_t> &data);
  void update_status_sensor_(const std::string &status);
  void force_lights_off_();
  void handle_packet_(const std::vector<uint8_t> &data);
  void process_packet_(const std::vector<uint8_t> &packet);
  void configure_collision_detection_();
  static float battery_level_from_voltage_(float voltage);  // NaN if implausible

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
  bool was_charging_{false};
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
  sensor::Sensor *battery_voltage_sensor_{nullptr};
  text_sensor::TextSensor *version_sensor_{nullptr};
  text_sensor::TextSensor *charging_status_sensor_{nullptr};
  binary_sensor::BinarySensor *collision_sensor_{nullptr};
  sensor::Sensor *collision_speed_sensor_{nullptr};
  sensor::Sensor *collision_magnitude_sensor_{nullptr};

  std::vector<SpheroBB8Light *> lights_;
  std::vector<uint8_t> packet_buffer_;
  bool auto_connect_{false};
  uint32_t wake_wait_ms_{200};
  uint32_t stabilize_wait_ms_{500};
  bool restore_lights_on_connect_{true};
  bool enabled_{true};
  std::string last_status_str_{""};
  
  // Script execution
  ScriptExecutor script_executor_;
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

class SpheroBB8ScriptButton : public button::Button, public Component {
 public:
  void set_parent(SpheroBB8 *parent) { parent_ = parent; }
  void set_script_name(const std::string &script_name) { script_name_ = script_name; }
  void press_action() override {
    if (this->parent_ != nullptr) {
      this->parent_->execute_script(this->script_name_);
    }
  }

 protected:
  SpheroBB8 *parent_;
  std::string script_name_;
};

template<typename... Ts> class ExecuteScriptAction : public Action<Ts...> {
 public:
  ExecuteScriptAction() = default;
  TEMPLATABLE_VALUE(SpheroBB8 *, parent)
  TEMPLATABLE_VALUE(std::string, script_name)

  void play(Ts... x) override {
    auto *parent = this->parent_.value(x...);
    auto script_name = this->script_name_.value(x...);
    if (parent != nullptr) {
      parent->execute_script(script_name);
    }
  }
};

}  // namespace sphero_bb8
}  // namespace esphome

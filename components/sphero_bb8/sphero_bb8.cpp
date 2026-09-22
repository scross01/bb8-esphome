#include "sphero_bb8.h"
#include "sphero_bb8_light.h"
#include "esphome/core/log.h"
#include "script_executor.h"

#include <cmath>

namespace esphome {
namespace sphero_bb8 {

static const char *const TAG = "sphero_bb8";

static const char *const SERVICE_BLE_UUID = "22bb746f-2bb0-7554-2d6f-726568705327";
static const char *const SERVICE_CONTROL_UUID = "22bb746f-2ba0-7554-2d6f-726568705327";

static const char *const CHAR_ANTI_DOS_UUID = "22bb746f-2bbd-7554-2d6f-726568705327";
static const char *const CHAR_TX_POWER_UUID = "22bb746f-2bb2-7554-2d6f-726568705327";
static const char *const CHAR_WAKE_UUID = "22bb746f-2bbf-7554-2d6f-726568705327";
static const char *const CHAR_COMMANDS_UUID = "22bb746f-2ba1-7554-2d6f-726568705327";
static const char *const CHAR_RESPONSES_UUID = "22bb746f-2ba6-7554-2d6f-726568705327";

// Status text for the failed-service-search case (wrong MAC / non-Sphero device
// at that address). A constant because loop()'s not-connected branch must NOT
// overwrite it with "Connecting" — the two sites must agree exactly.
static const char *const STATUS_CONN_FAILED = "Connection failed: not a BB-8";

static const uint8_t DID_CORE = 0x00;
static const uint8_t DID_SPHERO = 0x02;
static const uint8_t CID_VERSION = 0x02;
static const uint8_t CID_GET_POWER_STATE = 0x20;
static const uint8_t CID_SET_POWER_NOTIFY = 0x21;
static const uint8_t CID_CONFIG_COLLISION = 0x12;
static const uint8_t CID_SET_SELF_LEVEL = 0x09;

void SpheroBB8::setup() {
  this->current_r_ = 0xFE;
  this->current_g_ = 0xFE;
  this->current_b_ = 0xFE;
  this->current_back_brightness_ = 0xFE;
  this->enabled_ = this->auto_connect_;
  this->parent()->set_enabled(this->enabled_);
  this->parent()->set_auto_connect(this->enabled_);
}

void SpheroBB8::connect() {
  ESP_LOGI(TAG, "Connect button pressed, enabling BB8 connection...");
  this->enabled_ = true;
  // Clears a previous "Connection failed" message immediately.
  this->update_status_sensor_("Connecting");
  this->parent()->set_enabled(true);
  this->parent()->set_auto_connect(true);
}

void SpheroBB8::disconnect() {
  ESP_LOGI(TAG, "Disconnect button pressed, disabling BB8 connection...");
  if (this->is_script_running()) {
    ESP_LOGI(TAG, "Disconnect requested, stopping motion script");
  }
  this->stop_script();
  this->enabled_ = false;
  this->parent()->set_auto_connect(false);
  this->version_requested_ = false;
  this->power_notify_enabled_ = false;
  this->collision_config_sent_ = false;
}

void SpheroBB8::loop() {
  uint32_t now = millis();

  if (this->write_in_progress_ && now - this->last_write_request_ > 1000) {
    ESP_LOGW(TAG, "Write timeout, resetting write_in_progress_");
    this->write_in_progress_ = false;
  }

  // Handle disabling sequence
  if (!this->enabled_) {
    if (this->state_ != DISCONNECTED && this->state_ != DISABLING && this->parent()->connected()) {
      ESP_LOGI(TAG, "Sending Sleep command before disconnect...");
      this->state_ = DISABLING;
      this->last_state_change_ = now;
      this->send_packet(0x00, 0x22, {0x00, 0x00, 0x00, 0x00, 0x00}, false);
      this->force_lights_off_();
    }

    if (this->state_ == DISABLING && now - this->last_state_change_ > 500) {
      ESP_LOGI(TAG, "Disconnecting from Sphero BB8...");
      this->parent()->set_enabled(false);
      this->state_ = DISCONNECTED;
    }

    if (this->state_ == DISCONNECTED && this->parent()->enabled) {
      this->parent()->set_enabled(false);
    }

    if (this->state_ == DISABLING) {
      this->update_status_sensor_("Disabling");
    } else {
      this->update_status_sensor_("Disconnected");
    }
    return;
  }

  if (!this->parent()->connected()) {
    this->state_ = DISCONNECTED;
    // Don't mask an explicit failure message with "Connecting"; it stays until
    // the next connect attempt (connect() republishes "Connecting").
    if (this->last_status_str_ != STATUS_CONN_FAILED) {
      this->update_status_sensor_("Connecting");
    }
    this->version_requested_ = false;
    this->power_notify_enabled_ = false;
    this->collision_config_sent_ = false;
    return;
  }

  if (this->write_in_progress_) {
    return;
  }

  if (this->state_ == SUBSCRIBE && this->char_handle_responses_ != 0) {
    this->update_status_sensor_("Initializing");
    ESP_LOGD(TAG, "Initialization State: Subscribing to responses");
    auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(),
                                                    this->parent()->get_remote_bda(),
                                                    this->char_handle_responses_);
    if (status != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register for notify: %d", status);
    }
    this->state_ = ANTI_DOS;
    this->last_state_change_ = now;
  }
  else if (this->state_ == ANTI_DOS && this->char_handle_anti_dos_ != 0) {
    if (now - this->last_state_change_ < this->wake_wait_ms_) return;

    this->update_status_sensor_("Initializing");
    ESP_LOGD(TAG, "Initialization State: Anti-DOS");
    uint8_t anti_dos_payload[] = {'0', '1', '1', 'i', '3'};
    this->write_in_progress_ = true;
    this->last_write_request_ = now;
    auto status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                          this->char_handle_anti_dos_, sizeof(anti_dos_payload), anti_dos_payload,
                                          ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (status != ESP_OK) {
      ESP_LOGE(TAG, "Failed to write Anti-DOS: %d", status);
      this->write_in_progress_ = false;
    }
    this->state_ = TX_POWER;
    this->last_state_change_ = now;
  }
  else if (this->state_ == TX_POWER && this->char_handle_tx_power_ != 0) {
    this->update_status_sensor_("Initializing");
    ESP_LOGD(TAG, "Initialization State: TX Power");
    uint8_t tx_power_payload[] = {7};
    this->write_in_progress_ = true;
    this->last_write_request_ = now;
    auto status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                          this->char_handle_tx_power_, sizeof(tx_power_payload), tx_power_payload,
                                          ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (status != ESP_OK) {
      ESP_LOGE(TAG, "Failed to write TX Power: %d", status);
      this->write_in_progress_ = false;
    }
    this->state_ = WAKE;
    this->last_state_change_ = now;
  }
  else if (this->state_ == WAKE && this->char_handle_wake_ != 0) {
    this->update_status_sensor_("Initializing");
    ESP_LOGD(TAG, "Initialization State: Wake");
    uint8_t wake_payload[] = {0x01};
    this->write_in_progress_ = true;
    this->last_write_request_ = now;
    auto status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                          this->char_handle_wake_, sizeof(wake_payload), wake_payload,
                                          ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (status != ESP_OK) {
      ESP_LOGE(TAG, "Failed to write Wake: %d", status);
      this->write_in_progress_ = false;
    }
    this->state_ = READY_STABILIZE;
    this->last_state_change_ = now;
  }
  else if (this->state_ == READY_STABILIZE) {
    this->update_status_sensor_("Initializing");
    if (now - this->last_state_change_ >= this->stabilize_wait_ms_) {
      this->state_ = READY;
      this->last_state_change_ = now;
      this->last_packet_sent_ = now;
      this->last_power_check_ = now - 60000; // Force immediate check
      this->script_executor_.set_parent(this); // Initialize script executor with parent
      // Re-sync HA light entities: force_lights_off_ turned them off on disconnect,
      // but the target colors survive and are about to be re-sent to the droid
      // (current_ == 0xFE sentinel) - the UI must show what the droid will show.
      // Configurable via restore_lights_on_connect (default true).
      if (this->restore_lights_on_connect_) {
        this->publish_led_state_to_lights_();
      }
      ESP_LOGI(TAG, "Sphero BB8 is Ready!");
    } else {
      ESP_LOGV(TAG, "Initialization State: Stabilizing (%dms remaining)", (int)(this->stabilize_wait_ms_ - (now - this->last_state_change_)));
    }
  }
  else if (this->state_ == READY) {
    this->update_status_sensor_("Ready");

    // Enable Power Notifications Once
    if (!this->power_notify_enabled_) {
        ESP_LOGD(TAG, "Enabling Power Notifications");
        this->send_packet(DID_CORE, CID_SET_POWER_NOTIFY, {0x01});
        this->power_notify_enabled_ = true;
    }

    // Configure Collision Detection Once
    if (!this->collision_config_sent_) {
        this->configure_collision_detection_();
        this->collision_config_sent_ = true;
    }

    // Auto-reset collision sensor
    if (this->collision_sensor_ != nullptr && this->collision_sensor_->state && now - this->last_collision_time_ > 500) {
        this->collision_sensor_->publish_state(false);
    }

    // Poll Battery
    if (now - this->last_power_check_ > 60000) {
      ESP_LOGD(TAG, "Polling Battery");
      this->power_req_seq_ = this->send_packet(DID_CORE, CID_GET_POWER_STATE, {});
      this->last_power_check_ = now;
    }

    // Get Version Once
    if (!this->version_requested_ && now - this->last_state_change_ > 3000) {
      ESP_LOGD(TAG, "Requesting Firmware Version");
      this->version_req_seq_ = this->send_packet(DID_CORE, CID_VERSION, {});
      this->version_requested_ = true;
    }

    // Run script executor, rate-limited to the 50ms NO_RSP interval so consecutive
    // script actions cannot burst packets at the droid's MCU
    // (docs/MOTION_CONTROL.md section 5; spherov2 advertises 60ms for this toy class).
    if (now - this->last_packet_sent_ >= 50) {
      this->script_executor_.loop();
    }

    if (now - this->last_packet_sent_ > 2000) {
      ESP_LOGV(TAG, "Sending Keep Alive Ping");
      this->send_packet(0x00, 0x01, {}, false);
    }
    else if (now - this->last_packet_sent_ < 50) {
      return;
    }

    if (this->target_r_ != this->current_r_ || this->target_g_ != this->current_g_ || this->target_b_ != this->current_b_) {
      ESP_LOGV(TAG, "Syncing RGB: %d, %d, %d", this->target_r_, this->target_g_, this->target_b_);
      this->send_packet(0x02, 0x20, {this->target_r_, this->target_g_, this->target_b_, 0x00}, false);
      this->current_r_ = this->target_r_;
      this->current_g_ = this->target_g_;
      this->current_b_ = this->target_b_;
    } else if (this->target_back_brightness_ != this->current_back_brightness_) {
      ESP_LOGV(TAG, "Syncing Back LED: %d", this->target_back_brightness_);
      this->send_packet(0x02, 0x21, {this->target_back_brightness_}, false);
      this->current_back_brightness_ = this->target_back_brightness_;
    }
  }
}

void SpheroBB8::configure_collision_detection_() {
    ESP_LOGD(TAG, "Configuring Collision Detection");
    this->send_packet(DID_SPHERO, CID_CONFIG_COLLISION, {0x01, 0x64, 0x64, 0x64, 0x64, 0x32});
}

void SpheroBB8::center_head() {
    ESP_LOGI(TAG, "Centering Head (Self Level)...");
    this->send_packet(DID_SPHERO, CID_SET_SELF_LEVEL, {0x01, 0x00, 0x00, 0x00}, false);
}

void SpheroBB8::add_script(const Script &script) {
    this->script_executor_.add_script(script);
}

bool SpheroBB8::execute_script(const std::string &name) {
    if (!this->is_ready()) {
        ESP_LOGW(TAG, "Cannot execute script '%s': BB8 is not ready", name.c_str());
        return false;
    }
    
    ESP_LOGI(TAG, "Attempting to execute script: %s", name.c_str());
    return this->script_executor_.execute_script(name);
}

void SpheroBB8::stop_script() {
    this->script_executor_.stop_current_script();
}

void SpheroBB8::update_status_sensor_(const std::string &status) {
  if (this->status_sensor_ != nullptr && this->last_status_str_ != status) {
    this->status_sensor_->publish_state(status);
    this->last_status_str_ = status;
  }
}

void SpheroBB8::force_lights_off_() {
  for (auto *light : this->lights_) {
    if (light != nullptr && light->light_state_ != nullptr) {
      if (light->light_state_->remote_values.get_state() > 0) {
        auto call = light->light_state_->make_call();
        call.set_state(false);
        call.perform();
      }
    }
  }
}

void SpheroBB8::dump_config() {
  ESP_LOGCONFIG(TAG, "Sphero BB8");
  ESP_LOGCONFIG(TAG, "  State: %d", this->state_);
  LOG_SENSOR("  ", "Battery Level", this->battery_sensor_);
  LOG_TEXT_SENSOR("  ", "Firmware Version", this->version_sensor_);
  LOG_TEXT_SENSOR("  ", "Charging Status", this->charging_status_sensor_);
  LOG_BINARY_SENSOR("  ", "Collision Detected", this->collision_sensor_);
  LOG_SENSOR("  ", "Collision Speed", this->collision_speed_sensor_);
  LOG_SENSOR("  ", "Collision Magnitude", this->collision_magnitude_sensor_);
}

void SpheroBB8::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                    esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto ble_service_uuid = espbt::ESPBTUUID::from_raw(SERVICE_BLE_UUID);
      auto control_service_uuid = espbt::ESPBTUUID::from_raw(SERVICE_CONTROL_UUID);
      
      auto *anti_dos_char = this->parent()->get_characteristic(ble_service_uuid,
                                                               espbt::ESPBTUUID::from_raw(CHAR_ANTI_DOS_UUID));
      if (anti_dos_char != nullptr) this->char_handle_anti_dos_ = anti_dos_char->handle;

      auto *tx_power_char = this->parent()->get_characteristic(ble_service_uuid,
                                                               espbt::ESPBTUUID::from_raw(CHAR_TX_POWER_UUID));
      if (tx_power_char != nullptr) this->char_handle_tx_power_ = tx_power_char->handle;

      auto *wake_char = this->parent()->get_characteristic(ble_service_uuid,
                                                           espbt::ESPBTUUID::from_raw(CHAR_WAKE_UUID));
      if (wake_char != nullptr) this->char_handle_wake_ = wake_char->handle;

      auto *commands_char = this->parent()->get_characteristic(control_service_uuid,
                                                               espbt::ESPBTUUID::from_raw(CHAR_COMMANDS_UUID));
      if (commands_char != nullptr) this->char_handle_commands_ = commands_char->handle;

      auto *responses_char = this->parent()->get_characteristic(control_service_uuid,
                                                                espbt::ESPBTUUID::from_raw(CHAR_RESPONSES_UUID));
      if (responses_char != nullptr) this->char_handle_responses_ = responses_char->handle;

      if (this->char_handle_anti_dos_ && this->char_handle_wake_ && this->char_handle_commands_ && this->char_handle_responses_) {
        ESP_LOGI(TAG, "Found all required characteristics for Sphero BB8");
        this->state_ = SUBSCRIBE;
      } else {
        ESP_LOGE(TAG, "Failed to find all required characteristics for Sphero BB8");
        this->update_status_sensor_(STATUS_CONN_FAILED);
        this->state_ = DISCONNECTED;
        // Zero the handles ourselves: DISCONNECT may not fire for a client we
        // disable below (mirrors the DISCONNECT-event cleanup set).
        this->char_handle_anti_dos_ = 0;
        this->char_handle_tx_power_ = 0;
        this->char_handle_wake_ = 0;
        this->char_handle_commands_ = 0;
        this->char_handle_responses_ = 0;
        // Drop the link so ble_client's connect policy (auto_connect or the
        // user's Connect button) decides what happens next. Do NOT auto-retry
        // here: a droid left asleep must not be woken repeatedly (README
        // auto_connect note).
        this->parent()->set_enabled(false);
        this->parent()->set_auto_connect(false);
      }
      break;
    }
    case ESP_GATTC_CONNECT_EVT: {
      ESP_LOGI(TAG, "Connected to Sphero BB8");
      this->state_ = CONNECTING;
      this->last_state_change_ = millis();
      this->update_status_sensor_("Connected");
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGI(TAG, "Disconnected from Sphero BB8");
      this->state_ = DISCONNECTED;
      this->char_handle_anti_dos_ = 0;
      this->char_handle_tx_power_ = 0;
      this->char_handle_wake_ = 0;
      this->char_handle_commands_ = 0;
      this->char_handle_responses_ = 0;
      this->write_in_progress_ = false;
      this->current_r_ = 0xFE;
      this->current_g_ = 0xFE;
      this->current_b_ = 0xFE;
      this->current_back_brightness_ = 0xFE;
      this->version_requested_ = false;
      this->power_notify_enabled_ = false;
      this->collision_config_sent_ = false;
      this->update_status_sensor_("Disconnected");
      if (this->is_script_running()) {
        ESP_LOGI(TAG, "Connection lost, stopping motion script");
      }
      this->stop_script();
      this->force_lights_off_();
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      this->write_in_progress_ = false;
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error writing characteristic: %d", param->write.status);
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      ESP_LOGI(TAG, "Registered for notifications");
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle == this->char_handle_responses_) {
        std::vector<uint8_t> data(param->notify.value, param->notify.value + param->notify.value_len);
        this->handle_packet_(data);
      }
      break;
    }
    default:
      break;
  }
}

void SpheroBB8::set_rgb(uint8_t r, uint8_t g, uint8_t b) {
  this->stop_script();
  this->set_rgb_internal(r, g, b);
}

void SpheroBB8::set_back_led(uint8_t brightness) {
  this->stop_script();
  this->set_back_led_internal(brightness);
}

void SpheroBB8::set_rgb_from_light(uint8_t r, uint8_t g, uint8_t b) {
  this->set_rgb_internal(r, g, b);
}

void SpheroBB8::set_back_led_from_light(uint8_t brightness) {
  this->set_back_led_internal(brightness);
}

void SpheroBB8::publish_led_state_to_lights_() {
  for (auto *light : this->lights_) {
    if (light == nullptr) continue;
    light->publish_from_hub();
  }
}

uint8_t SpheroBB8::send_packet(uint8_t did, uint8_t cid, const std::vector<uint8_t> &data, bool wait_for_response) {
  if (this->char_handle_commands_ == 0) return 0;

  if (this->write_in_progress_) {
    return 0;
  }

  uint8_t seq = this->sequence_number_++;
  if (seq == 0) seq = this->sequence_number_++; // Skip 0

  uint8_t dlen = data.size() + 1;
  uint8_t checksum = this->calculate_checksum(did, cid, seq, data);

  std::vector<uint8_t> packet;
  packet.push_back(0xFF); packet.push_back(0xFF);
  packet.push_back(did); packet.push_back(cid);
  packet.push_back(seq); packet.push_back(dlen);
  packet.insert(packet.end(), data.begin(), data.end());
  packet.push_back(checksum);

  ESP_LOGV(TAG, "Sending packet DID=0x%02X CID=0x%02X SEQ=%d (wait=%d)", did, cid, seq, wait_for_response);
  
  auto write_type = wait_for_response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP;

  if (wait_for_response) {
    this->write_in_progress_ = true;
    this->last_write_request_ = millis();
  }

  auto status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                        this->char_handle_commands_, packet.size(), packet.data(),
                                        write_type, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) {
    ESP_LOGE(TAG, "Failed to write command: %d", status);
    if (wait_for_response) this->write_in_progress_ = false;
    return 0;
  }
  this->last_packet_sent_ = millis();
  return seq;
}

uint8_t SpheroBB8::calculate_checksum(uint8_t did, uint8_t cid, uint8_t seq, const std::vector<uint8_t> &data) {
  uint32_t sum = did + cid + seq + (data.size() + 1);
  for (uint8_t b : data) sum += b;
  return ~(sum % 256) & 0xFF;
}

void SpheroBB8::handle_packet_(const std::vector<uint8_t> &data) {
  this->packet_buffer_.insert(this->packet_buffer_.end(), data.begin(), data.end());

  while (this->packet_buffer_.size() >= 5) {
      if (this->packet_buffer_[0] != 0xFF || (this->packet_buffer_[1] != 0xFF && this->packet_buffer_[1] != 0xFE)) {
          this->packet_buffer_.erase(this->packet_buffer_.begin());
          continue;
      }

      uint8_t sop2 = this->packet_buffer_[1];
      size_t packet_len = 0;

      if (sop2 == 0xFF) { // Sync
          uint8_t dlen = this->packet_buffer_[4];
          packet_len = 5 + dlen;
      } else { // Async (0xFE)
          uint16_t dlen = (this->packet_buffer_[3] << 8) | this->packet_buffer_[4];
          packet_len = 5 + dlen;
      }

      // Sanity: no current frame exceeds 21 bytes (collision). 128 leaves headroom
      // for future data-streaming frames (~88 bytes) from the ROADMAP. A dlen
      // implying more means the stream is corrupt; resync from scratch. Without
      // this, an incomplete corrupt frame never satisfies size() < packet_len's
      // opposite, so the buffer grows forever and all parsing dies until
      // disconnect (the checksum gate in process_packet_ never sees it — it only
      // validates already-complete frames).
      if (packet_len > 128) {
          ESP_LOGW(TAG, "Corrupt packet length %zu in buffer, resyncing", (size_t)packet_len);
          this->packet_buffer_.clear();
          return;
      }
      if (this->packet_buffer_.size() > 256) {
          ESP_LOGW(TAG, "Packet buffer overflow (%zu bytes), resyncing",
                   this->packet_buffer_.size());
          this->packet_buffer_.clear();
          return;
      }

      if (this->packet_buffer_.size() < packet_len) {
          return;
      }

      std::vector<uint8_t> packet(this->packet_buffer_.begin(), this->packet_buffer_.begin() + packet_len);
      this->packet_buffer_.erase(this->packet_buffer_.begin(), this->packet_buffer_.begin() + packet_len);

      this->process_packet_(packet);
  }
}

float SpheroBB8::battery_level_from_voltage_(float voltage) {
  // Implausible-reading guard lives here, in exactly one place; NaN signals rejection.
  if (voltage < 2.0f || voltage > 9.0f) {
    ESP_LOGW(TAG, "Implausible battery voltage %.2fV, not publishing", voltage);
    return NAN;
  }
  float level = 0.0f;
  if (voltage > 5.0f) level = (voltage - 7.0f) / (8.4f - 7.0f) * 100.0f;       // 2S pack
  else if (voltage > 2.0f) level = (voltage - 3.5f) / (4.2f - 3.5f) * 100.0f;  // 1S pack
  if (level > 100.0f) level = 100.0f;
  if (level < 0.0f) level = 0.0f;
  return level;
}

void SpheroBB8::process_packet_(const std::vector<uint8_t> &data) {
  if (data.size() < 5) return;

  // Validate the checksum before interpreting anything. Both frame types share
  // the layout SOP1,SOP2,<3 header bytes>,payload...,CHK, so the covered range
  // is identical: everything between SOP2 and the checksum byte. A real frame
  // is >= 6 bytes (5 header + chk); shorter frames cannot be validated.
  if (data.size() >= 6) {
    const uint8_t expected_chk = data.back();
    const uint8_t computed = checksum_bytes({data[2], data[3], data[4]},
                                            std::vector<uint8_t>(data.begin() + 5, data.end() - 1));
    if (computed != expected_chk) {
      ESP_LOGW(TAG, "Discarding packet with bad checksum (len=%zu)", data.size());
      return;
    }
  }

  if (data[0] == 0xFF && data[1] == 0xFE) {
     uint8_t id_code = data[2];
     uint16_t length = (data[3] << 8) | data[4];
     
     if (id_code == 0x01) {
         // Async power notify. Two payload shapes are handled:
         //  - DLEN >= 9: full PowerStatePacket [RecVer@5, State@6, Voltage@7-8, ...]
         //    (layout per reference/gobot .../spherocommon_packets.go PowerStatePacket)
         //  - DLEN 2: bare state byte [State@5]
         // Hardware-observed (2026-09): the bare state byte, pushed ~every 11s with no
         // voltage - battery level therefore updates only from the 60s poll.
         uint8_t state = 0;
         bool has_voltage = false;
         float voltage = 0.0f;
         if (length >= 9) {
             state = data[6];
             voltage = (float)(((data[7] << 8) | data[8])) / 100.0f;
             has_voltage = true;
             ESP_LOGD(TAG, "Async power: RecVer=%d state=%d voltage=%.2fV", data[5], state, voltage);
         } else if (length >= 1) {
             state = data[5];
             ESP_LOGD(TAG, "Async power: state=%d (no voltage in payload)", state);
         } else {
             return;
         }

         if (this->charging_status_sensor_ != nullptr) {
            std::string status = "Unknown";
            if (state == 0x01) status = "Charging";
            else if (state == 0x02) status = "OK";
            else if (state == 0x03) status = "Low";
            else if (state == 0x04) status = "Critical";
            this->charging_status_sensor_->publish_state(status);
         }

         // Track charging transitions (placed on / lifted off the base). When charging
         // ends, force an immediate battery poll so the level reflects reality (e.g. a
         // full pack on the base reads ~8.4V -> 100%) instead of waiting for the 60s
         // cycle. No percentage is ever fabricated from the state category.
         bool charging = (state == 0x01);
         if (charging != this->was_charging_) {
             this->was_charging_ = charging;
             if (!charging) {
                 ESP_LOGI(TAG, "Charging ended, forcing battery poll");
                 this->last_power_check_ = millis() - 60000;
             }
         }

         // Raw voltage is diagnostic and published regardless of charging state
         // (hardware-observed: async bare-state-byte notifications carry no voltage,
         // so in practice this updates from the DLEN>=9 shape only, if ever seen).
         if (this->battery_voltage_sensor_ != nullptr && has_voltage) {
             this->battery_voltage_sensor_->publish_state(voltage);
         }

         // Battery level ONLY from voltage - never a fabricated category percentage.
         // While charging the voltage reading is inflated (same rule as the sync path);
         // implausible voltages are rejected inside battery_level_from_voltage_().
         if (this->battery_sensor_ != nullptr && has_voltage && state != 0x01) {
             float level = battery_level_from_voltage_(voltage);
             if (!std::isnan(level)) {
                 this->battery_sensor_->publish_state(level);
             }
         }
     }
     else if (id_code == 0x07 && length >= 1) {
         if (this->collision_sensor_ != nullptr) {
             this->collision_sensor_->publish_state(true);
             this->last_collision_time_ = millis();
         }
        // Collision async frame (21 bytes total): payload from index 5 is
        // X@5-6 Y@7-8 Z@9-10 Axis@11 XMagnitude@12-13 YMagnitude@14-15
        // Speed@16 Timestamp@17-19, checksum@20. DLEN 0x11 = 16 payload bytes
        // + checksum. Layout per reference/gobot .../spherocommon_packets.go.
        if (data.size() >= 5 + 16) {
            int16_t mag_x = (int16_t)((data[12] << 8) | data[13]);
            int16_t mag_y = (int16_t)((data[14] << 8) | data[15]);
            uint8_t speed = data[16];
            if (this->collision_speed_sensor_ != nullptr) this->collision_speed_sensor_->publish_state(speed);
            if (this->collision_magnitude_sensor_ != nullptr) {
                // (float) casts: with mag_x = mag_y = -32768, int math overflows
                // 2^31 (UB, garbage output on hard impacts). Offsets and the
                // >= 5 + 16 guard are verified correct — do not "fix" them.
                float magnitude = std::sqrt((float)mag_x * mag_x + (float)mag_y * mag_y);
                this->collision_magnitude_sensor_->publish_state(magnitude);
            }
        }
     }
     return;
  }

  if (data[0] != 0xFF || data[1] != 0xFF) return;

  uint8_t mrp = data[2];
  uint8_t seq = data[3];
  uint8_t dlen = data[4];
  
  if (mrp != 0x00) return;
  if (data.size() < 5 + dlen) return;

  if (seq == this->power_req_seq_) {
    // Reads through data[8]: need 9 bytes total, i.e. dlen >= 4 (3 payload
    // bytes + the checksum byte) AND the actual vector large enough.
    if (dlen >= 4 && data.size() >= 9) {
      uint8_t power_state = data[6];
      uint16_t voltage_raw = (data[7] << 8) | data[8];
      float voltage = voltage_raw / 100.0f;
      // Raw voltage is diagnostic and valid during charging too (that's the point:
      // watch charge progress in volts while the percentage is intentionally frozen)
      if (this->battery_voltage_sensor_ != nullptr) {
        this->battery_voltage_sensor_->publish_state(voltage);
      }
      if (this->charging_status_sensor_ != nullptr) {
        std::string status = "Unknown";
        if (power_state == 0x01) status = "Charging";
        else if (power_state == 0x02) status = "OK";
        else if (power_state == 0x03) status = "Low";
        else if (power_state == 0x04) status = "Critical";
        this->charging_status_sensor_->publish_state(status);
      }
      // Skip battery level publishing when charging - voltage reading is inflated
      if (this->battery_sensor_ != nullptr && power_state != 0x01) {
        float level = battery_level_from_voltage_(voltage);
        if (!std::isnan(level)) {
          this->battery_sensor_->publish_state(level);
        }
      }
    }
  } 
  else if (seq == this->version_req_seq_) {
     if (dlen >= 5 && data.size() >= 10) {
       uint8_t maj = data[8];
       uint8_t min = data[9];
       char buffer[16];
       snprintf(buffer, sizeof(buffer), "%d.%d.0", maj, min);
       if (this->version_sensor_ != nullptr) this->version_sensor_->publish_state(buffer);
     }
  }
}

}  // namespace sphero_bb8
}  // namespace esphome

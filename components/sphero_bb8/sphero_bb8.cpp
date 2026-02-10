#include "sphero_bb8.h"
#include "sphero_bb8_light.h"
#include "esphome/core/log.h"

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

void SpheroBB8::setup() {
  this->current_r_ = 0xFE;
  this->current_g_ = 0xFE;
  this->current_b_ = 0xFE;
  this->current_back_brightness_ = 0xFE;
  this->enabled_ = this->auto_connect_;
  this->parent()->set_enabled(this->enabled_);
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
      // Sleep command: DID 0x00, CID 0x22, Data [0,0,0,0,0]
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
      this->update_status_sensor_("Disabling...");
    } else {
      this->update_status_sensor_("Disconnected");
    }
    return;
  }

  // Handle re-enabling
  if (this->enabled_ && !this->parent()->enabled) {
    ESP_LOGI(TAG, "Enabling BB8 connection...");
    this->parent()->set_enabled(true);
  }

  if (this->state_ == DISCONNECTED || !this->parent()->connected()) {
    this->update_status_sensor_("Disconnected");
    return;
  }
  
  if (this->write_in_progress_) {
    return;
  }

  // State Machine matching Gobot: Subscribe -> Anti-DOS -> TX Power -> Wake -> Ready
  
  if (this->state_ == SUBSCRIBE && this->char_handle_responses_ != 0) {
    this->update_status_sensor_("Initializing (Subscribe)");
    ESP_LOGI(TAG, "Subscribing to notifications...");
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
    if (now - this->last_state_change_ < 200) return;

    this->update_status_sensor_("Initializing (Anti-DOS)");
    ESP_LOGI(TAG, "Sending Anti-DOS...");
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
    this->update_status_sensor_("Initializing (TX Power)");
    ESP_LOGI(TAG, "Sending TX Power...");
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
    this->update_status_sensor_("Initializing (Wake)");
    ESP_LOGI(TAG, "Sending Wake...");
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
    this->state_ = READY;
    this->last_state_change_ = now;
    this->last_packet_sent_ = now;
    ESP_LOGI(TAG, "Sphero BB8 is Ready!");
  } 
  else if (this->state_ == READY) {
    this->update_status_sensor_("Ready");
    if (now - this->last_packet_sent_ > 2000) {
      ESP_LOGV(TAG, "Sending Keep Alive Ping");
      this->send_packet(0x00, 0x01, {}, false);
    } 
    else if (now - this->last_packet_sent_ < 50) {
      return;
    }

    if (this->target_r_ != this->current_r_ || this->target_g_ != this->current_g_ || this->target_b_ != this->current_b_) {
      ESP_LOGD(TAG, "Syncing RGB: %d, %d, %d", this->target_r_, this->target_g_, this->target_b_);
      this->send_packet(0x02, 0x20, {this->target_r_, this->target_g_, this->target_b_, 0x00}, false);
      this->current_r_ = this->target_r_;
      this->current_g_ = this->target_g_;
      this->current_b_ = this->target_b_;
    } else if (this->target_back_brightness_ != this->current_back_brightness_) {
      ESP_LOGD(TAG, "Syncing Back LED: %d", this->target_back_brightness_);
      this->send_packet(0x02, 0x21, {this->target_back_brightness_}, false);
      this->current_back_brightness_ = this->target_back_brightness_;
    }
  }
}

void SpheroBB8::update_status_sensor_(const std::string &status) {
  if (this->status_sensor_ != nullptr && this->last_status_str_ != status) {
    this->status_sensor_->publish_state(status);
    this->last_status_str_ = status;
  }
}

void SpheroBB8::force_lights_off_() {
  for (auto *light : this->lights_) {
    if (light->light_state_ != nullptr) {
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
      this->update_status_sensor_("Disconnected");
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
    default:
      break;
  }
}

void SpheroBB8::set_rgb(uint8_t r, uint8_t g, uint8_t b) {
  ESP_LOGV(TAG, "Setting RGB target: %d, %d, %d", r, g, b);
  this->target_r_ = r;
  this->target_g_ = g;
  this->target_b_ = b;
}

void SpheroBB8::set_back_led(uint8_t brightness) {
  ESP_LOGV(TAG, "Setting Back LED target: %d", brightness);
  this->target_back_brightness_ = brightness;
}

void SpheroBB8::send_packet(uint8_t did, uint8_t cid, const std::vector<uint8_t> &data, bool wait_for_response) {
  if (this->char_handle_commands_ == 0) return;

  uint8_t seq = this->sequence_number_++;
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
  }
  this->last_packet_sent_ = millis();
}

uint8_t SpheroBB8::calculate_checksum(uint8_t did, uint8_t cid, uint8_t seq, const std::vector<uint8_t> &data) {
  uint32_t sum = did + cid + seq + (data.size() + 1);
  for (uint8_t b : data) sum += b;
  return ~(sum % 256) & 0xFF;
}

}  // namespace sphero_bb8
}  // namespace esphome
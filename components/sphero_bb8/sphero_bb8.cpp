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

static const uint8_t DID_CORE = 0x00;
static const uint8_t DID_SPHERO = 0x02;
static const uint8_t CID_VERSION = 0x02;
static const uint8_t CID_GET_POWER_STATE = 0x20;
static const uint8_t CID_SET_POWER_NOTIFY = 0x21;
static const uint8_t CID_CONFIG_COLLISION = 0x12;

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
  this->parent()->set_enabled(true);
  this->parent()->set_auto_connect(true);
}

void SpheroBB8::disconnect() {
  ESP_LOGI(TAG, "Disconnect button pressed, disabling BB8 connection...");
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
    this->update_status_sensor_("Connecting");
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
    if (now - this->last_state_change_ < 200) return;

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
    if (now - this->last_state_change_ >= 2000) {
      this->state_ = READY;
      this->last_state_change_ = now;
      this->last_packet_sent_ = now;
      this->last_power_check_ = now - 60000; // Force immediate check
      ESP_LOGI(TAG, "Sphero BB8 is Ready!");
    } else {
      ESP_LOGV(TAG, "Initialization State: Stabilizing (%dms remaining)", 2000 - (now - this->last_state_change_));
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
    // Method: 0x01 (Enable)
    // Xt (Threshold): 0x64 (100)
    // Xspd (Speed): 0x64 (100)
    // Yt (Threshold): 0x64 (100)
    // Yspd (Speed): 0x64 (100)
    // DeadTime: 0x32 (50 * 10ms = 500ms)
    this->send_packet(DID_SPHERO, CID_CONFIG_COLLISION, {0x01, 0x64, 0x64, 0x64, 0x64, 0x32});
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
      this->version_requested_ = false;
      this->power_notify_enabled_ = false;
      this->collision_config_sent_ = false;
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
  ESP_LOGV(TAG, "Setting RGB target: %d, %d, %d", r, g, b);
  this->target_r_ = r;
  this->target_g_ = g;
  this->target_b_ = b;
}

void SpheroBB8::set_back_led(uint8_t brightness) {
  ESP_LOGV(TAG, "Setting Back LED target: %d", brightness);
  this->target_back_brightness_ = brightness;
}

uint8_t SpheroBB8::send_packet(uint8_t did, uint8_t cid, const std::vector<uint8_t> &data, bool wait_for_response) {
  if (this->char_handle_commands_ == 0) return 0;

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
      // Check SOP
      if (this->packet_buffer_[0] != 0xFF || (this->packet_buffer_[1] != 0xFF && this->packet_buffer_[1] != 0xFE)) {
          // Invalid start, shift by 1 to find next SOP
          this->packet_buffer_.erase(this->packet_buffer_.begin());
          continue;
      }

      // Parse Length
      uint8_t sop2 = this->packet_buffer_[1];
      size_t packet_len = 0;

      if (sop2 == 0xFF) { // Sync
          uint8_t dlen = this->packet_buffer_[4];
          packet_len = 5 + dlen;
      } else { // Async (0xFE)
          uint16_t dlen = (this->packet_buffer_[3] << 8) | this->packet_buffer_[4];
          packet_len = 5 + dlen;
      }

      if (this->packet_buffer_.size() < packet_len) {
          // Incomplete packet, wait for more data
          return;
      }

      // Extract complete packet
      std::vector<uint8_t> packet(this->packet_buffer_.begin(), this->packet_buffer_.begin() + packet_len);
      this->packet_buffer_.erase(this->packet_buffer_.begin(), this->packet_buffer_.begin() + packet_len);

      this->process_packet_(packet);
  }
}

void SpheroBB8::process_packet_(const std::vector<uint8_t> &data) {
  // Debug dump
  std::string hex_dump = "";
  for (uint8_t b : data) {
    char buf[4];
    sprintf(buf, "%02X ", b);
    hex_dump += buf;
  }
  ESP_LOGD(TAG, "Processing Packet: %s", hex_dump.c_str());

  if (data.size() < 5) return;
  
  // Async Packet (Notification)
  if (data[0] == 0xFF && data[1] == 0xFE) {
     uint8_t id_code = data[2];
     uint16_t length = (data[3] << 8) | data[4];
     
     // Async Power Notification
     if (id_code == 0x01 && length >= 1) {
         uint8_t state = data[5];
         ESP_LOGI(TAG, "Received Async Power Notification: State=0x%02X", state);
         
         if (this->charging_status_sensor_ != nullptr) {
            std::string status = "Unknown";
            if (state == 0x01) status = "Charging";
            else if (state == 0x02) status = "OK";
            else if (state == 0x03) status = "Low";
            else if (state == 0x04) status = "Critical";
            this->charging_status_sensor_->publish_state(status);
         }
    
         if (this->battery_sensor_ != nullptr) {
            float level = 0.0f;
            if (state == 0x01) level = 100.0f;
            else if (state == 0x02) level = 100.0f;
            else if (state == 0x03) level = 20.0f;
            else if (state == 0x04) level = 5.0f;
            this->battery_sensor_->publish_state(level);
         }
     }
     // Async Collision Notification
     else if (id_code == 0x07 && length >= 1) {
         ESP_LOGI(TAG, "Received Async Collision Notification");
         if (this->collision_sensor_ != nullptr) {
             this->collision_sensor_->publish_state(true);
             this->last_collision_time_ = millis();
         }

         // Payload parsing (Standard 16-byte structure)
         // X(2), Y(2), Z(2), Axis(1), MagX(2), MagY(2), Speed(1), Time(4)
         if (data.size() >= 5 + 16) { 
             int16_t mag_x = (int16_t)((data[12] << 8) | data[13]);
             int16_t mag_y = (int16_t)((data[14] << 8) | data[15]);
             uint8_t speed = data[16];
             
             ESP_LOGD(TAG, "Collision Data: MagX=%d MagY=%d Speed=%d", mag_x, mag_y, speed);
             
             if (this->collision_speed_sensor_ != nullptr) {
                 this->collision_speed_sensor_->publish_state(speed);
             }
             
             if (this->collision_magnitude_sensor_ != nullptr) {
                 float magnitude = std::sqrt(mag_x*mag_x + mag_y*mag_y);
                 this->collision_magnitude_sensor_->publish_state(magnitude);
             }
         }
     }
     return;
  }

  // Sync Packet (Response)
  if (data[0] != 0xFF || data[1] != 0xFF) return;

  uint8_t mrp = data[2];
  uint8_t seq = data[3];
  uint8_t dlen = data[4];
  
  if (mrp != 0x00) {
    ESP_LOGW(TAG, "Received error response code: 0x%02X for sequence %d", mrp, seq);
    return;
  }

  if (data.size() < 5 + dlen) return;

  if (seq == this->power_req_seq_) {
    if (dlen >= 3) {
      uint8_t rec_state = data[5];
      uint8_t power_state = data[6];
      ESP_LOGD(TAG, "Received Power State: RecState=0x%02X, PowerState=0x%02X", rec_state, power_state);

      if (this->charging_status_sensor_ != nullptr) {
        std::string status = "Unknown";
        if (rec_state == 0x01) status = "Charging";
        else if (rec_state == 0x02) status = "OK";
        else if (rec_state == 0x03) status = "Low";
        else if (rec_state == 0x04) status = "Critical";
        this->charging_status_sensor_->publish_state(status);
      }

      if (this->battery_sensor_ != nullptr) {
        float level = 0.0f;
        if (rec_state == 0x01) level = 100.0f;
        else if (rec_state == 0x02) level = 100.0f;
        else if (rec_state == 0x03) level = 20.0f;
        else if (rec_state == 0x04) level = 5.0f;
        this->battery_sensor_->publish_state(level);
      }
    }
  } 
  else if (seq == this->version_req_seq_) {
     if (dlen >= 6 && data.size() >= 10) {
       uint8_t maj = data[8];
       uint8_t min = data[9];
       char buffer[16];
       snprintf(buffer, sizeof(buffer), "%d.%d", maj, min);
       ESP_LOGI(TAG, "Received Firmware Version: %s", buffer);
       if (this->version_sensor_ != nullptr) {
         this->version_sensor_->publish_state(buffer);
       }
     } else {
       ESP_LOGW(TAG, "Received Version packet but too short (DLEN=%d, Size=%d)", dlen, data.size());
     }
  }
}

}  // namespace sphero_bb8
}  // namespace esphome

#include "script_executor.h"
#include "sphero_bb8.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sphero_bb8 {

static const char *const TAG = "sphero_bb8.script";

ScriptExecutor::ScriptExecutor() {}

void ScriptExecutor::add_script(const Script &script) {
  scripts_.push_back(script);
  ESP_LOGI(TAG, "Added script: %s with %zu steps", script.name.c_str(), script.steps.size());
}

bool ScriptExecutor::execute_script(const std::string &name) {
  if (state_ == ScriptState::RUNNING) {
    ESP_LOGD(TAG, "Stopping current script to start: %s", name.c_str());
    stop_current_script();
  }

  for (const auto &script : scripts_) {
    if (script.name == name) {
      frames_.push({&script.steps, 0, 1});
      state_ = ScriptState::RUNNING;
      wait_duration_ = 0;
      ESP_LOGI(TAG, "Started script: %s", name.c_str());
      return true;
    }
  }

  ESP_LOGW(TAG, "Script not found: %s", name.c_str());
  return false;
}

void ScriptExecutor::stop_current_script() {
  if (state_ != ScriptState::IDLE) {
    ESP_LOGI(TAG, "Stopping script execution");
    state_ = ScriptState::IDLE;
    while (!frames_.empty()) {
      frames_.pop();
    }
    wait_duration_ = 0;
  }
}

void ScriptExecutor::loop() {
  if (state_ != ScriptState::RUNNING) {
    return;
  }

  if (wait_duration_ > 0) {
    if (millis() - wait_start_time_ >= wait_duration_) {
      wait_duration_ = 0;
    } else {
      return;
    }
  }

  if (frames_.empty()) {
    state_ = ScriptState::FINISHED;
    ESP_LOGI(TAG, "Script execution finished");
    state_ = ScriptState::IDLE;
    return;
  }

  ExecutionFrame &frame = frames_.top();

  if (frame.step_index >= frame.steps->size()) {
    frame.remaining_repeats--;
    if (frame.remaining_repeats > 0) {
      frame.step_index = 0;
      ESP_LOGV(TAG, "Repeating frame, %d remaining", frame.remaining_repeats);
    } else {
      frames_.pop();
    }
    return;
  }

  const ScriptAction &action = (*frame.steps)[frame.step_index];

  if (action.type == ScriptActionType::REPEAT) {
    ESP_LOGD(TAG, "Entering repeat block: count=%d", action.repeat_count);
    frame.step_index++; // Move past the repeat action in the current frame
    frames_.push({&action.repeat_steps, 0, action.repeat_count});
    return;
  }

  if (execute_action_(action)) {
    frame.step_index++;
  } else {
    // Failed to send (likely BLE busy), retry next loop
    ESP_LOGV(TAG, "Action failed to send, retrying next loop");
  }
}

bool ScriptExecutor::execute_action_(const ScriptAction &action) {
  if (parent_ == nullptr) {
    return false;
  }

  uint8_t seq = 0;
  switch (action.type) {
    case ScriptActionType::ROLL:
      ESP_LOGD(TAG, "Step: Roll(speed=%d, heading=%d)", action.roll_speed, action.roll_heading);
      seq = parent_->send_packet(0x02, 0x30, {action.roll_speed, (uint8_t)(action.roll_heading >> 8),
                                             (uint8_t)(action.roll_heading & 0xFF), 0x01}, false);
      return seq != 0;

    case ScriptActionType::STOP:
      ESP_LOGD(TAG, "Step: Stop");
      seq = parent_->send_packet(0x02, 0x30, {0x00, 0x00, 0x00, 0x01}, false);
      return seq != 0;

    case ScriptActionType::SPIN:
      ESP_LOGD(TAG, "Step: Spin(rate=%d)", action.spin_rate);
      seq = parent_->send_packet(0x02, 0x03, {action.spin_rate}, false);
      return seq != 0;

    case ScriptActionType::RAW_MOTOR:
      ESP_LOGD(TAG, "Step: RawMotor(left=%d, right=%d)", action.left_speed, action.right_speed);
      seq = parent_->send_packet(0x02, 0x33, {0x01, action.left_speed, 0x01, action.right_speed}, false);
      return seq != 0;

    case ScriptActionType::SET_HEADING:
      ESP_LOGD(TAG, "Step: SetHeading(%d)", action.heading);
      seq = parent_->send_packet(0x02, 0x01, {(uint8_t)(action.heading >> 8), (uint8_t)(action.heading & 0xFF)}, false);
      return seq != 0;

    case ScriptActionType::LIGHT:
      ESP_LOGD(TAG, "Step: Light(r=%d, g=%d, b=%d)", action.red, action.green, action.blue);
      // We use internal methods to avoid stopping the script itself
      parent_->set_rgb_internal(action.red, action.green, action.blue);
      return true;

    case ScriptActionType::BACK_LIGHT:
      ESP_LOGD(TAG, "Step: BackLight(brightness=%d)", action.brightness);
      parent_->set_back_led_internal(action.brightness);
      return true;

    case ScriptActionType::CENTER_HEAD:
      ESP_LOGD(TAG, "Step: CenterHead");
      parent_->center_head();
      return true;

    case ScriptActionType::STABILIZE:
      ESP_LOGD(TAG, "Step: Stabilize(%s)", action.stabilize_state ? "ON" : "OFF");
      seq = parent_->send_packet(0x02, 0x02, {static_cast<uint8_t>(action.stabilize_state ? 0x01 : 0x00)}, false);
      return seq != 0;

    case ScriptActionType::WAIT:
      ESP_LOGD(TAG, "Step: Wait(%d ms)", action.duration_ms);
      wait_start_time_ = millis();
      wait_duration_ = action.duration_ms;
      return true;

    case ScriptActionType::REPEAT:
      // Should be handled in loop()
      return true;
  }
  return true;
}

}  // namespace sphero_bb8
}  // namespace esphome

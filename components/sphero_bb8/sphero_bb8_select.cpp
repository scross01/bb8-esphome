#include "sphero_bb8_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sphero_bb8 {

static const char *const TAG = "sphero_bb8.select";

void SpheroBB8ScriptSelect::dump_config() {
  ESP_LOGCONFIG(TAG, "Sphero BB8 Script Select");
  ESP_LOGCONFIG(TAG, "  Scripts: %zu options", this->size());
}

void SpheroBB8ScriptSelect::control(const std::string &value) {
  if (this->parent_ == nullptr) {
    return;
  }
  if (this->parent_->execute_script(value)) {
    // Success path publishes via the hub's script-started notification, which
    // also covers scripts launched from buttons and automations.
    ESP_LOGD(TAG, "Script '%s' launched from select", value.c_str());
  } else {
    // Unknown script or droid not ready: republish the current selection so
    // the UI snaps back instead of showing an option that didn't take.
    ESP_LOGW(TAG, "Script '%s' could not start, reverting selection", value.c_str());
    if (this->has_state()) {
      this->publish_state(this->current_option().str());
    }
  }
}

}  // namespace sphero_bb8
}  // namespace esphome

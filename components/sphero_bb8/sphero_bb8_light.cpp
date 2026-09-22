#include "sphero_bb8_light.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome {
namespace sphero_bb8 {

static const char *const TAG = "sphero_bb8_light";

light::LightTraits SpheroBB8Light::get_traits() {
  auto traits = light::LightTraits();
  if (this->type_ == "RGB") {
    traits.set_supported_color_modes({light::ColorMode::RGB});
  } else {
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  }
  return traits;
}

void SpheroBB8Light::write_state(light::LightState *state) {
  this->light_state_ = state;

  auto remote_vals = state->remote_values;

  if (this->parent_ == nullptr || !this->parent_->is_ready()) {
    if (remote_vals.get_state() > 0) {
      ESP_LOGD(TAG, "Robot not ready, forcing light state back to OFF in UI");
      auto call = state->make_call();
      call.set_state(false);
      call.perform();
    }
    return;
  }

  auto current_vals = state->current_values;

  // A running script owns the droid's LEDs: swallow transitional ticks (fades
  // tick at ~33ms and previously tore the script down via stop-on-every-write).
  // Explicit user OFF is still honored -- a hard stop must always be available
  // (docs/MOTION_CONTROL.md section 6 manual-commands-stop-scripts policy).
  if (this->parent_->is_script_running()) {
    if (current_vals.get_state() == 0.0f) {
      ESP_LOGI(TAG, "Light turned off during script; stopping script");
      this->parent_->stop_script();
      // fall through: script no longer running, push the off color below
    } else {
      return;
    }
  }

  if (this->type_ == "RGB") {
    float r = current_vals.get_red();
    float g = current_vals.get_green();
    float b = current_vals.get_blue();
    float br = current_vals.get_brightness() * current_vals.get_state();
    this->parent_->set_rgb_from_light(r * br * 255, g * br * 255, b * br * 255);
  } else if (this->type_ == "TAILLIGHT") {
    float br = current_vals.get_brightness() * current_vals.get_state();
    this->parent_->set_back_led_from_light(br * 255);
  }
}

void SpheroBB8Light::publish_from_hub() {
  if (this->light_state_ == nullptr || this->parent_ == nullptr) return;
  auto call = this->light_state_->make_call();
  if (this->type_ == "RGB") {
    uint8_t r = this->parent_->target_r();
    uint8_t g = this->parent_->target_g();
    uint8_t b = this->parent_->target_b();
    uint8_t maxc = std::max(r, std::max(g, b));
    if (maxc == 0) {
      call.set_state(false);
    } else {
      // Script colors are absolute RGB: force full brightness so the UI color
      // matches what the droid actually emits (hub targets are pre-scaled).
      call.set_state(true);
      call.set_brightness(1.0f);
      call.set_red(r / 255.0f);
      call.set_green(g / 255.0f);
      call.set_blue(b / 255.0f);
    }
  } else {
    uint8_t br = this->parent_->target_back_brightness();
    call.set_state(br > 0);
    call.set_brightness(br / 255.0f);
  }
  call.perform();
}

}  // namespace sphero_bb8
}  // namespace esphome
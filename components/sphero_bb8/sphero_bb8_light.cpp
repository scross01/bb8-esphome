#include "sphero_bb8_light.h"
#include "esphome/core/log.h"

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
  if (this->parent_ == nullptr || !this->parent_->is_ready())
    return;

  auto values = state->remote_values;
  if (this->type_ == "RGB") {
    float r = values.get_red();
    float g = values.get_green();
    float b = values.get_blue();
    float br = values.get_brightness() * values.get_state();
    this->parent_->set_rgb(r * br * 255, g * br * 255, b * br * 255);
  } else if (this->type_ == "TAILLIGHT") {
    float br = values.get_brightness() * values.get_state();
    this->parent_->set_back_led(br * 255);
  }
}

}  // namespace sphero_bb8
}  // namespace esphome

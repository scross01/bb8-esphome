#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "sphero_bb8.h"

namespace esphome {
namespace sphero_bb8 {

class SpheroBB8Light : public light::LightOutput, public Component {
 public:
  void set_parent(SpheroBB8 *parent) { parent_ = parent; }
  void set_type(const std::string &type) { type_ = type; }

  void setup() override { this->parent_->register_light(this); }
  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;

 protected:
  friend class SpheroBB8;
  SpheroBB8 *parent_{nullptr};
  light::LightState *light_state_{nullptr};
  std::string type_;
};

}  // namespace sphero_bb8
}  // namespace esphome
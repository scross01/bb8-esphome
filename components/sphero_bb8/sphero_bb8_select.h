#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "sphero_bb8.h"

namespace esphome {
namespace sphero_bb8 {

// Dropdown of installed motion scripts; choosing an option executes it.
// The selection follows every script start, whatever surface launched it
// (dropdown, script button, or automation action). Logging lives in the
// .cpp so this header defines no TAG (the hub's TU has its own).
class SpheroBB8ScriptSelect : public select::Select, public Component {
 public:
  explicit SpheroBB8ScriptSelect(SpheroBB8 *parent) : parent_(parent) {}

  void setup() override {
    // Show the first script as the resting selection; the entity is a command
    // surface, so the state is informational until a script starts.
    if (!this->traits.get_options().empty()) {
      this->publish_state(static_cast<size_t>(0));
    }
  }

  void dump_config() override;

  // Called by the hub when any script starts; keeps the dropdown in sync with
  // scripts launched from buttons or automations.
  void notify_script_started(const std::string &name) { this->publish_state(name); }

 protected:
  void control(const std::string &value) override;

  SpheroBB8 *parent_;
};

}  // namespace sphero_bb8
}  // namespace esphome

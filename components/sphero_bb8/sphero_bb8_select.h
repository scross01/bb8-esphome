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
    // Resting state is the synthetic Idle option (index 0 — select.py prepends
    // it to the options list; that order is a load-bearing invariant). Honest
    // (no script running) and different from every script name, so HA will
    // always transmit a re-pick of the same script: HA only sends differential
    // picks, so resting on a script name blocks same-script re-fire.
    this->publish_state(static_cast<size_t>(0));
  }

  void dump_config() override;

  // Called by the hub when any script starts; keeps the dropdown in sync with
  // scripts launched from buttons or automations.
  void notify_script_started(const std::string &name) { this->publish_state(name); }

  // Called by the hub when a script finishes or is stopped; back to Idle so
  // the next pick — including an immediate repeat — always transmits.
  void notify_script_finished() { this->publish_state(static_cast<size_t>(0)); }

 protected:
  void control(const std::string &value) override;

  SpheroBB8 *parent_;
};

}  // namespace sphero_bb8
}  // namespace esphome

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <vector>
#include <stack>
#include <queue>

namespace esphome {
namespace sphero_bb8 {

enum class ScriptActionType {
  ROLL,
  STOP,
  SPIN,
  RAW_MOTOR,
  SET_HEADING,
  LIGHT,
  BACK_LIGHT,
  CENTER_HEAD,
  STABILIZE,
  WAIT,
  REPEAT
};

struct ScriptAction {
  ScriptActionType type;
  
  // For roll action
  uint8_t roll_speed{0};
  uint16_t roll_heading{0};
  
  // For spin action
  uint8_t spin_rate{0};
  
  // For raw motor action
  uint8_t left_speed{0};
  uint8_t right_speed{0};
  
  // For set heading action
  uint16_t heading{0};
  
  // For light action
  uint8_t red{0};
  uint8_t green{0};
  uint8_t blue{0};
  
  // For back light action
  uint8_t brightness{0};
  
  // For stabilize action
  bool stabilize_state{true};
  
  // For wait action
  uint32_t duration_ms{0};
  
  // For repeat action
  uint8_t repeat_count{0};
  std::vector<ScriptAction> repeat_steps{};
};

struct Script {
  std::string name;
  std::vector<ScriptAction> steps;
};

enum class ScriptState {
  IDLE,
  RUNNING,
  PAUSED,
  FINISHED
};

class ScriptExecutor {
 public:
  ScriptExecutor();
  
  void add_script(const Script &script);
  bool execute_script(const std::string &name);
  void stop_current_script();
  void loop();
  
  bool is_idle() const { return state_ == ScriptState::IDLE; }
  bool is_running() const { return state_ == ScriptState::RUNNING; }
  
  void set_parent(class SpheroBB8 *parent) { parent_ = parent; }

 protected:
  void execute_next_step_();
  void execute_action_(const ScriptAction &action);
  void handle_repeat_();
  
  std::vector<Script> scripts_;
  ScriptState state_{ScriptState::IDLE};
  size_t current_script_index_{0};
  size_t current_step_index_{0};
  uint32_t wait_start_time_{0};
  uint32_t wait_duration_{0};
  
  // Repeat handling
  std::stack<std::pair<size_t, size_t>> repeat_stack_; // Stores (step_index, remaining_iterations)
  
  class SpheroBB8 *parent_{nullptr};
};

}  // namespace sphero_bb8
}  // namespace esphome
# Host-side config-validation tests for the sphero_bb8 ESPHome component.
#
# Runs under pytest:            python -m pytest tests/ -v
# Runs without pytest as well:  python3 tests/test_config_validation.py
# (a builtin fallback runner executes test_* functions in order; see bottom).
#
# The component module imports esphome.codegen at top level; tests/conftest.py
# (or the fallback runner below) puts the installed ESPHome site-packages on
# sys.path first.
#
# NOTE: no `import pytest` — cv.Invalid is raised via plain `raise`, so this
# file works in both modes. The `_raises_invalid` helper encodes the "must
# raise" semantics; pytest would report it as a failed test if no exception.

import sys
import os

_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

try:
    # Under pytest conftest.py already ran; standalone mode needs the shim
    # explicitly (it sets up sys.path, ESPHome site-packages, and the
    # esphome.components mount).
    import conftest  # noqa: F401

    conftest.setup()
except ImportError:
    pass

from esphome.config_validation import Invalid  # noqa: E402
from esphome.config_validation import (  # noqa: E402
    positive_time_period_milliseconds,
)

from components.sphero_bb8.__init__ import (  # noqa: E402
    ACTION_WAIT_SCHEMA,
    SCRIPT_SCHEMA,
    validate_script_action,
)


def _raises_invalid(fn):
    """True if fn() raises esphome Invalid (or any Exception, conservatively)."""
    try:
        fn()
    except Invalid:
        return True
    except Exception:  # noqa: BLE001 — any validation failure counts
        return True
    return False


# ---- 1. validate_script_action: single valid action passes -------------------
def test_single_action_valid():
    result = validate_script_action({"roll": {"heading": 30, "speed": 100}})
    assert isinstance(result, dict)
    assert "roll" in result
    assert result["roll"]["heading"] == 30
    assert result["roll"]["speed"] == 100


# ---- 2. two action keys rejected ---------------------------------------------
def test_two_actions_rejected():
    value = {"roll": {"heading": 30, "speed": 100}, "stop": {}}
    assert _raises_invalid(lambda: validate_script_action(value))


# ---- 3. zero action keys rejected --------------------------------------------
def test_zero_actions_rejected():
    assert _raises_invalid(lambda: validate_script_action({}))


# ---- 4. wait as bare time period ----------------------------------------------
def test_wait_bare_period():
    period = positive_time_period_milliseconds("150ms")
    result = validate_script_action({"wait": period})
    assert "wait" in result
    assert result["wait"].total_milliseconds == 150


# ---- 5. wait as dict -----------------------------------------------------------
def test_wait_dict_form():
    result = validate_script_action({"wait": {"duration": "200ms"}})
    assert "wait" in result
    # Dict form converts the inner value to the same TimePeriod type.
    assert result["wait"]["duration"].total_milliseconds == 200


# ---- 6. out-of-range roll heading rejected ------------------------------------
def test_roll_heading_out_of_range():
    assert _raises_invalid(lambda: validate_script_action({"roll": {"heading": 400, "speed": 100}}))


# ---- 6b. out-of-range raw_motor speed rejected (post-plan-010 bounds) ---------
def test_raw_motor_speed_out_of_range():
    assert _raises_invalid(
        lambda: validate_script_action({"raw_motor": {"left_speed": 300, "right_speed": 0}})
    )
    # And the documented signed bounds are accepted end-to-end.
    result = validate_script_action({"raw_motor": {"left_speed": -255, "right_speed": 255}})
    assert result["raw_motor"]["left_speed"] == -255


# ---- 7. SCRIPT_SCHEMA minimal script accepted; bad step rejected --------------
def test_script_schema_accepts_minimal():
    script = {
        "name": "Minimal",
        "steps": [
            {"roll": {"heading": 0, "speed": 50}},
            {"wait": "100ms"},
            {"stop": {}},
        ],
    }
    result = SCRIPT_SCHEMA(script)
    assert result["name"] == "Minimal"
    assert len(result["steps"]) == 3


def test_script_schema_rejects_bad_step():
    script = {
        "name": "Bad",
        "steps": [{"roll": {"heading": 999, "speed": 50}}],
    }
    assert _raises_invalid(lambda: SCRIPT_SCHEMA(script))


def test_script_schema_rejects_missing_name():
    script = {"steps": [{"stop": {}}]}
    assert _raises_invalid(lambda: SCRIPT_SCHEMA(script))


# ---- fallback runner (no pytest installed) -------------------------------------
def _main():
    import traceback

    tests = [
        (name, obj)
        for name, obj in sorted(globals().items())
        if name.startswith("test_") and callable(obj)
    ]
    failures = 0
    for name, fn in tests:
        try:
            fn()
            print(f"PASSED {name}")
        except Exception:  # noqa: BLE001
            failures += 1
            print(f"FAILED {name}")
            traceback.print_exc()
    print(f"{len(tests) - failures}/{len(tests)} config-validation tests passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(_main())

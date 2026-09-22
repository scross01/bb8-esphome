# Makes `components.sphero_bb8.__init__` importable outside an ESPHome build.
# Used by pytest automatically, and by tests/test_config_validation.py's
# standalone fallback runner (`import conftest`).
#
# Three things are needed:
# 1. Repo root on sys.path so `components.sphero_bb8` resolves as a package.
# 2. The installed ESPHome distribution's site-packages appended so
#    `import esphome.codegen` (a component top-level import) works. The
#    environment-specific globs below are only appended when they exist.
# 3. The repo's `components/` dir registered under the `esphome.components`
#    namespace — the component imports its own submodules as
#    `esphome.components.sphero_bb8.action`, exactly how ESPHome mounts
#    external components during a build. Extending the package __path__
#    replicates that mount.
#
# Adjust the globs if ESPHome lives elsewhere (venv, pipx, ...).
import glob
import os
import sys
import types

_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_done = False


def setup():
    global _done
    if _done:
        return
    _done = True

    # 1. Repo root.
    if _ROOT not in sys.path:
        sys.path.insert(0, _ROOT)

    # 2. ESPHome's site-packages.
    patterns = [
        "/home/linuxbrew/.linuxbrew/Cellar/esphome/*/libexec/lib/python*/site-packages",
        os.path.expanduser("~/.local/pipx/venvs/esphome/lib/python*/site-packages"),
    ]
    for pattern in patterns:
        for site_packages in sorted(glob.glob(pattern), reverse=True):
            if site_packages not in sys.path:
                sys.path.append(site_packages)

    # 3. Mount the repo's components/ under esphome.components.
    try:
        import esphome  # noqa: F401
    except ImportError:
        # Let the component import raise the real (informative) error later.
        return
    component_dir = os.path.join(_ROOT, "components")
    try:
        import esphome.components as _ec

        if component_dir not in _ec.__path__:
            _ec.__path__.append(component_dir)
    except ImportError:
        # esphome.components is not itself a package in this distribution:
        # synthesize a namespace module instead.
        synth = types.ModuleType("esphome.components")
        synth.__path__ = [component_dir]
        sys.modules["esphome.components"] = synth


setup()

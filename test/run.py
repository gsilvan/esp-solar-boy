#!/usr/bin/env python3
"""Run host firmware regressions, settings interaction, and simulator checks."""
import os
import json
import tomllib
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
os.environ["PYTHONDONTWRITEBYTECODE"] = "1"

def verify_preset_files(directory):
    with (directory / "presets.json").open() as source:
        loaded = json.load(source)
    definitions = [tomllib.loads(path.read_text()) for path in (ROOT / "data" / "presets").glob("*.toml")]
    assert set(loaded) == {definition["id"] for definition in definitions}
    for definition in definitions:
        actual = loaded[definition["id"]]
        assert actual["requiresOverrides"] == definition["requires_overrides"]
        for index, key in enumerate(("soc", "battery", "plant", "grid")):
            expected, value = definition[key], actual["telemetry"][index]
            for field in ("register", "unit", "type", "scale", "invert"):
                assert str(value[field]) == str(expected[field]), (key, field)
            assert value["order"] == expected["word_order"]
            assert value["unavailable"] == expected["ignore_no_data"]
            assert not value["unavailable"]


with tempfile.TemporaryDirectory(prefix="solarboy-tests-") as output:
    for platform in ("ESP32", "ESP8266"):
        directory = Path(output) / platform
        directory.mkdir()
        executable = str(directory / "inverter-tests")
        subprocess.run([
            "g++", "-std=c++11", "-D" + platform, "-Wall", "-Wextra", "-Werror", "-Wno-attributes",
            "-fsanitize=undefined,address", "-fno-omit-frame-pointer", "-g",
            "-Itest/host/stubs", "-Isrc", "-Iinclude", "test/host/test_inverter.cpp",
            "src/Inverter.cpp", "src/SmartRelay.cpp", "src/DevicePresets.cpp", "-o", executable,
        ], check=True)
        subprocess.run([executable, str(directory)], check=True)
        verify_preset_files(directory)
        subprocess.run(["node", "test/settings.test.js", str(directory)], check=True)
subprocess.run(["python3", "-m", "unittest", "discover", "-s", "test", "-p", "test_*.py"], check=True)

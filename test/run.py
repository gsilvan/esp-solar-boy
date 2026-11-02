#!/usr/bin/env python3
"""Run host firmware regressions, settings interaction, and simulator checks."""
import os
import csv
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
os.environ["PYTHONDONTWRITEBYTECODE"] = "1"
ARDUINO_JSON = ROOT / ".pio" / "libdeps" / "esp32" / "ArduinoJson" / "src"
if not (ARDUINO_JSON / "ArduinoJson.h").exists():
    raise SystemExit("Install dependencies with 'pio pkg install -e esp32' before running tests.")

def verify_preset_files(directory):
    definitions = {config["id"]: config for path in (ROOT / "data" / "presets").glob("*.json")
                   for config in [json.loads(path.read_text())]}
    with (directory / "presets.tsv").open() as source:
        loaded = list(csv.DictReader(source, delimiter="\t"))
    assert len(loaded) == len(definitions) * 4
    assert {row["preset"] for row in loaded} == set(definitions)
    for row in loaded:
        definition = definitions[row["preset"]]
        expected = definition[row["reading"]]
        assert bool(int(row["requires_overrides"])) == definition["requires_overrides"]
        for field in ("register", "unit"):
            assert int(row[field]) == expected[field]
        assert row["type"] == expected["type"]
        assert row["order"] == expected["word_order"]
        assert float(row["scale"]) == expected["scale"]
        assert bool(int(row["invert"])) == expected["invert"]
        assert bool(int(row["unavailable"])) == expected["ignore_no_data"]
        assert not expected["ignore_no_data"]


with tempfile.TemporaryDirectory(prefix="solarboy-tests-") as output:
    for platform in ("ESP32", "ESP8266"):
        directory = Path(output) / platform
        directory.mkdir()
        executable = str(directory / "inverter-tests")
        subprocess.run([
            "g++", "-std=c++11", "-D" + platform, "-Wall", "-Wextra", "-Werror", "-Wno-attributes",
            "-fsanitize=undefined,address", "-fno-omit-frame-pointer", "-g",
            "-Itest/host/stubs", "-Isrc", "-Iinclude", "-I" + str(ARDUINO_JSON), "test/host/test_inverter.cpp",
            "src/Inverter.cpp", "src/SmartRelay.cpp", "src/DevicePresets.cpp", "-o", executable,
        ], check=True)
        subprocess.run([executable, str(directory)], check=True)
        verify_preset_files(directory)
        subprocess.run(["node", "test/settings.test.js", str(directory)], check=True)
subprocess.run(["python3", "-m", "unittest", "discover", "-s", "test", "-p", "test_*.py"], check=True)

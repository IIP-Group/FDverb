"""
yaml_to_preset.py — convert FDverb YAML presets to JUCE APVTS XML.

Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
SPDX-License-Identifier: AGPL-3.0-or-later

Usage:
    python python/scripts/yaml_to_preset.py [preset_dir] [output_dir]

Defaults:
    preset_dir  = presets/
    output_dir  = presets/generated/

The generated XML files can be embedded via juce_add_binary_data and loaded
by FDverbAudioProcessor::setCurrentProgram(). The XML format matches what
apvts.copyState().createXml() produces: a <Parameters> root with <PARAM>
children carrying the *denormalized* (engineering-unit) value for each
parameter, plus the choice index for AudioParameterChoice (FFTSIZE).

Run this script from the repo root whenever a preset YAML changes, then
commit the updated XML alongside the source change.

NOTE: presets/default.local.yaml is gitignored (*.local.* pattern). Its
generated XML (default-local.xml) CAN be committed, but regenerating it
on a fresh clone requires the source YAML to be present manually.
"""

import sys
import os
import math
import yaml

# ---------------------------------------------------------------------------
# Presets to convert (filenames relative to preset_dir, in display order).
# plot_* and *.local.* are excluded from the factory set by default unless
# listed here explicitly.
# ---------------------------------------------------------------------------
PRESET_FILES = [
    ("default.yaml",        "Default"),
    ("dark-room.yaml",      "Dark Room"),
    ("live-room.yaml",      "Live Room"),
    ("bell.yaml",           "Bell"),
    ("gated.yaml",          "Gated"),
    ("aura.yaml",           "Aura"),
    ("shifter.yaml",        "Shifter"),
    ("drifter.yaml",        "Drifter"),
]

# ---------------------------------------------------------------------------
# FFTSIZE choices (must match PluginProcessor.cpp createParameterLayout)
# AudioParameterChoice: stored value = choice index (float)
# ---------------------------------------------------------------------------
FFTSIZE_CHOICES = [32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384]


def fftsize_to_index(size):
    """Convert FFT size integer to choice index. Raises if not a valid choice."""
    if size not in FFTSIZE_CHOICES:
        raise ValueError(f"FFT size {size} not in choices {FFTSIZE_CHOICES}")
    return float(FFTSIZE_CHOICES.index(size))


def clamp(value, lo, hi):
    return max(lo, min(hi, value))


def yaml_to_params(cfg):
    """
    Map a loaded YAML config dict to plugin parameter id→value pairs.

    Values are denormalized (engineering units) exactly as JUCE stores them
    in the APVTS ValueTree — i.e. what apvts.copyState().createXml() writes.
    Exceptions:
      - FFTSIZE: stored as choice index (float), not the raw FFT size
      - ENVMODE: stored as bool float (0.0 = exponential, 1.0 = arithmetic)
      - LINK:    stored as bool float
      - FREEZE, FREEZEINPUT: always 0.0 (foreverb section excluded per spec)

    Skipped YAML keys (no plugin parameter):
      - envelope.attack          [python only]
      - early_reflections.decay_scale  [no plugin param]
      - early_reflections.er2tail_gain [python only]
      - early_reflections.er_preset    [python only]
      - foreverb.*               [excluded by user]
      - debug.*                  [excluded by user]
    """
    p = {}

    # fft
    p["FFTSIZE"] = fftsize_to_index(int(cfg["fft"]["size"]))

    # envelope
    env_mode = cfg["envelope"]["env_mode"]
    if env_mode == "exponential":
        p["ENVMODE"] = 0.0
    elif env_mode == "arithmetic":
        p["ENVMODE"] = 1.0
    else:
        raise ValueError(f"Unknown env_mode: {env_mode!r} (expected 'exponential' or 'arithmetic')")

    p["DECAY"] = clamp(float(cfg["envelope"]["decay"]), 100.0, 10000.0)
    p["COLOR"] = clamp(float(cfg["envelope"]["color"]), 0.5, 2.0)
    p["TILT"]  = clamp(float(cfg["envelope"]["tilt"]),  -1.0, 1.0)
    p["SHIFT"] = clamp(float(cfg["envelope"]["shift"]), -100.0, 100.0)
    p["DRIFT"] = clamp(float(cfg["envelope"]["drift"]), -100.0, 100.0)

    # stereo
    p["LINK"]  = 1.0 if cfg["stereo"]["link"] else 0.0
    p["WIDTH"] = clamp(float(cfg["stereo"]["width"]), 0.0, 2.0)

    # eq — clamp to plugin parameter ranges
    p["LOWCUT"]  = clamp(float(cfg["eq"]["low_cut"]),   20.0, 2000.0)
    p["HIGHCUT"] = clamp(float(cfg["eq"]["high_cut"]), 1000.0, 20000.0)

    # early reflections (decay_scale excluded — no plugin parameter)
    p["ERTAPS"]    = clamp(float(cfg["early_reflections"]["taps"]),      0.0, 32.0)
    p["PREDELAY"]  = clamp(float(cfg["early_reflections"]["predelay"]),  1.0, 500.0)
    p["TAILDELAY"] = clamp(float(cfg["early_reflections"]["taildelay"]), 1.0, 500.0)

    # mix — gainRange is -60..0; values below -60 snap to silence in JUCE
    # Clamp to [-60, 0] so the XML matches what JUCE would store after snapping.
    p["DRY"]     = clamp(float(cfg["mix"]["dry_level"]),  -60.0, 0.0)
    p["ERLEVEL"] = clamp(float(cfg["mix"]["er_level"]),   -60.0, 0.0)
    p["TAIL"]    = clamp(float(cfg["mix"]["tail_level"]), -60.0, 0.0)

    # foreverb: always off in presets (section excluded per spec)
    p["FREEZE"]      = 0.0
    p["FREEZEINPUT"] = 0.0

    return p


# Parameter order matches createParameterLayout() for readability (not required).
PARAM_ORDER = [
    "DRY", "TAIL", "PREDELAY", "DECAY", "TILT",
    "LOWCUT", "HIGHCUT", "LINK", "TAILDELAY", "WIDTH",
    "ERTAPS", "ERLEVEL", "COLOR", "SHIFT", "DRIFT",
    "ENVMODE", "FFTSIZE", "FREEZE", "FREEZEINPUT",
]


def params_to_xml(params, preset_name):
    """
    Emit XML matching apvts.copyState().createXml() structure:

        <Parameters presetName="..." version="1">
          <PARAM id="DRY" value="-6.0"/>
          ...
        </Parameters>

    The version attribute is used by setStateInformation for future migration.
    """
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<Parameters presetName="{_escape_xml(preset_name)}" version="1">',
    ]
    for pid in PARAM_ORDER:
        val = params[pid]
        # Format: avoid unnecessary decimals but always include at least one
        if val == int(val):
            val_str = f"{int(val)}.0"
        else:
            val_str = f"{val:.6g}"
        lines.append(f'  <PARAM id="{pid}" value="{val_str}"/>')
    lines.append("</Parameters>")
    return "\n".join(lines) + "\n"


def _escape_xml(s):
    return s.replace("&", "&amp;").replace('"', "&quot;").replace("<", "&lt;").replace(">", "&gt;")


def yaml_filename_to_xml_name(yaml_filename):
    """
    Map a YAML filename to the output XML filename.
    Avoids *.local.* pattern (gitignored) by using a hyphen separator.
    """
    base = os.path.splitext(yaml_filename)[0]  # strip .yaml
    # Replace .local. with -local (avoids gitignore *.local.* match)
    base = base.replace(".local", "-local")
    return base + ".xml"


def convert(preset_dir, output_dir):
    os.makedirs(output_dir, exist_ok=True)
    converted = []
    skipped = []

    for yaml_file, display_name in PRESET_FILES:
        src_path = os.path.join(preset_dir, yaml_file)
        if not os.path.exists(src_path):
            print(f"  SKIP (not found): {yaml_file}")
            skipped.append(yaml_file)
            continue

        with open(src_path, "r") as f:
            cfg = yaml.safe_load(f)

        params = yaml_to_params(cfg)
        xml_name = yaml_filename_to_xml_name(yaml_file)
        out_path = os.path.join(output_dir, xml_name)

        xml = params_to_xml(params, display_name)
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(xml)

        print(f"  OK  {yaml_file} -> {xml_name}  ({display_name})")
        converted.append(xml_name)

    print(f"\nConverted {len(converted)} preset(s). Skipped {len(skipped)}.")
    if skipped:
        print("Skipped:", skipped)
    return converted


if __name__ == "__main__":
    repo_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    preset_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(repo_root, "presets")
    output_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(repo_root, "presets", "generated")

    print(f"Source presets : {preset_dir}")
    print(f"Output dir     : {output_dir}")
    print()

    convert(preset_dir, output_dir)

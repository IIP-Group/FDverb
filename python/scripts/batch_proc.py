# batch_proc.py - apply FDverb notebook to combinations of audio input files and parameters
#
# Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
# SPDX-License-Identifier: AGPL-3.0-or-later
#
#
# Run this script as
# ```
# cd python
# python scripts/batch_proc.py
# ```
# output path is set below


import papermill as pm
import os
import warnings
import hashlib

temp_notebook_dir = "tmp/"


def get_audio_files(audio_dir):
    return [f for f in os.listdir(audio_dir) if f.endswith(".wav")]


def run_fdverb(config_file, input_file, output_file):

    notebook_path = temp_notebook_dir + infile.replace(
        ".wav", "_" + config.replace(".yaml", "") + ".local.ipynb"
    )

    pm.execute_notebook(
        "FDverb.ipynb",
        notebook_path,
        parameters={
            "config_file": config_file,
            "input_file": input_file,
            "output_file": output_file,
            "write_audio_to_file": True,
        },
    )


def wav_hash(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def check_wav_duplicates(file_list):
    """Warn for each WAV file in file_list that is identical to an earlier one."""
    seen = {}
    for path in file_list:
        digest = wav_hash(path)
        if digest in seen:
            warnings.warn(f"Duplicate WAV files: '{path}' == '{seen[digest]}'")
        else:
            seen[digest] = path


if __name__ == "__main__":

    config_list = [
        "default.yaml",
        "dark-room.yaml",
        "aura.yaml",
        "drifter.yaml"
    ]

    input_dir = "audio_in"
    # infile_list = get_audio_files(input_dir)
    infile_list = [
        "organ.wav",
        "drum-fill.wav",
        "piano.wav"
    ]

    output_dir = "out/batch"

    os.makedirs(temp_notebook_dir, exist_ok=True)

    num_combinations = len(config_list) * len(infile_list)
    i = 0
    for infile in infile_list:
        for config in config_list:
            i += 1
            print(f"Running {config} on {infile} ({i} of {num_combinations})")
            outfile = infile.replace(".wav", "_" + config.replace(".yaml", "") + ".wav")
            run_fdverb(
                config,
                os.path.join(input_dir, infile),
                os.path.join(output_dir, outfile),
            )

    # check for duplicates to catch setup mistakes
    output_files = [os.path.join(output_dir, f) for f in get_audio_files(output_dir)]
    check_wav_duplicates(output_files)

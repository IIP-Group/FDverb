# utils.py - helper functions for the FDverb Python implementation
# 
# Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
# SPDX-License-Identifier: AGPL-3.0-or-later

import yaml
import numpy as np

def load_fdverb_config(config_file='../presets/default.yaml'):
    """
    Load fdverb configuration parameters from yaml file
    Args:
        config_file (str): Path to the configuration YAML file
    Returns:
        dict: Dictionary containing all parameters from the config file
    """
    with open(config_file, 'r') as f:
        config = yaml.safe_load(f)

    params = {}

    # fft
    params['N'] = config['fft']['size']

    # envelope
    params['ENVMODE'] = config['envelope']['env_mode'] # 'exponential', 'arithmetic', 'forever'
    params['ATTACK'] = config['envelope']['attack']
    params['DECAY'] = config['envelope']['decay']
    params['COLOR'] = config['envelope']['color']
    params['TILT'] = config['envelope']['tilt']
    params['SHIFT'] = config['envelope']['shift']
    params['DRIFT'] = config['envelope']['drift']

    # stereo
    params['LINK'] = config['stereo']['link']
    params['WIDTH'] = config['stereo']['width']

    # eq
    params['LOWCUT'] = config['eq']['low_cut']
    params['HIGHCUT'] = config['eq']['high_cut']

    # early reflections
    params['ERTAPS'] = config['early_reflections']['taps']
    params['PREDELAY'] = config['early_reflections']['predelay']
    params['TAILDELAY'] = config['early_reflections']['taildelay']
    params['ERDECAYSCALE'] = config['early_reflections']['decay_scale']
    params['ER2TAIL'] = config['early_reflections']['er2tail_gain']
    params['ERPRESET'] = config['early_reflections']['er_preset']

    # mix
    params['DRYLEVEL'] = config['mix']['dry_level']
    params['ERLEVEL'] = config['mix']['er_level']
    params['TAILLEVEL'] = config['mix']['tail_level']

    # foreverb
    params['FOREVERB'] = config['foreverb']['enabled']
    params['FREEZEINPUT'] = config['foreverb']['freeze_input']

    # debug
    params['DBG_TRANSFORMONLY'] = config['debug']['transform_only']
    params['DBG_DEVOPT'] = config['debug']['dev_option']
    params['DBG_IGNORECAUSALITY'] = config['debug']['ignore_causality']
    # validate parameters
    # assert params['ENVMODE']
    assert params['N'] > 0 and (params['N'] & (params['N'] - 1) == 0), "FFT size (N) must be a power of two"
    assert params['ATTACK'] > 0, "attack must be positive"
    assert params['DECAY'] > 0, "decay must be positive"
    # assert params['DECAYMODE']
    assert params['COLOR'] > 0, "color must be positive"
    assert -1 <= params['TILT'] <= 1, "tilt must be between -1 and 1"
    assert 0 <= params['WIDTH'] <= 2, "width must be between 0 and 2"
    assert params['LINK'] in [True, False], "stereo link must be True or False"
    assert params['LOWCUT'] >= 0, "low cut must be non-negative"
    assert params['HIGHCUT'] >= 0, "high cut must be non-negative"
    assert params['ERTAPS'] >= 0, "number of early reflections taps must be non-negative"
    assert params['PREDELAY'] >= 0, "predelay must be non-negative"
    assert params['TAILDELAY'] > params['PREDELAY'], "tail delay must be greater than predelay"
    assert params['ERDECAYSCALE'] >= 0, "ER decay scale must not be negative"
    assert params['ER2TAIL'] >= 0, "ER to tail gain must be non-negative"
    assert params['ERPRESET'] in [None, 'boston_er'], "er_preset must be None or 'boston_er'"
    # assert params['DRYLEVEL']
    # assert params['ERLEVEL']
    # assert params['TAILLEVEL']

    return params

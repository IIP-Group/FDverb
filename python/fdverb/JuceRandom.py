# JuceRandom - mimics juce::Random in Python
# 
# Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
# SPDX-License-Identifier: AGPL-3.0-or-later

class JuceRandom:
    """Bit-exact reimplementation of juce::Random's LCG."""
    _MASK48 = (1 << 48) - 1
    _MASK32 = (1 << 32) - 1
    _MULT   = 0x5deece66d
    _FLOAT32_EPS = 1.1920929e-07

    def __init__(self, seed: int):
        self.seed = seed

    def next_int(self) -> int:
        self.seed = ((self.seed * self._MULT) + 11) & self._MASK48
        val = (self.seed >> 16) & self._MASK32
        return val - (1 << 32) if val >= (1 << 31) else val

    def next_float(self) -> float:
        # matches juce::Random::nextFloat() exactly, including its clamp
        u = self.next_int() & self._MASK32
        f = u / 2**32
        return min(f, 1.0 - self._FLOAT32_EPS)

    def uniform(self, low: float = 0.0, high: float = 1.0) -> float:
        return low + (high - low) * self.next_float()

    def random(self) -> float:
        return self.next_float()

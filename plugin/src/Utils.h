// Copyright (C) 2026 Integrated Information Processing (IIP) group, ETH Zurich: https://iip.ethz.ch
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cmath>

namespace FdverbConstants
{
    constexpr float SILENCE_THRESHOLD_DB = -60.0f;
}

using namespace FdverbConstants;

inline float dbToLinear(float db)
{
    return (db > SILENCE_THRESHOLD_DB) ? std::pow(10.0f, db / 20.0f) : 0.0f;
}

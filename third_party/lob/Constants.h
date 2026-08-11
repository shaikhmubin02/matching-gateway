#pragma once

#include <limits>

#include "Usings.h"

struct Constants
{
    // Sentinel for unpriced market orders (Price is an integer type).
    static constexpr Price InvalidPrice = std::numeric_limits<Price>::min();
};

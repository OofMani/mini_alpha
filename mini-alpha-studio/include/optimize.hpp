#pragma once
#include "strategy.hpp"
#include <string>
#include <vector>

struct OptResult {
    int best_fast = 0;
    int best_slow = 0;
    double best_score = -1e300;  // higher is better
};

OptResult grid_search_fast_slow(const std::vector<std::string>& csv_paths,
                                const MAParams& base,   // use base.fee_bps & slippage
                                int fast_min, int fast_max,
                                int slow_min, int slow_max);

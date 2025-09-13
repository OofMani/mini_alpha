#pragma once
#include "model.hpp"
#include <vector>

struct MAParams {
    int   fast = 20;
    int   slow = 50;
    float fee_bps = 1.0f;       // per trade, float so ImGui::SliderFloat works
    float slippage_bps = 2.0f;  // per trade
};

struct BacktestPoint {
    int64_t ts_ms;
    double  px;
    double  equity;
};

struct BacktestResult {
    std::vector<BacktestPoint> curve;
    double pnl  = 0.0;   // ending equity
    double max_dd = 0.0; // absolute drawdown
    double sharpe = 0.0; // placeholder
};

// Simple moving-average crossover (+ basic costs)
BacktestResult run_ma_crossover(const std::vector<Bar>& bars, const MAParams& p);

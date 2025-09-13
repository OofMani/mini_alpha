#pragma once
#include "model.hpp"
#include <vector>

// ---- Parameters ----
struct MAParams {
    int   fast = 20;
    int   slow = 50;
    float fee_bps = 1.0f;       // per trade
    float slippage_bps = 2.0f;  // per trade
};

// ---- Backtest outputs ----
struct BacktestPoint {
    int64_t ts_ms;
    double  px;
    double  equity;
};

struct Trade {
    size_t  idx;     // bar index in the input series (for plotting quickly)
    int64_t ts_ms;   // timestamp at the trade
    double  px;      // trade price (pre-cost)
    int     dir;     // +1 = buy/open; -1 = sell/close
};

struct BacktestResult {
    std::vector<BacktestPoint> curve;
    std::vector<Trade>         trades;  // <--- NEW
    double pnl   = 0.0;   // ending equity
    double max_dd= 0.0;   // absolute drawdown
    double sharpe= 0.0;   // placeholder
};

// Simple moving-average crossover (+ basic costs)
BacktestResult run_ma_crossover(const std::vector<Bar>& bars, const MAParams& p);

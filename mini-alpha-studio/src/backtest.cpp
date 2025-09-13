#include "strategy.hpp"
#include <algorithm>
#include <cmath>

static std::vector<double> sma(const std::vector<Bar>& b, int w) {
    std::vector<double> m(b.size(), NAN);
    if (w <= 0 || b.empty()) return m;
    double s = 0.0;
    for (size_t i = 0; i < b.size(); ++i) {
        s += b[i].close;
        if (i + 1 >= (size_t)w) {
            if (i + 1 > (size_t)w) s -= b[i - w].close;
            m[i] = s / w;
        }
    }
    return m;
}

BacktestResult run_ma_crossover(const std::vector<Bar>& bars, const MAParams& p) {
    BacktestResult r;
    if (bars.empty() || p.fast <= 0 || p.slow <= 0 || p.fast >= p.slow) return r;

    auto mf = sma(bars, p.fast);
    auto ms = sma(bars, p.slow);

    int    pos   = 0;    // 0 or 1 share
    double cash  = 0.0;
    double equity= 0.0;
    double peak  = 0.0, dd = 0.0;

    auto trade_costed = [&](double px, int dir /*+1 buy, -1 sell*/) {
        const double bps = (static_cast<double>(p.fee_bps) + static_cast<double>(p.slippage_bps)) / 10000.0;
        return (dir > 0) ? px * (1.0 + bps) : px * (1.0 - bps);
    };

    for (size_t i = 0; i < bars.size(); ++i) {
        if (std::isnan(mf[i]) || std::isnan(ms[i])) continue;

        const double px = bars[i].close;
        int want = pos;
        if (mf[i] > ms[i] && pos == 0) want = 1;
        if (mf[i] < ms[i] && pos == 1) want = 0;

        if (want != pos) {
            if (want == 1) { cash -= trade_costed(px, +1); pos = 1; }
            else           { cash += trade_costed(px, -1); pos = 0; }
        }

        equity = cash + pos * px;
        peak   = std::max(peak, equity);
        dd     = std::max(dd, peak - equity);

        r.curve.push_back({bars[i].ts_ms, px, equity});
    }

    r.pnl    = equity;
    r.max_dd = dd;
    r.sharpe = 0.0; // simple
    return r;
}

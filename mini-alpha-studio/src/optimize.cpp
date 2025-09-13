#include "optimize.hpp"
#include "csv.hpp"
#include <algorithm>

static double score_run(const BacktestResult& r){
    // Simple score: avg PnL over files, lightly penalize drawdown
    return r.pnl / (1.0 + r.max_dd);
}

OptResult grid_search_fast_slow(const std::vector<std::string>& csv_paths,
                                const MAParams& base,
                                int fast_min, int fast_max,
                                int slow_min, int slow_max)
{
    OptResult out;
    if (csv_paths.empty()) return out;

    // Preload all files once
    std::vector<std::vector<Bar>> datasets;
    datasets.reserve(csv_paths.size());
    for (auto& path : csv_paths){
        std::string warn, err;
        auto bars = load_csv(path, warn, err);
        if (!err.empty() || bars.empty()) continue;
        datasets.push_back(std::move(bars));
    }
    if (datasets.empty()) return out;

    for (int f = fast_min; f <= fast_max; ++f){
        for (int s = std::max(slow_min, f+1); s <= slow_max; ++s){
            double total = 0.0; int used = 0;
            MAParams p = base; p.fast = f; p.slow = s;
            for (auto& ds : datasets){
                auto r = run_ma_crossover(ds, p);
                total += score_run(r);
                ++used;
            }
            if (used>0 && total/used > out.best_score){
                out.best_score = total/used;
                out.best_fast  = f;
                out.best_slow  = s;
            }
        }
    }
    return out;
}

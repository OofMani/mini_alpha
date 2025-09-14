#include <SDL.h>
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include "optimize.hpp"

#if __APPLE__
#  include <OpenGL/gl3.h>
#else
#  include <GL/gl.h>
#endif

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

#include "csv.hpp"
#include "strategy.hpp"

static void export_run(const BacktestResult& r) {
    // ensure reports directory exists relative to CWD
    std::filesystem::create_directories("reports");

    // CSV
    std::ofstream c("reports/run.csv");
    c << "ts_ms,price,equity\n";
    for (const auto& p : r.curve) c << p.ts_ms << "," << p.px << "," << p.equity << "\n";

    // Minimal HTML (Plotly CDN)
    std::ofstream h("reports/run.html");
    h << R"(<!doctype html><meta charset="utf-8"><title>Run Report</title>
<script src="https://cdn.plot.ly/plotly-2.32.0.min.js"></script>
<div id="plot" style="width:100%;height:75vh"></div>
<script>
const ts=[)";
    for (size_t i = 0; i < r.curve.size(); ++i) { if (i) h << ","; h << r.curve[i].ts_ms; }
    h << "], px=[";
    for (size_t i = 0; i < r.curve.size(); ++i) { if (i) h << ","; h << r.curve[i].px; }
    h << "], eq=[";
    for (size_t i = 0; i < r.curve.size(); ++i) { if (i) h << ","; h << r.curve[i].equity; }
    h << R"(];
Plotly.newPlot('plot',[
  {x:ts,y:eq,name:'Equity',mode:'lines'},
  {x:ts,y:px,name:'Price',mode:'lines',yaxis:'y2'}
],{
  title:'Mini-Alpha Studio — MA Crossover',
  xaxis:{title:'Time (ms)'},
  yaxis:{title:'Equity'},
  yaxis2:{title:'Price',overlaying:'y',side:'right'}
});
</script>)";
}

static void DrawPriceWithTrades(const std::vector<BacktestPoint>& curve,
                                const std::vector<Trade>& trades,
                                float height_px = 300.0f)
{
    if (curve.empty()) { ImGui::TextDisabled("No data"); return; }

    // Canvas area
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float  w  = ImGui::GetContentRegionAvail().x;
    float  h  = height_px;
    ImVec2 p1 = ImVec2(p0.x + w, p0.y + h);

    auto* draw = ImGui::GetWindowDrawList();
    // Frame
    draw->AddRect(p0, p1, IM_COL32(180,180,180,255));

    // Find min/max price
    double min_px = curve.front().px, max_px = curve.front().px;
    for (auto& p : curve) { if (p.px < min_px) min_px = p.px; if (p.px > max_px) max_px = p.px; }
    if (max_px <= min_px) max_px = min_px + 1.0; // avoid div by zero

    const int N = (int)curve.size();
    const float x_step = (N > 1) ? (w / float(N - 1)) : 0.0f;

    // Line segments
    for (int i=1; i<N; ++i) {
        float x0 = p0.x + (i-1) * x_step;
        float x1 = p0.x + (i)   * x_step;
        float y0 = p0.y + (float)(1.0 - ( (curve[i-1].px - min_px) / (max_px - min_px) )) * h;
        float y1 = p0.y + (float)(1.0 - ( (curve[i].px   - min_px) / (max_px - min_px) )) * h;
        draw->AddLine(ImVec2(x0,y0), ImVec2(x1,y1), IM_COL32(200,200,255,255), 1.5f);
    }

    // Trades (filled circles)
    const float R = 4.0f;
    for (auto& tr : trades) {
        if (tr.idx >= curve.size()) continue;
        float x = p0.x + (float)tr.idx * x_step;
        float y = p0.y + (float)(1.0 - ( (curve[tr.idx].px - min_px) / (max_px - min_px) )) * h;

        if (tr.dir > 0) {
            // Buy = green
            draw->AddCircleFilled(ImVec2(x,y), R, IM_COL32(40,200,90,255));
            draw->AddCircle(ImVec2(x,y), R, IM_COL32(10,150,60,255), 0, 1.5f);
        } else {
            // Sell = red
            draw->AddCircleFilled(ImVec2(x,y), R, IM_COL32(220,70,70,255));
            draw->AddCircle(ImVec2(x,y), R, IM_COL32(160,40,40,255), 0, 1.5f);
        }
    }

    // Legend
    draw->AddRectFilled(ImVec2(p1.x-130, p0.y+8), ImVec2(p1.x-10, p0.y+46), IM_COL32(0,0,0,120), 6.0f);
    draw->AddText(ImVec2(p1.x-120, p0.y+12), IM_COL32(200,200,255,255), "Price");
    draw->AddCircleFilled(ImVec2(p1.x-92, p0.y+30), R, IM_COL32(40,200,90,255));
    draw->AddText(ImVec2(p1.x-82,  p0.y+24), IM_COL32(230,230,230,255), "Buy");
    draw->AddCircleFilled(ImVec2(p1.x-48, p0.y+30), R, IM_COL32(220,70,70,255));
    draw->AddText(ImVec2(p1.x-38,  p0.y+24), IM_COL32(230,230,230,255), "Sell");

    // Advance the ImGui cursor so following items don’t overlap this canvas
    ImGui::Dummy(ImVec2(w, h + 6.0f));
}


int main() {
    // --- SDL + OpenGL init ---
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Mini-Alpha Studio",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1200, 700, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, gl);
    ImGui_ImplOpenGL3_Init("#version 150");

    // --- Load CSV (relative to CWD) ---
    std::string warn, err;
    auto bars = load_csv("sample_data/TSLA_5Y.csv", warn, err);

    // --- Backtest state ---
    MAParams params;
    BacktestResult result = run_ma_crossover(bars, params);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Controls / stats
        ImGui::Begin("Controls");
        ImGui::Text("Bars loaded: %zu", bars.size());
        if (!warn.empty()) ImGui::TextColored(ImVec4(1,0.8f,0.2f,1), "WARN: %s", warn.c_str());
        if (!err.empty())  ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "ERR: %s", err.c_str());

        int fast = params.fast, slow = params.slow;
        ImGui::SliderInt("Fast MA", &fast, 2, 200);
        ImGui::SliderInt("Slow MA", &slow, 5, 400);
        ImGui::SliderFloat("Fee (bps)", &params.fee_bps, 0.0f, 10.0f);
        ImGui::SliderFloat("Slippage (bps)", &params.slippage_bps, 0.0f, 20.0f);

        bool recompute = false;
        if (fast != params.fast || slow != params.slow) {
            if (fast < slow) { params.fast = fast; params.slow = slow; recompute = true; }
            else ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "Fast must be < Slow");
        }
        if (ImGui::Button("Recompute")) recompute = true;
        if (recompute) result = run_ma_crossover(bars, params);

        ImGui::Separator();
        ImGui::Text("PnL: %.2f | Max DD: %.2f | Sharpe (placeholder): %.2f",
                    result.pnl, result.max_dd, result.sharpe);

        if (ImGui::Button("Export CSV + HTML")) {
            export_run(result);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(writes to ./reports/)");

        static bool show_opt = false;
if (ImGui::Button("Optimize Fast/Slow (grid)")) show_opt = true;

if (show_opt) {
    ImGui::Separator();
    ImGui::Text("Grid search over multiple CSVs");
    static char p0[256] = "sample_data/TSLA_5Y.csv";
    static char p1[256] = "sample_data/MSFT_5Y.csv";
    static char p2[256] = "sample_data/NVDA_5Y.csv";
    static char p3[256] = "sample_data/AAPL_5Y.csv";
    static char p4[256] = "";
    ImGui::InputText("CSV #1", p0, sizeof(p0));
    ImGui::InputText("CSV #2", p1, sizeof(p1));
    ImGui::InputText("CSV #3", p2, sizeof(p2));
    ImGui::InputText("CSV #4", p3, sizeof(p3));
    ImGui::InputText("CSV #5", p4, sizeof(p4));
    static int fmin=5,fmax=60,smin=20,smax=200;
    ImGui::InputInt("fast min", &fmin); ImGui::SameLine(); ImGui::InputInt("fast max", &fmax);
    ImGui::InputInt("slow min", &smin); ImGui::SameLine(); ImGui::InputInt("slow max", &smax);

    if (ImGui::Button("Run grid search")) {
        std::vector<std::string> paths;
        if (p0[0]) paths.push_back(p0);
        if (p1[0]) paths.push_back(p1);
        if (p2[0]) paths.push_back(p2);
        if (p3[0]) paths.push_back(p3);
        if (p4[0]) paths.push_back(p4);

        auto opt = grid_search_fast_slow(paths, params, fmin, fmax, smin, smax);
        if (opt.best_fast>0) {
            params.fast = opt.best_fast;
            params.slow = opt.best_slow;
            result = run_ma_crossover(bars, params);
        }
    }
}

        ImGui::End();

        // Equity plot
        ImGui::Begin("Equity Curve");
        static std::vector<float> eq;
        eq.clear(); eq.reserve(result.curve.size());
        for (auto& p : result.curve) eq.push_back((float)p.equity);
        if (!eq.empty()) ImGui::PlotLines("Equity", eq.data(), (int)eq.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(-1, 300));
        ImGui::End();

        // Price plot
        ImGui::Begin("Price (with trades)");
        DrawPriceWithTrades(result.curve, result.trades, 300.0f);
        ImGui::End();


        // Render
        ImGui::Render();
        int w, h; SDL_GetWindowSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

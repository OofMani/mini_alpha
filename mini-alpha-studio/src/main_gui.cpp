#include <SDL.h>
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

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
    auto bars = load_csv("sample_data/spy_1min.csv", warn, err);

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

        ImGui::End();

        // Equity plot
        ImGui::Begin("Equity Curve");
        static std::vector<float> eq;
        eq.clear(); eq.reserve(result.curve.size());
        for (auto& p : result.curve) eq.push_back((float)p.equity);
        if (!eq.empty()) ImGui::PlotLines("Equity", eq.data(), (int)eq.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(-1, 300));
        ImGui::End();

        // Price plot
        ImGui::Begin("Price");
        static std::vector<float> px;
        px.clear(); px.reserve(result.curve.size());
        for (auto& p : result.curve) px.push_back((float)p.px);
        if (!px.empty()) ImGui::PlotLines("Close", px.data(), (int)px.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(-1, 300));
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

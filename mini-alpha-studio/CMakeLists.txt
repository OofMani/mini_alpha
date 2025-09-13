cmake_minimum_required(VERSION 3.22)
project(mini_alpha_studio LANGUAGES CXX)

# ---------- Global C++ setup ----------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Force Apple Silicon builds to arm64 (prevents accidental x86_64)
if(APPLE)
  # Users can still override from CLI if they want.
  if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build arch" FORCE)
  endif()
endif()

# ---------- ImGui (vendored) ----------
add_library(imgui
  third_party/imgui/imgui.cpp
  third_party/imgui/imgui_draw.cpp
  third_party/imgui/imgui_tables.cpp
  third_party/imgui/imgui_widgets.cpp
  third_party/imgui/backends/imgui_impl_sdl2.cpp
  third_party/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC third_party/imgui)

# ---------- SDL2 (prefer CONFIG; fallback to pkg-config) ----------
# Primary path: Homebrew installs SDL2 with a CMake config on macOS ARM.
#   cmake .. -DCMAKE_PREFIX_PATH=/opt/homebrew
find_package(SDL2 CONFIG)

if(SDL2_FOUND)
  message(STATUS "Using SDL2 via CMake CONFIG")
  target_link_libraries(imgui PUBLIC SDL2::SDL2)
  set(SDL2_LINK_TARGET SDL2::SDL2)
else()
  message(STATUS "SDL2 CMake CONFIG not found; falling back to pkg-config")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED sdl2)
  target_include_directories(imgui PUBLIC ${SDL2_INCLUDE_DIRS})
  target_link_libraries(imgui PUBLIC ${SDL2_LIBRARIES})
  set(SDL2_LINK_TARGET ${SDL2_LIBRARIES})
endif()

# ---------- OpenGL ----------
if(APPLE)
  find_library(OpenGL_LIB OpenGL)
else()
  find_package(OpenGL REQUIRED)
endif()

# ---------- GUI app ----------
add_executable(mini_alpha_gui
  src/main_gui.cpp
  src/csv.cpp   
  src/backtest.cpp        # GUI uses the CSV loader
)
target_include_directories(mini_alpha_gui PUBLIC include third_party/imgui)
target_link_libraries(mini_alpha_gui PRIVATE imgui ${SDL2_LINK_TARGET})
if(APPLE)
  target_link_libraries(mini_alpha_gui PRIVATE ${OpenGL_LIB})
  target_compile_definitions(mini_alpha_gui PRIVATE GL_SILENCE_DEPRECATION)
else()
  target_link_libraries(mini_alpha_gui PRIVATE OpenGL::GL)
endif()

# ---------- Pi streamer (also builds on Mac for convenience) ----------
add_executable(streamer_pi
  src/streamer_pi.cpp
  src/csv.cpp
  src/backtest.cpp
)
target_include_directories(streamer_pi PUBLIC include)
# streamer_pi doesn't need SDL/OpenGL

# ---------- Nice warnings (optional) ----------
if(MSVC)
  target_compile_options(imgui PRIVATE /W4)
  target_compile_options(mini_alpha_gui PRIVATE /W4)
  target_compile_options(streamer_pi PRIVATE /W4)
else()
  target_compile_options(imgui PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(mini_alpha_gui PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(streamer_pi PRIVATE -Wall -Wextra -Wpedantic)
endif()

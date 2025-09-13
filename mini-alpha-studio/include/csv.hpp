#pragma once
#include "model.hpp"
#include <string>
#include <vector>

// Load CSV of format: ts_ms,open,high,low,close,volume
std::vector<Bar> load_csv(const std::string& path,
                          std::string& warn,
                          std::string& err);

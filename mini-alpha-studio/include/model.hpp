#pragma once
#include <cstdint>

// A single OHLCV bar
struct Bar {
    int64_t ts_ms{};      // timestamp in milliseconds
    double open{}, high{}, low{}, close{}, volume{};
};

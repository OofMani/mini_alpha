#include "csv.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>

#if defined(_WIN32)
  #include <time.h>
  static inline time_t timegm_portable(std::tm* t){ return _mkgmtime(t); }
#else
  static inline time_t timegm_portable(std::tm* t){ return timegm(t); }
#endif

static inline void trim_cr(std::string& s){
  if (!s.empty() && (s.back() == '\r' || s.back()=='\n')) s.pop_back();
}

static inline std::string strip_chars(std::string s, const char* chars){
  for (const char* c = chars; *c; ++c){
    s.erase(std::remove(s.begin(), s.end(), *c), s.end());
  }
  return s;
}

static inline bool split6(const std::string& line, std::string out[6]){
  std::istringstream ss(line);
  for (int i=0;i<6;i++){
    if(!std::getline(ss, out[i], ',')) return false;
  }
  return true;
}

// Parse "MM/DD/YYYY" -> epoch ms at 00:00:00 UTC
static inline int64_t parse_date_ms(const std::string& mmddyyyy){
  int m=0,d=0,y=0;
  // Be tolerant of leading/trailing spaces
  std::string s = mmddyyyy;
  // manual parse
  char c1='/', c2='/';
  std::istringstream is(s);
  is >> m >> c1 >> d >> c2 >> y;
  if (!is || c1!='/' || c2!='/') return -1;

  std::tm tm{}; tm.tm_year = y - 1900; tm.tm_mon = m - 1; tm.tm_mday = d;
  tm.tm_hour = 0; tm.tm_min = 0; tm.tm_sec = 0;
  time_t t = timegm_portable(&tm);
  if (t < 0) return -1;
  return static_cast<int64_t>(t) * 1000;
}

std::vector<Bar> load_csv(const std::string& path, std::string& warn, std::string& err){
  std::vector<Bar> out;
  warn.clear(); err.clear();

  std::ifstream f(path);
  if(!f){ err = "Cannot open " + path; return out; }

  std::string header;
  if (!std::getline(f, header)){ err = "Empty file"; return out; }
  trim_cr(header);

  // Normalize header for detection (remove spaces, lower-case)
  auto norm = [](std::string s){
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
  };
  std::string H = norm(header);

  const bool schema_ts_ms =
      H.find("ts_ms") != std::string::npos && H.find("open") != std::string::npos;
  const bool schema_date_close_last =
      H.find("date") != std::string::npos && H.find("close/last") != std::string::npos;

  if (!schema_ts_ms && !schema_date_close_last){
    err = "Unrecognized header: " + header;
    return out;
  }

  std::string line; size_t ln = 1; // already read header

  if (schema_ts_ms){
    // ---- ORIGINAL SCHEMA: ts_ms,open,high,low,close,volume ----
    int64_t last=-1;
    while (std::getline(f, line)){
      ++ln; trim_cr(line);
      if(line.empty()) continue;

      std::istringstream ss(line); std::string tok; Bar b{};
      if(!std::getline(ss,tok,',')) { warn = "Malformed line " + std::to_string(ln); continue; }

      try {
        b.ts_ms = std::stoll(tok);
        auto rd=[&](double& d){
          if(!std::getline(ss,tok,',')) return false;
          d = std::stod(tok); return true;
        };
        if(!rd(b.open)||!rd(b.high)||!rd(b.low)||!rd(b.close)||!rd(b.volume)){
          warn = "Bad numeric at " + std::to_string(ln); continue;
        }
      } catch(...){
        warn = "Parse error at " + std::to_string(ln); continue;
      }

      if (b.ts_ms <= last) warn = "Non-monotonic ts at " + std::to_string(ln);
      last = b.ts_ms;
      out.push_back(b);
    }
  } else {
    // ---- NEW SCHEMA: Date,Close/Last,Volume,Open,High,Low ----
    // Example:
    // 09/12/2025,$395.94,168,156,400, $370.94, $396.6899, $370.24
    // (commas and $ signs must be stripped)
    std::vector<Bar> tmp;
    while (std::getline(f, line)){
      ++ln; trim_cr(line);
      if(line.empty()) continue;

      std::string cols[6];
      if (!split6(line, cols)){ warn = "Malformed line " + std::to_string(ln); continue; }

      Bar b{};
      // Date -> ts_ms
      b.ts_ms = parse_date_ms(cols[0]);
      if (b.ts_ms < 0){ warn = "Bad date at line " + std::to_string(ln); continue; }

      // Helpers to clean money/commas
      auto money = [](std::string s)->double{
        s = strip_chars(s, "$ \t");
        // some providers add '\r'
        if (!s.empty() && (s.back()=='\r' || s.back()=='\n')) s.pop_back();
        return std::stod(s);
      };
      auto number_commas = [](std::string s)->double{
        s = strip_chars(s, ", \t");
        if (!s.empty() && (s.back()=='\r' || s.back()=='\n')) s.pop_back();
        return std::stod(s);
      };

      try{
        double close = money(cols[1]);
        double vol   = number_commas(cols[2]);  // volume can be very big; store as double
        double open  = money(cols[3]);
        double high  = money(cols[4]);
        double low   = money(cols[5]);

        b.open = open; b.high = high; b.low = low; b.close = close; b.volume = vol;
      } catch(...){
        warn = "Numeric parse error at line " + std::to_string(ln); continue;
      }

      tmp.push_back(b);
    }

    if (tmp.empty()) return out;

    // Many vendor files are newest-first. Ensure ascending time.
    if (tmp.size() >= 2 && tmp.front().ts_ms > tmp.back().ts_ms){
      std::reverse(tmp.begin(), tmp.end());
    }
    out.swap(tmp);
  }

  return out;
}
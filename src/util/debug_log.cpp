#include "video/sdl/debug_log.hpp"

#include <cstdio>
#include <cstdarg>
#include <vector>
#include <string>

namespace {
  std::vector<std::string> g_log_ring;
  constexpr size_t MAX_LOG_LINES = 500;
}

void frame_log(const char* fmt, ...)
{
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  g_log_ring.emplace_back(buf);
  if (g_log_ring.size() > MAX_LOG_LINES)
    g_log_ring.erase(g_log_ring.begin());
}

void flush_log_to_disk()
{
  FILE* f = fopen("sdmc:/supertux_debug.log", "w");
  if (!f) return;
  for (auto& line : g_log_ring)
    fputs(line.c_str(), f);
  fclose(f);
}

//  SuperTux
//  Copyright (C) 2009 Ingo Ruhnke <grumbel@gmail.com>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <SDL.h>

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "supertux/main.hpp"

#include <switch.h>

static std::unique_ptr<Main> g_main;

int main(int argc, char** argv)
{
  fprintf(stderr, "[main] entry, argc=%d\n", argc);
  fflush(stderr);
  socketInitializeDefault();
  nxlinkStdio();
  fprintf(stderr, "[main] nxlink ready, constructing Main\n");
  fflush(stderr);
  g_main = std::make_unique<Main>();
  fprintf(stderr, "[main] Main constructed, calling run()\n");
  fflush(stderr);

  int ret = g_main->run(argc, argv);
  fprintf(stderr, "[main] run() returned %d\n", ret);
  fflush(stderr);

#if !defined(__EMSCRIPTEN__)
  // Manually destroy, as atexit() functions are called before global
  // destructors and thus would make the destruction crash.
  g_main.reset();
#endif

  socketExit();
  return ret;
}

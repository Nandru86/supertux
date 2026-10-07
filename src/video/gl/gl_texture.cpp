//  SuperTux
//  Copyright (C) 2006 Matthias Braun <matze@braunis.de>
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

#include "video/gl/gl_texture.hpp"

#include <algorithm>
#include <assert.h>
#include <cstdint>
#include <cstdio>

#include "supertux/gameconfig.hpp"
#include "supertux/globals.hpp"
#include "video/glutil.hpp"
#include "video/sampler.hpp"
#include "video/sdl_surface.hpp"

namespace {

// Cumulative bytes handed to glTexImage2D, plus a count of the uploads. Only
// used for the stderr diagnostics below.
uint64_t g_total_upload_bytes = 0;
uint64_t g_upload_count = 0;
uint64_t g_live_bytes = 0;
uint64_t g_live_count = 0;

// Returns true when every pixel of the surface is fully opaque.
bool surface_is_opaque(const SDL_Surface& surface)
{
  const int bpp = surface.format->BytesPerPixel;
  if (bpp != 4)
    return false;

  SDL_Surface* mutable_surface = const_cast<SDL_Surface*>(&surface);
  const bool must_lock = SDL_MUSTLOCK(mutable_surface) != 0;
  if (must_lock && SDL_LockSurface(mutable_surface) != 0)
    return false;

  bool opaque = true;
  const size_t stride = surface.pitch >= 0
    ? static_cast<size_t>(surface.pitch)
    : static_cast<size_t>(-static_cast<int>(surface.pitch));
  const auto* base = static_cast<const unsigned char*>(surface.pixels);

  for (int y = 0; y < surface.h && opaque; ++y)
  {
    const unsigned char* row = base + static_cast<size_t>(y) * stride;
    for (int x = 0; x < surface.w * bpp; x += bpp)
    {
      // The alpha byte is the last of the four, whatever the channel order.
      if (row[x + bpp - 1] != 255)
      {
        opaque = false;
        break;
      }
    }
  }

  if (must_lock)
    SDL_UnlockSurface(mutable_surface);

  return opaque;
}

} // namespace

GLTexture::GLTexture(int width, int height, std::optional<Color> fill_color) :
  m_handle(),
  m_texture_width(),
  m_texture_height(),
  m_image_width(),
  m_image_height()
{
#if defined(GL_VERSION_ES_CM_1_0) && !defined(HAVE_EPOXY)
  assert(is_power_of_2(width));
  assert(is_power_of_2(height));
#endif
  m_texture_width  = width;
  m_texture_height = height;
  m_image_width  = width;
  m_image_height = height;

  assert_gl();

  glGenTextures(1, &m_handle);

  try {
    glBindTexture(GL_TEXTURE_2D, m_handle);

    if (fill_color)
    {
      std::vector<uint32_t> pixels(m_texture_width * m_texture_height, fill_color->rgba());
      glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_RGBA), m_texture_width,
                   m_texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    }
    else
    {
      glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_RGBA), m_texture_width,
                   m_texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    set_texture_params();
  } catch(...) {
    glDeleteTextures(1, &m_handle);
    throw;
  }

  assert_gl();
}

GLTexture::GLTexture(const SDL_Surface& image, const Sampler& sampler) :
  Texture(sampler),
  m_handle(),
  m_texture_width(),
  m_texture_height(),
  m_image_width(),
  m_image_height()
{
  reload(image);
}

void
GLTexture::reload(const SDL_Surface& image)
{
  assert_gl();

  glDeleteTextures(1, &m_handle);

  // A texture larger than the window can never be shown at its native size, and
  // on the Switch every byte counts: the Gallium/nouveau driver only grants
  // ~64 MiB of texture memory, which a single SuperTux level already exceeds.
  // Scale oversized images down to the window before uploading rather than
  // paying full resolution for detail that can never be on screen.
  const Size& window_size = g_config->window_size;
  SDLSurfacePtr downscaled;
  const SDL_Surface* source = &image;
  if (window_size.width > 0 && window_size.height > 0 &&
      (image.w > window_size.width || image.h > window_size.height))
  {
    const float scale_x = static_cast<float>(window_size.width) / static_cast<float>(image.w);
    const float scale_y = static_cast<float>(window_size.height) / static_cast<float>(image.h);
    const float scale = std::min(scale_x, scale_y);
    if (scale < 1.0f)
    {
      const int width  = std::max(1, static_cast<int>(image.w * scale));
      const int height = std::max(1, static_cast<int>(image.h * scale));
      downscaled = SDLSurface::create_rgba(width, height);
      SDL_SetSurfaceBlendMode(const_cast<SDL_Surface*>(&image), SDL_BLENDMODE_NONE);
      SDL_BlitScaled(const_cast<SDL_Surface*>(&image), nullptr, downscaled.get(), nullptr);
      source = downscaled.get();
    }
  }

  if (gl_needs_power_of_two())
  {
    m_texture_width = next_power_of_two(source->w);
    m_texture_height = next_power_of_two(source->h);
  }
  else
  {
    m_texture_width  = source->w;
    m_texture_height = source->h;
  }

  m_image_width  = source->w;
  m_image_height = source->h;

  SDLSurfacePtr convert = SDLSurface::create_rgba(m_texture_width, m_texture_height);

  SDL_SetSurfaceBlendMode(const_cast<SDL_Surface*>(&image), SDL_BLENDMODE_NONE);
  SDL_BlitSurface(const_cast<SDL_Surface*>(source), nullptr, convert.get(), nullptr);

  // Fill the remaining pixels of 'convert' with repeated copies of
  // 'image' to minimize OpenGL blending artifacts at the borders.
  if (m_image_width != m_texture_width || m_image_height != m_texture_height)
  {
    if (SDL_MUSTLOCK(convert)) {
      SDL_LockSurface(convert.get());
    }

    if (m_image_width != m_texture_width) {
      SDL_Rect srcrect{m_image_width - 1, 0, 1, m_image_height};
      for (int x = m_image_width; x < m_texture_width; ++x) {
        SDL_Rect dstrect{x, 0, 1, m_image_height};
        SDL_BlitSurface(const_cast<SDL_Surface*>(source), &srcrect, convert.get(), &dstrect);
      }
    }

    if (m_image_height != m_texture_height) {
      SDL_Rect srcrect{0, m_image_height - 1, m_image_width, 1};
      for (int y = m_image_height; y < m_texture_height; ++y) {
        SDL_Rect dstrect{0, y, m_image_width, 1};
        SDL_BlitSurface(const_cast<SDL_Surface*>(source), &srcrect, convert.get(), &dstrect);
      }
    }

    if (m_image_width != m_texture_width && m_image_height != m_texture_height)
    {
      const int bpp = convert->format->BytesPerPixel;
      const int x = m_image_width - 1;
      const int y = m_image_height - 1;
      Uint32 color = 0;
      memcpy(&color, static_cast<uint8_t*>(convert->pixels) + y * convert->pitch + x * bpp, bpp);
      SDL_Rect dstrect{m_image_width, m_image_height, m_texture_width, m_texture_height};
      SDL_FillRect(convert.get(), &dstrect, color);
    }

    if (SDL_MUSTLOCK(convert)) {
      SDL_UnlockSurface(convert.get());
    }
  }

  assert_gl();

  // Measured after the border fill above, so the whole texture is opaque.
  const bool drop_alpha = surface_is_opaque(*convert);

  glGenTextures(1, &m_handle);

  try {
    GLenum sdl_format;
    // Match the internal format to what the pixels actually need. Most SuperTux
    // tiles are fully opaque, and dropping their alpha channel saves 25% of the
    // texture memory -- which is what brings a level load back under the ~64 MiB
    // ceiling the Gallium/nouveau driver enforces on the Switch. Forcing GL_RGBA
    // on an RGB image would likewise waste a third of the memory.
    GLint internal_format;
    const int bytes_per_pixel = (convert->format->BytesPerPixel == 3)
      ? 3
      : (drop_alpha ? 3 : 4);
    if (bytes_per_pixel == 3) {
      sdl_format = GL_RGB;
      internal_format = GL_RGB8;
    } else {
      sdl_format = GL_RGBA;
      internal_format = GL_RGBA8;
    }

    glBindTexture(GL_TEXTURE_2D, m_handle);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#if defined(GL_UNPACK_ROW_LENGTH)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, convert->pitch/convert->format->BytesPerPixel);
#else
    /* OpenGL ES doesn't support UNPACK_ROW_LENGTH, let's hope SDL didn't add
     * padding bytes, otherwise we need some extra code here... */
    assert(convert->pitch == static_cast<int>(m_texture_width * convert->format->BytesPerPixel));
#endif

    if (SDL_MUSTLOCK(convert)) {
      SDL_LockSurface(convert.get());
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internal_format,
                 m_texture_width, m_texture_height, 0, sdl_format,
                 GL_UNSIGNED_BYTE, convert->pixels);

    g_total_upload_bytes +=
      static_cast<uint64_t>(m_texture_width) * static_cast<uint64_t>(m_texture_height)
      * static_cast<uint64_t>(bytes_per_pixel);
    g_upload_count++;
    m_live_bytes =
      static_cast<uint64_t>(m_texture_width) * static_cast<uint64_t>(m_texture_height)
      * static_cast<uint64_t>(bytes_per_pixel);
    g_live_bytes += m_live_bytes;
    g_live_count++;

    // Disable the use of mipmaps for the texture.
#if 0
    glGenerateMipmap(GL_TEXTURE_2D);
#endif

    if (SDL_MUSTLOCK(convert.get())) {
      SDL_UnlockSurface(convert.get());
    }

    assert_gl();

    set_texture_params();

    if (g_upload_count % 1000 == 0)
    {
      fprintf(stderr, "[tex] %llu uploads  live=%.1f MB/%llu  cumulative=%.1f MB\n",
              static_cast<unsigned long long>(g_upload_count),
              static_cast<double>(g_live_bytes) / 1048576.0,
              static_cast<unsigned long long>(g_live_count),
              static_cast<double>(g_total_upload_bytes) / 1048576.0);
      fflush(stderr);
    }
  } catch(...) {
    glDeleteTextures(1, &m_handle);
    throw;
  }

  assert_gl();
}

GLTexture::~GLTexture()
{
  glDeleteTextures(1, &m_handle);

  if (m_live_bytes)
  {
    g_live_bytes -= m_live_bytes;
    g_live_count--;
  }
}

void
GLTexture::set_texture_params()
{
  assert_gl();

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(m_sampler.get_filter()));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(m_sampler.get_filter()));

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(m_sampler.get_wrap_s()));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(m_sampler.get_wrap_t()));

  assert_gl();
}

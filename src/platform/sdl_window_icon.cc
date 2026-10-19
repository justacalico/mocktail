#include "mocktail/platform/sdl_window_icon.h"

#include <SDL3/SDL.h>

#if !SDL_VERSION_ATLEAST(3, 4, 0)
#include <png.h>

#include <cstring>
#include <vector>
#endif

#include <string>
#include <utility>

#include "mocktail/platform/sdl_window_icon_data.h"

namespace mocktail {
namespace platform {
namespace {

Status IconError(const char* operation) {
  std::string message = operation;
  message += " failed: ";
  const char* error = SDL_GetError();
  message += error != nullptr && error[0] != '\0' ? error : "unknown SDL error";
  return Status::Error(StatusCode::kPlatformError, std::move(message));
}

#if !SDL_VERSION_ATLEAST(3, 4, 0)
struct IconPngReader {
  png_size_t offset = 0;
};

void IconPngRead(png_structp png, png_bytep output, png_size_t length) {
  auto* reader = static_cast<IconPngReader*>(png_get_io_ptr(png));
  if (reader == nullptr ||
      length > internal::kMocktailWindowIconPngSize - reader->offset) {
    png_error(png, "icon png read past end of buffer");
  }
  std::memcpy(output, internal::kMocktailWindowIconPng + reader->offset,
              length);
  reader->offset += length;
}

SDL_Surface* LoadIconSurfaceFromPng() {
  png_structp png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (png == nullptr) {
    return nullptr;
  }
  png_infop info = png_create_info_struct(png);
  if (info == nullptr) {
    png_destroy_read_struct(&png, nullptr, nullptr);
    return nullptr;
  }
  SDL_Surface* icon = nullptr;
  if (setjmp(png_jmpbuf(png)) == 0) {
    IconPngReader reader;
    png_set_read_fn(png, &reader, &IconPngRead);
    png_read_info(png, info);
    png_uint_32 width = 0;
    png_uint_32 height = 0;
    int bit_depth = 0;
    int color_type = 0;
    png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, nullptr,
                 nullptr, nullptr);
    if (bit_depth == 16) {
      png_set_strip_16(png);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
      png_set_palette_to_rgb(png);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
      png_set_expand_gray_1_2_4_to_8(png);
    }
    if (png_get_valid(png, info, PNG_INFO_tRNS) != 0) {
      png_set_tRNS_to_alpha(png);
    } else if ((color_type & PNG_COLOR_MASK_ALPHA) == 0) {
      png_set_filler(png, 0xff, PNG_FILLER_AFTER);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
      png_set_gray_to_rgb(png);
    }
    png_read_update_info(png, info);
    icon = SDL_CreateSurface(static_cast<int>(width), static_cast<int>(height),
                             SDL_PIXELFORMAT_RGBA32);
    if (icon != nullptr) {
      std::vector<png_bytep> rows(height);
      auto* pixels = static_cast<unsigned char*>(icon->pixels);
      for (png_uint_32 y = 0; y < height; ++y) {
        rows[y] = pixels + y * static_cast<std::size_t>(icon->pitch);
      }
      png_read_image(png, rows.data());
      png_read_end(png, nullptr);
    }
  }
  png_destroy_read_struct(&png, &info, nullptr);
  return icon;
}
#endif

}  // namespace

Status ApplySdlWindowIcon(SDL_Window* window) {
  if (window == nullptr) {
    return Status::Error(StatusCode::kInvalidArgument,
                         "SDL window must not be null");
  }

  SDL_Surface* icon = nullptr;
#if SDL_VERSION_ATLEAST(3, 4, 0)
  SDL_IOStream* stream = SDL_IOFromConstMem(
      internal::kMocktailWindowIconPng,
      internal::kMocktailWindowIconPngSize);
  if (stream == nullptr) {
    return IconError("SDL_IOFromConstMem(window icon)");
  }

  icon = SDL_LoadPNG_IO(stream, true);
  if (icon == nullptr) {
    return IconError("SDL_LoadPNG_IO(window icon)");
  }
#else
  icon = LoadIconSurfaceFromPng();
  if (icon == nullptr) {
    return IconError("window icon png decode");
  }
#endif

  if (!SDL_SetWindowIcon(window, icon)) {
    Status status = IconError("SDL_SetWindowIcon");
    SDL_DestroySurface(icon);
    return status;
  }
  SDL_DestroySurface(icon);
  return Status::Ok();
}

}  // namespace platform
}  // namespace mocktail

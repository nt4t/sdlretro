#include "sdl1_video.h"

#include "sdl1_ttf.h"

#include "cfg.h"

#include "helper.h"

#include "logger.h"

#include <SDL.h>

#include <cstdio>

namespace drivers {

const int sdl_video_flags = SDL_SWSURFACE |
#ifdef SDL_TRIPLEBUF
    SDL_TRIPLEBUF
#else
    SDL_DOUBLEBUF
#endif
    ;

sdl1_video::sdl1_video() {
    SDL_ShowCursor(SDL_DISABLE);
    g_cfg.get_resolution(curr_width, curr_height);
    curr_pixel_format = 2;
    screen = SDL_SetVideoMode(curr_width, curr_height, 16, sdl_video_flags);
    SDL_LockSurface(screen);
    screen_ptr = screen->pixels;

    ttf[0] = std::make_shared<sdl1_ttf>();
    ttf[0]->init(16, 0);
    ttf[0]->add(g_cfg.get_data_dir() + PATH_SEPARATOR_CHAR + "fonts" + PATH_SEPARATOR_CHAR + "regular.ttf", 0);
    LOG(INFO, "SDL1 font[0] loaded: {}", ttf[0]->is_valid() ? "yes" : "no");
    ttf[1] = std::make_shared<sdl1_ttf>();
    ttf[1]->init(16, 0);
    ttf[1]->add(g_cfg.get_data_dir() + PATH_SEPARATOR_CHAR + "fonts" + PATH_SEPARATOR_CHAR + "bold.ttf", 0);
    LOG(INFO, "SDL1 font[1] loaded: {}", ttf[1]->is_valid() ? "yes" : "no");

    for (int i = 0; i < 128; i++) {
        glyph_cache[i].pixels = nullptr;
        glyph_cache[i].width = 0;
        glyph_cache[i].height = 0;
        glyph_cache[i].valid = false;
    }
    glyph_cache_initialized = false;
}

sdl1_video::~sdl1_video() {
    deinit_glyph_cache();
    SDL_UnlockSurface(screen);
}

  void sdl1_video::window_resized(int width, int height, bool fullscreen) {
    if (fullscreen) {
        SDL_UnlockSurface(screen);
        int flags = sdl_video_flags | SDL_FULLSCREEN;
        screen = SDL_SetVideoMode(0, 0, 16, flags);
        SDL_LockSurface(screen);
        screen_ptr = screen->pixels;
        curr_width = screen->w;
        curr_height = screen->h;
    } else {
        SDL_UnlockSurface(screen);
        screen = SDL_SetVideoMode(width, height, 16, sdl_video_flags);
        SDL_LockSurface(screen);
        screen_ptr = screen->pixels;
        curr_width = width;
        curr_height = height;
    }
}

bool sdl1_video::game_resolution_changed(int width, int height, int max_width, int max_height, unsigned pixel_format) {
    if (g_cfg.get_scaling_mode() == 0) {
        SDL_UnlockSurface(screen);
        curr_pixel_format = pixel_format;
        unsigned bpp = pixel_format == 1 ? 32 : 16;
        bool was_fullscreen = (screen->flags & SDL_FULLSCREEN) != 0;
        if (width != 0 && height != 0) {
            curr_width = (int)width;
            curr_height = (int)height;
            int scale = force_scale == 0 ? g_cfg.get_scale() : force_scale;
            if (g_cfg.get_integer_scaling() && force_scale == 0) {
                int screen_w = 0, screen_h = 0;
                if (was_fullscreen) {
                    SDL_Surface *fs = SDL_SetVideoMode(0, 0, 16, sdl_video_flags | SDL_FULLSCREEN);
                    if (fs) {
                        screen_w = fs->w;
                        screen_h = fs->h;
                    }
                } else {
                    screen_w = screen->w;
                    screen_h = screen->h;
                }
                if (screen_w > 0 && screen_h > 0) {
                    int scale_x = screen_w / width;
                    int scale_y = screen_h / height;
                    int max_int_scale = (scale_x < scale_y) ? scale_x : scale_y;
                    if (max_int_scale < 1) max_int_scale = 1;
                    if (scale > max_int_scale) scale = max_int_scale;
                }
            }
            current_scale = scale;
            int flags = sdl_video_flags;
            if (was_fullscreen) flags |= SDL_FULLSCREEN;
            screen = SDL_SetVideoMode(width * scale, height * scale, bpp, flags);
        } else {
            g_cfg.get_resolution(curr_width, curr_height);
            int flags = sdl_video_flags;
            if (was_fullscreen) flags |= SDL_FULLSCREEN;
            screen = SDL_SetVideoMode(curr_width, curr_height, bpp, flags);
        }
        SDL_LockSurface(screen);
        screen_ptr = screen->pixels;
    } else {
        int scale = force_scale == 0 ? g_cfg.get_scale() : force_scale;
        if (g_cfg.get_integer_scaling() && width > 0 && height > 0) {
            int screen_w = screen->w;
            int screen_h = screen->h;
            if (screen_w > 0 && screen_h > 0) {
                int scale_x = screen_w / width;
                int scale_y = screen_h / height;
                int int_scale = (scale_x < scale_y) ? scale_x : scale_y;
                if (int_scale < 1) int_scale = 1;
                if (scale > int_scale) scale = int_scale;
            }
        }
        current_scale = scale;
        curr_width = (int)width;
        curr_height = (int)height;
        curr_pixel_format = pixel_format;
    }
    return true;
}

  void sdl1_video::render(const void *data, int width, int height, size_t pitch) {
    if (!data) {
        drawn = false;
        return;
    }
    if (skip_frame) {
        drawn = false;
        skip_frame = false;
        return;
    }
    drawn = true;

    if (fps_enabled) {
        frame_count++;
        double now = SDL_GetTicks() / 1000.0;
        if (now - last_fps_update >= 1.0) {
            current_fps = frame_count;
            frame_count = 0;
            last_fps_update = now;
        }
    }

    if (curr_width != width || curr_height != height) {
        game_resolution_changed(width, height, 0, 0, curr_pixel_format);
    }
    int h = static_cast<int>(height);
    int scale = current_scale;
    unsigned input_bpp = (pitch > 0 && width > 0) ? (pitch / width) * 8 : 16;
    unsigned output_bpp = screen->format->BitsPerPixel;
    
    int offset_x = 0;
    int offset_y = 0;
    if (g_cfg.get_scaling_mode() == 1 && scale > 0) {
        int scaled_w = width * scale;
        int scaled_h = height * scale;
        offset_x = (screen->w - scaled_w) / 2;
        offset_y = (screen->h - scaled_h) / 2;
        if (offset_x < 0) offset_x = 0;
        if (offset_y < 0) offset_y = 0;
        
        uint32_t black = 0;
        if (output_bpp == 32) {
            black = 0xFF000000;
        } else {
            black = 0x0000;
        }
        auto *clear_ptr = (uint32_t*)screen_ptr;
        size_t clear_count = (size_t)screen->w * screen->h;
        for (size_t i = 0; i < clear_count; i++) {
            clear_ptr[i] = black;
        }
    }
    
    if (scale == 1) {
        auto *pixels = static_cast<uint8_t *>(screen_ptr);
        const auto *input = static_cast<const uint8_t *>(data);
        int output_pitch = screen->pitch;
        if (offset_x > 0 || offset_y > 0) {
            int line_bytes = width*(input_bpp >> 3);
            int dest_pitch = output_pitch + offset_x * (output_bpp >> 3);
            uint8_t *dest = pixels + offset_y * output_pitch + offset_x * (output_bpp >> 3);
            for (; h; h--) {
                memcpy(dest, input, line_bytes);
                dest += dest_pitch;
                input += pitch;
            }
        } else if (output_pitch == pitch) {
            memcpy(pixels, input, h * pitch);
        } else {
            int line_bytes = width*(input_bpp >> 3);
            for (; h; h--) {
                memcpy(pixels, input, line_bytes);
                pixels += output_pitch;
                input += pitch;
            }
        }
    } else {
    #define CODE_WITH_TYPE(TYPE) \
        int output_pitch = screen->pitch / sizeof(TYPE); \
        const TYPE *input_data = static_cast<const TYPE*>(data); \
        TYPE *pixels = static_cast<TYPE*>(screen_ptr); \
        int src_pitch = pitch / sizeof(TYPE); \
        int scaled_width = width * scale; \
        int scaled_height = height * scale; \
        TYPE *h_line = (TYPE*)malloc(scaled_width * sizeof(TYPE)); \
        if (h_line) { \
            TYPE *dest = pixels + offset_y * output_pitch + offset_x; \
            int dest_pitch = output_pitch; \
            for (int y = 0; y < height; y++) { \
                int x = 0; \
                for (int i = 0; i < width; i++) { \
                    TYPE pix = input_data[i]; \
                    for (int j = 0; j < scale; j++) { \
                        h_line[x++] = pix; \
                    } \
                } \
                input_data += src_pitch; \
                for (int v = 0; v < scale; v++) { \
                    memcpy(dest, h_line, scaled_width * sizeof(TYPE)); \
                    dest += dest_pitch; \
                } \
            } \
            free(h_line); \
        }
        if (output_bpp == 32) {
            CODE_WITH_TYPE(uint32_t)
        } else {
            CODE_WITH_TYPE(uint16_t)
        }
    #undef CODE_WITH_TYPE
    }
    if (!messages.empty()) {
        uint32_t lh = get_font_size() + 2;
        uint32_t y = (curr_height - 5 - (messages.size() - 1) * lh) * scale;
        for (auto &m: messages) {
            draw_text_pixel(5, y, m.first.c_str(), 0, true);
            y += lh;
        }
    }
    if (fps_enabled && current_fps > 0) {
        char fps_str[32];
        snprintf(fps_str, sizeof(fps_str), "FPS: %d", current_fps);
        draw_text_pixel(5, 30, fps_str, 0, true);
    }
}

void sdl1_video::frame_render() {
    if (drawn) {
        flip();
    }
}

void *sdl1_video::get_framebuffer(unsigned *width, unsigned *height, size_t *pitch, int *format) {
    if (!screen) return nullptr;
    *width = screen->w;
    *height = screen->h;
    *pitch = screen->pitch;
    *format = 2;
    return screen_ptr;
}

void sdl1_video::clear() {
    memset(screen_ptr, 0, screen->pitch * screen->h);
}

void sdl1_video::flip() {
    SDL_UnlockSurface(screen);
    SDL_UpdateRect(screen, 0, 0, screen->w, screen->h);
    SDL_LockSurface(screen);
    screen_ptr = screen->pixels;
}

int sdl1_video::get_font_size() const {
    if (ttf[0]) {
        return ttf[0]->get_font_size();
    } else {
#ifdef GCW_ZERO
        return 8;
#else
        return 16;
#endif
    }
}

void sdl1_video::set_draw_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    draw_color[0] = r;
    draw_color[1] = g;
    draw_color[2] = b;
    draw_color[3] = a;
}

void sdl1_video::draw_rectangle(int x, int y, int w, int h) {
    auto bytespp = screen->format->BytesPerPixel;
    uint32_t pixel_color = SDL_MapRGB(screen->format, draw_color[0], draw_color[1], draw_color[2]);
    uint8_t *ptr = (uint8_t*)screen_ptr + screen->pitch * y + x * bytespp;
    int rx = x + w;
    int by = y + h;
    for (int cx = x; cx < rx; ++cx) {
        memcpy(ptr, &pixel_color, bytespp);
        ptr += bytespp;
    }
    ptr = (uint8_t*)screen_ptr + screen->pitch * y + x * bytespp;
    for (int cy = y + 1; cy <= by; ++cy) {
        memcpy(ptr, &pixel_color, bytespp);
        ptr += screen->pitch;
    }
    ptr = (uint8_t*)screen_ptr + screen->pitch * by + x * bytespp;
    for (int cx = x; cx < rx; ++cx) {
        memcpy(ptr, &pixel_color, bytespp);
        ptr += bytespp;
    }
    ptr = (uint8_t*)screen_ptr + screen->pitch * y + rx * bytespp;
    for (int cy = y; cy <= by; ++cy) {
        memcpy(ptr, &pixel_color, bytespp);
        ptr += screen->pitch;
    }
}

void sdl1_video::fill_rectangle(int x, int y, int w, int h) {
    auto bytespp = screen->format->BytesPerPixel;
    uint32_t pixel_color = SDL_MapRGB(screen->format, draw_color[0], draw_color[1], draw_color[2]);
    uint8_t *ptr = (uint8_t*)screen_ptr + screen->pitch * y + x * bytespp;
    int rx = x + w;
    int by = y + h;
    size_t lp = screen->pitch - bytespp * w;
    for (int cy = y; cy < by; ++cy) {
        for (int cx = x; cx < rx; ++cx) {
            memcpy(ptr, &pixel_color, bytespp);
            ptr += bytespp;
        }
        ptr += lp;
    }
}

void sdl1_video::draw_text(int x, int y, const char *text, int width, bool shadow) {
    draw_text_pixel(x, y, text, width, shadow);
}

#include "bmfont.inl"

inline const font_data_t &get_pixel_font_data(uint8_t c) {
#ifdef GCW_ZERO
    return font_small_data[c];
#else
    return font_big_data[c];
#endif
}

void sdl1_video::init_glyph_cache() {
    if (glyph_cache_initialized) return;
    
    for (int c = 0; c < 128; c++) {
        const auto &fd = get_pixel_font_data(c);
        if (fd.w == 0 || fd.h == 0) continue;
        
        glyph_cache[c].width = fd.w;
        glyph_cache[c].height = fd.h;
        glyph_cache[c].x_offset = fd.x;
        glyph_cache[c].y_offset = fd.y;
        glyph_cache[c].valid = true;
        
        size_t pixel_size = sizeof(uint32_t);
        glyph_cache[c].pixels = new uint8_t[fd.w * fd.h * pixel_size];
        
        uint8_t *dest = glyph_cache[c].pixels;
        const uint8_t *fontdata = fd.data;
        uint32_t step = (fd.w + 7) >> 3;
        
        for (int y = 0; y < fd.h; y++) {
            uint8_t bitflag = 0x01;
            uint32_t fdidx = 0;
            for (int x = 0; x < fd.w; x++) {
                uint32_t pixel = 0;
                if (fontdata[fdidx] & bitflag) {
                    pixel = 0xFFFFFFFF;
                }
                memcpy(dest, &pixel, pixel_size);
                dest += pixel_size;
                if (bitflag == 0x80) {
                    fdidx++;
                    bitflag = 1;
                } else {
                    bitflag <<= 1;
                }
            }
            fontdata += step;
        }
    }
    glyph_cache_initialized = true;
}

void sdl1_video::deinit_glyph_cache() {
    if (!glyph_cache_initialized) return;
    
    for (int c = 0; c < 128; c++) {
        if (glyph_cache[c].pixels) {
            delete[] glyph_cache[c].pixels;
            glyph_cache[c].pixels = nullptr;
        }
        glyph_cache[c].valid = false;
    }
    glyph_cache_initialized = false;
}

void sdl1_video::render_glyph_pixel(uint8_t c, int x, int y, int swidth, bool shadow) {
    if (!glyph_cache[c].valid) {
        const auto &fd = get_pixel_font_data(c);
        unsigned bpp = curr_pixel_format == 1 ? 32 : 16;
    #define CODE_WITH_TYPE(TYPE) \
        auto *ptr = (TYPE*)screen_ptr + x + fd.x + (y + fd.y) * swidth; \
        auto *fontdata = fd.data; \
        uint32_t wrapx = swidth - fd.w; \
        uint32_t step = (fd.w + 7) >> 3; \
        if (shadow) { \
            for (int h = fd.h; h; h--) { \
                uint8_t bitflag = 0x01; \
                uint32_t fdidx = 0; \
                for (int w = fd.w; w; w--) { \
                    if (fontdata[fdidx] & bitflag) { \
                        *ptr++ = (TYPE)-1; \
                        *(ptr + swidth) = 0; \
                    } else ++ptr; \
                    if (bitflag == 0x80) { \
                        fdidx++; \
                        bitflag = 1; \
                    } else { \
                        bitflag <<= 1; \
                    } \
                } \
                ptr += wrapx; \
                fontdata += step; \
            } \
        } else { \
            for (int h = fd.h; h; h--) { \
                uint8_t bitflag = 0x01; \
                uint32_t fdidx = 0; \
                for (int w = fd.w; w; w--) { \
                    if (fontdata[fdidx] & bitflag) \
                        *ptr++ = (TYPE)-1; \
                    else ++ptr; \
                    if (bitflag == 0x80) { \
                        fdidx++; \
                        bitflag = 1; \
                    } else { \
                        bitflag <<= 1; \
                    } \
                } \
                ptr += wrapx; \
                fontdata += step; \
            } \
        }
        if (bpp == 32) {
            CODE_WITH_TYPE(uint32_t)
        } else {
            CODE_WITH_TYPE(uint16_t)
        }
    #undef CODE_WITH_TYPE
        return;
    }
    
    unsigned bpp = curr_pixel_format == 1 ? 32 : 16;
    int gw = glyph_cache[c].width;
    int gh = glyph_cache[c].height;
    int gx = glyph_cache[c].x_offset;
    int gy = glyph_cache[c].y_offset;
    
    if (bpp == 32) {
        auto *ptr = (uint32_t*)screen_ptr + x + gx + (y + gy) * swidth;
        uint32_t *src = (uint32_t*)glyph_cache[c].pixels;
        uint32_t wrapx = swidth - gw;
        for (int h = 0; h < gh; h++) {
            for (int w = 0; w < gw; w++) {
                if (src[w]) {
                    ptr[w] = 0xFFFFFFFF;
                }
            }
            ptr += swidth;
            src += gw;
        }
    } else {
        auto *ptr = (uint16_t*)screen_ptr + x + gx + (y + gy) * swidth;
        uint32_t *src = (uint32_t*)glyph_cache[c].pixels;
        uint32_t wrapx = swidth - gw;
        for (int h = 0; h < gh; h++) {
            for (int w = 0; w < gw; w++) {
                if (src[w]) {
                    ptr[w] = 0xFFFF;
                }
            }
            ptr += swidth;
            src += gw;
        }
    }
}

void sdl1_video::get_text_width_and_height(const char *text, int &w, int &t, int &b) const {
    w = 0;
    t = 255;
    b = -255;
    while (*text) {
        uint8_t c = *text++;
        if (c > 0x7F) continue;
        const auto &fd = get_pixel_font_data(c);
        w += fd.sw;
        if (fd.y < t) t = fd.y;
        if (fd.y + fd.h > b) b = fd.y + fd.h;
    }
}

void sdl1_video::draw_text_pixel(int x, int y, const char *text, int width, bool shadow) {
    bool allow_wrap = false;
    int nwidth;
    int ox = x;
    unsigned bpp = curr_pixel_format == 1 ? 32 : 16;
    if (width == 0) {
        nwidth = width = screen->w - x;
    } else if (width == -1) {
        nwidth = width = screen->w - x;
        allow_wrap = true;
    } else {
        if (width < 0) {
            allow_wrap = true;
            width = -width;
            nwidth = width;
        } else {
            nwidth = width;
        }
    }
    int swidth = screen->pitch / screen->format->BytesPerPixel;
    
    if (!glyph_cache_initialized) {
        init_glyph_cache();
    }
    
    while (*text) {
        uint8_t c = *text++;
        if (c > 0x7F) continue;
        const auto &fd = get_pixel_font_data(c);
        if (fd.sw > nwidth) {
            if (!allow_wrap) break;
            x = ox;
            nwidth = width;
#ifdef GCW_ZERO
            y += 8 + 1;
#else
            y += 16 + 1;
#endif
        }
        nwidth -= fd.sw;
        render_glyph_pixel(c, x, y, swidth, shadow);
        x += fd.sw;
    }
}

void sdl1_video::gui_popup() {
    SDL_UnlockSurface(screen);
    saved_width = curr_width;
    saved_height = curr_height;
    saved_pixel_format = curr_pixel_format;
    g_cfg.get_resolution(curr_width, curr_height);
    curr_pixel_format = 2;
    screen = SDL_SetVideoMode(curr_width, curr_height, curr_pixel_format == 1 ? 32 : 16, sdl_video_flags);
    SDL_LockSurface(screen);
    screen_ptr = screen->pixels;
}

void sdl1_video::gui_leave() {
    game_resolution_changed(saved_width, saved_height, 0, 0, saved_pixel_format);
}

}

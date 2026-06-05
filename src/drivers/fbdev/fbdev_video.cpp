#include "fbdev_video.h"

#include <cfg.h>
#include "bmfont.inl"
#include <logger.h>

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <ctime>

namespace drivers {

inline const font_data_t &get_pixel_font_data(uint8_t c) {
    return font_big_data[c];
}

fbdev_video::fbdev_video(int fb_fd, void *fb_ptr, size_t fb_size, struct fb_var_screeninfo vinfo, struct fb_fix_screeninfo finfo)
    : fb_fd(fb_fd), fb_ptr(fb_ptr), fb_size(fb_size),
      fb_width(vinfo.xres), fb_height(vinfo.yres),
      fb_bpp(vinfo.bits_per_pixel), fb_pitch(finfo.line_length) {
    g_cfg.get_resolution(output_width, output_height);
    if (output_width == 0) output_width = fb_width;
    if (output_height == 0) output_height = fb_height;
    
    game_pixel_format = 2;
    drawn = false;
    frame_count = 0;
    last_fps_time = 0;
    
    memset(fb_ptr, 0, fb_size);
    LOG(INFO, "fbdev_video: {}x{}, {}bpp, pitch={}, line_length={}", fb_width, fb_height, fb_bpp, fb_pitch, finfo.line_length);
    LOG(INFO, "fb_pixel_fmt: r={}/{} g={}/{} b={}/{}", vinfo.red.offset, vinfo.red.length, vinfo.green.offset, vinfo.green.length, vinfo.blue.offset, vinfo.blue.length);
}

fbdev_video::~fbdev_video() {
    delete[] h_line_16;
    delete[] h_line_32;
    delete[] static_cast<uint8_t*>(game_frame_buffer);
}

void fbdev_video::window_resized(int width, int height, bool fullscreen) {
    output_width = width;
    output_height = height;
    
    if (fullscreen) {
        g_cfg.set_fullscreen(true);
    }
}

bool fbdev_video::game_resolution_changed(int width, int height, int max_width, int max_height, uint32_t pixel_format) {
    game_width = width;
    game_height = height;
    game_max_width = max_width;
    game_max_height = max_height;
    game_pixel_format = pixel_format;
    
    unsigned int scale_factor = g_cfg.get_scale();
    scale = static_cast<int>(scale_factor);
    scaling_mode = g_cfg.get_scaling_mode();
    
    if (scaling_mode == 0 && scale > 1) {
        output_width = width * scale;
        output_height = height * scale;
    } else {
        output_width = width;
        output_height = height;
    }
    
     size_t max_line_pixels = static_cast<size_t>(width) * scale;
    if (max_line_pixels > h_line_size) {
        delete[] h_line_16;
        delete[] h_line_32;
        h_line_16 = new uint16_t[max_line_pixels];
        h_line_32 = new uint32_t[max_line_pixels];
        h_line_size = max_line_pixels;
    }
    
    size_t new_frame_size = fb_width * fb_height * (fb_bpp == 32 ? 4 : 2);
    if (new_frame_size != game_frame_size) {
        delete[] static_cast<uint8_t*>(game_frame_buffer);
        game_frame_buffer = new uint8_t[new_frame_size];
        game_frame_size = new_frame_size;
    }
    
    if (!pixel_format_logged) {
        LOG(INFO, "Core pixel format: {} (RGB1555=0, XRGB8888=1, RGB565=2), fb_bpp={}, pitch={}", pixel_format, fb_bpp, fb_pitch);
        pixel_format_logged = true;
    }
    LOG(INFO, "fbdev_video: game {}x{}, max {}x{}, fmt={}, scale={}, output {}x{}",
        width, height, max_width, max_height, pixel_format, scale, output_width, output_height);
    
    return true;
}

void fbdev_video::render(const void *data, int width, int height, size_t pitch) {
    if (!data || game_width == 0) {
        drawn = false;
        return;
    }
    
    if (skip_frame) {
        skip_frame = false;
        drawn = false;
        return;
    }
    
   drawn = true;
    frame_count++;
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    if (last_fps_time == 0) {
        last_fps_time = now;
    }
    
     if (scaling_mode == 0 && scale > 1) {
        render_scaled(data, width, height, pitch);
    } else {
        render_1to1(data, width, height, pitch);
    }
    
    if (game_frame_buffer && game_frame_size > 0) {
        size_t fb_pitch_pixels = fb_pitch / (fb_bpp / 8);
        if (fb_bpp == 32) {
            uint32_t *src = static_cast<uint32_t*>(fb_ptr);
            uint32_t *dst = static_cast<uint32_t*>(game_frame_buffer);
            for (int y = 0; y < fb_height; y++) {
                memcpy(dst + y * fb_width, src + y * fb_pitch_pixels, fb_width * sizeof(uint32_t));
            }
        } else {
            uint16_t *src = static_cast<uint16_t*>(fb_ptr);
            uint16_t *dst = static_cast<uint16_t*>(game_frame_buffer);
            for (int y = 0; y < fb_height; y++) {
                memcpy(dst + y * fb_width, src + y * fb_pitch_pixels, fb_width * sizeof(uint16_t));
            }
        }
    }
    
    if (fps_enabled) {
        uint64_t elapsed = now - last_fps_time;
        if (elapsed >= 1000000000ULL) {
            current_fps = static_cast<float>(frame_count) * 1000000000.0f / static_cast<float>(elapsed);
            frame_count = 0;
            last_fps_time = now;
            
            uint64_t elapsed_console = now - last_console_fps_time;
            if (elapsed_console >= 10000000000ULL) {
                LOG(INFO, "FPS: {}", current_fps);
                last_console_fps_time = now;
            }
        }
        
        if (current_fps > 0.f) {
            char fps_text[32];
            int len = snprintf(fps_text, sizeof(fps_text), "FPS: %.1f", current_fps);
            if (len > 0) {
                int tw = 0;
                for (const char *c = fps_text; *c; c++) {
                    int idx = static_cast<unsigned char>(*c);
                    if (idx >= 128) idx = 0;
                    const font_data_t &fd = get_pixel_font_data(idx);
                    tw += fd.sw;
                }
                
               set_draw_color(128, 128, 128, 255);
                fill_rectangle(6, 0, tw + 8, 11);
                set_draw_color(255, 255, 255, 255);
                draw_text(10, 10, fps_text, 0, false);
            }
        }
    }
}

void fbdev_video::frame_render() {
}

bool fbdev_video::frame_drawn() {
    return drawn;
}

void *fbdev_video::get_framebuffer(uint32_t *width, uint32_t *height, size_t *pitch, int *format) {
    *width = fb_width;
    *height = fb_height;
    *pitch = fb_pitch;
    *format = fb_bpp == 32 ? 1 : 2;
    return fb_ptr;
}

void fbdev_video::get_resolution(int &width, int &height) {
    width = fb_width;
    height = fb_height;
}

void fbdev_video::clear() {
    size_t fb_pitch_pixels = fb_pitch / (fb_bpp / 8);
    if (fb_bpp == 32) {
        uint32_t *ptr = static_cast<uint32_t*>(fb_ptr);
        for (int y = 0; y < fb_height; y++) {
            for (int x = 0; x < fb_width; x++) {
                ptr[y * fb_pitch_pixels + x] = 0x00000000;
            }
        }
    } else {
        uint16_t *ptr = static_cast<uint16_t*>(fb_ptr);
        for (int y = 0; y < fb_height; y++) {
            for (int x = 0; x < fb_width; x++) {
                ptr[y * fb_pitch_pixels + x] = 0x0000;
            }
        }
    }
}

void fbdev_video::flip() {
}

void fbdev_video::fill_rectangle(int x, int y, int w, int h) {
    if (x < 0 || y < 0 || x >= fb_width || y >= fb_height) return;
    
    int draw_w = w;
    int draw_h = h;
    if (x + w > fb_width) draw_w = fb_width - x;
    if (y + h > fb_height) draw_h = fb_height - y;
    
    uint32_t color = 0xFFFFFFFF;
    if (fb_bpp == 32) {
        color = (draw_a << 24) | (draw_r << 16) | (draw_g << 8) | draw_b;
    } else {
        uint8_t r5 = (draw_r >> 3);
        uint8_t g6 = (draw_g >> 2);
        uint8_t b5 = (draw_b >> 3);
        color = (r5 << 11) | (g6 << 5) | b5;
    }
    
    size_t fb_pitch_pixels = fb_pitch / (fb_bpp / 8);
    if (fb_bpp == 32) {
        uint32_t *ptr = static_cast<uint32_t*>(fb_ptr);
        ptr += y * fb_pitch_pixels + x;
        size_t row_pitch = fb_pitch_pixels;
        for (int row = 0; row < draw_h; row++) {
            for (int col = 0; col < draw_w; col++) {
                ptr[row * row_pitch + col] = color;
            }
        }
    } else {
        uint16_t *ptr = static_cast<uint16_t*>(fb_ptr);
        ptr += y * fb_pitch_pixels + x;
        size_t row_pitch = fb_pitch_pixels;
        for (int row = 0; row < draw_h; row++) {
            for (int col = 0; col < draw_w; col++) {
                ptr[row * row_pitch + col] = static_cast<uint16_t>(color);
            }
        }
    }
}

void fbdev_video::draw_text(int x, int y, const char *text, int width, bool shadow) {
    draw_text_impl(x, y, text, width, shadow);
}

void fbdev_video::get_text_width_and_height(const char *text, int &w, int &t, int &b) const {
    int text_width = 0;
    int max_height = 0;
    
    for (const char *c = text; *c; c++) {
        if (*c == '\n') {
            w += text_width;
            text_width = 0;
            t += max_height;
            max_height = 0;
            continue;
        }
        
        int idx = static_cast<unsigned char>(*c);
        if (idx >= 128) idx = 0;
        
        const font_data_t &fd = get_pixel_font_data(idx);
        text_width += fd.sw;
        if (fd.h > max_height) max_height = fd.h;
    }
    w += text_width;
    t += max_height;
    b = max_height;
}

void fbdev_video::gui_popup() {
    clear();
}

void fbdev_video::gui_leave() {
}

void fbdev_video::gui_predraw() {
    clear();
}

void fbdev_video::set_draw_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    draw_r = r;
    draw_g = g;
    draw_b = b;
    draw_a = a;
}

void fbdev_video::draw_text_impl(int x, int y, const char *text, int width, bool shadow) {
    int current_x = x;
    int current_y = y;
    int max_width = width;
    if (max_width == 0) {
        max_width = fb_width - x;
    }
    
    uint32_t text_color = 0xFFFFFFFF;
    if (fb_bpp == 32) {
        text_color = (draw_a << 24) | (draw_r << 16) | (draw_g << 8) | draw_b;
    } else {
        uint8_t r5 = (draw_r >> 3);
        uint8_t g6 = (draw_g >> 2);
        uint8_t b5 = (draw_b >> 3);
        text_color = (r5 << 11) | (g6 << 5) | b5;
    }
    
    uint32_t shadow_color = 0x30303030;
    if (fb_bpp == 16) {
        shadow_color = 0x7BEF;
    }
    
   size_t fb_pitch_pixels = fb_pitch / (fb_bpp / 8);
    for (const char *c = text; *c; c++) {
        if (*c == '\n') {
            current_x = x;
            current_y += 9;
            continue;
        }
        
        int idx = static_cast<unsigned char>(*c);
        if (idx >= 128) idx = 0;
        
        const font_data_t &fd = get_pixel_font_data(idx);
        if (max_width > 0 && fd.sw > max_width) {
            break;
        }
        size_t step = (fd.w + 7) >> 3;
        
        if (shadow) {
            for (int py = 0; py < fd.h; py++) {
                const unsigned char *fontdata = fd.data + py * step;
                uint8_t bitflag = 0x01;
                size_t fdidx = 0;
                for (int px = 0; px < fd.w; px++) {
                    if (fontdata[fdidx] & bitflag) {
                        if (fb_bpp == 32) {
                            uint32_t *ptr = static_cast<uint32_t*>(fb_ptr);
                            ptr += (current_y + py + fd.y + 1) * fb_pitch_pixels + (current_x + px + fd.x + 1);
                            *ptr = shadow_color;
                        } else {
                            uint16_t *ptr = static_cast<uint16_t*>(fb_ptr);
                            ptr += (current_y + py + fd.y + 1) * fb_pitch_pixels + (current_x + px + fd.x + 1);
                            *ptr = static_cast<uint16_t>(shadow_color);
                        }
                    }
                    if (bitflag == 0x80) {
                        fdidx++;
                        bitflag = 1;
                    } else {
                        bitflag <<= 1;
                    }
                }
            }
        }
        
        for (int py = 0; py < fd.h; py++) {
            const unsigned char *fontdata = fd.data + py * step;
            uint8_t bitflag = 0x01;
            size_t fdidx = 0;
            for (int px = 0; px < fd.w; px++) {
                if (fontdata[fdidx] & bitflag) {
                    if (fb_bpp == 32) {
                        uint32_t *ptr = static_cast<uint32_t*>(fb_ptr);
                        ptr += (current_y + py + fd.y) * fb_pitch_pixels + (current_x + px + fd.x);
                        *ptr = text_color;
                    } else {
                        uint16_t *ptr = static_cast<uint16_t*>(fb_ptr);
                        ptr += (current_y + py + fd.y) * fb_pitch_pixels + (current_x + px + fd.x);
                        *ptr = static_cast<uint16_t>(text_color);
                    }
                }
                if (bitflag == 0x80) {
                    fdidx++;
                    bitflag = 1;
                } else {
                    bitflag <<= 1;
                }
            }
        }
        
        current_x += fd.sw;
        if (max_width > 0) {
            max_width -= fd.sw;
        }
    }
}

void fbdev_video::convert_xrgb8888_to_rgb565(const uint32_t *src, uint16_t *dst, int pixels) {
    for (int i = 0; i < pixels; i++) {
        uint32_t p = src[i];
        uint8_t r8 = (p >> 16) & 0xFF;
        uint8_t g8 = (p >> 8) & 0xFF;
        uint8_t b8 = p & 0xFF;
        uint16_t r5 = r8 >> 3;
        uint16_t g6 = g8 >> 2;
        uint16_t b5 = b8 >> 3;
        uint16_t out = (r5 << 11) | (g6 << 5) | b5;
        if (game_pixel_format == 1 && p == 0xBDBEBD) {
            LOG(INFO, "COLOR TRACE: input=0x%08X r8=%02X g8=%02X b8=%02X | RGB565: r5=%u g6=%u b5=%u -> output=0x%04X",
                p, r8, g8, b8, r5, g6, b5, out);
        }
        dst[i] = out;
    }
}

void fbdev_video::render_1to1(const void *data, int width, int height, size_t pitch) {
    int offset_x = (fb_width - width) / 2;
    int offset_y = (fb_height - height) / 2;
    
    size_t fb_pitch_pixels = fb_pitch / (fb_bpp / 8);
    
    int input_bpp = (pitch > 0 && width > 0) ? static_cast<int>((pitch / width) * 8) : 16;
    if (input_bpp != 16 && input_bpp != 32) {
        input_bpp = (game_pixel_format == 1) ? 32 : 16;
    }
    
    if (fb_bpp == 32) {
        uint32_t *dest = static_cast<uint32_t*>(fb_ptr);
        dest += offset_y * fb_pitch_pixels + offset_x;
        size_t output_pitch = fb_pitch_pixels;
        
        if (input_bpp == 32) {
            const uint8_t *src = static_cast<const uint8_t*>(data);
            for (int h = 0; h < height; h++) {
                const uint32_t *src_row = reinterpret_cast<const uint32_t*>(src);
                for (int x = 0; x < width; x++) {
                    dest[x] = src_row[x];
                }
                src += pitch;
                dest += output_pitch;
            }
      } else {
             const uint8_t *src = static_cast<const uint8_t*>(data);
             for (int h = 0; h < height; h++) {
                 const uint16_t *src_row = reinterpret_cast<const uint16_t*>(src);
                 for (int x = 0; x < width; x++) {
                     uint16_t p = src_row[x];
                     uint16_t r = (p >> 10) & 0x1F;
                     uint16_t g = (p >> 5) & 0x1F;
                     uint16_t b = p & 0x1F;
                     dest[x] = (r << 19) | (g << 14) | (b << 9) | 0x80000000u;
                 }
                 src += pitch;
                 dest += output_pitch;
             }
         }
  } else {
         uint16_t *dest = static_cast<uint16_t*>(fb_ptr);
         dest += offset_y * fb_pitch_pixels + offset_x;
         size_t output_pitch = fb_pitch_pixels;
         
         if (input_bpp == 32) {
             const uint8_t *src = static_cast<const uint8_t*>(data);
             for (int h = 0; h < height; h++) {
                 const uint32_t *src_row = reinterpret_cast<const uint32_t*>(src);
                 convert_xrgb8888_to_rgb565(src_row, dest, width);
                 src += pitch;
                 dest += output_pitch;
             }
         } else {
             const uint8_t *src = static_cast<const uint8_t*>(data);
             for (int h = 0; h < height; h++) {
                 const uint16_t *src_row = reinterpret_cast<const uint16_t*>(src);
                 memcpy(dest, src_row, width * sizeof(uint16_t));
                 src += pitch;
                 dest += output_pitch;
             }
         }
     }
}

void fbdev_video::render_scaled(const void *data, int width, int height, size_t pitch) {
    int scaled_w = width * scale;
    int scaled_h = height * scale;
    int offset_x = (fb_width - scaled_w) / 2;
    int offset_y = (fb_height - scaled_h) / 2;
    
    int input_bpp = (pitch > 0 && width > 0) ? static_cast<int>((pitch / width) * 8) : 16;
    if (input_bpp != 16 && input_bpp != 32) {
        input_bpp = (game_pixel_format == 1) ? 32 : 16;
    }
    size_t fb_pitch_pixels = fb_pitch / (fb_bpp / 8);
    
    if (fb_bpp == 32) {
        uint32_t *dest = static_cast<uint32_t*>(fb_ptr);
        dest += offset_y * fb_pitch_pixels + offset_x;
        size_t dest_pitch = fb_pitch_pixels;
        
 if (input_bpp == 32) {
             const uint8_t *src = static_cast<const uint8_t*>(data);
             for (int y = 0; y < height; y++) {
                 const uint32_t *src_row = reinterpret_cast<const uint32_t*>(src);
                 for (int x = 0; x < width; x++) {
                     for (int sx = 0; sx < scale; sx++) {
                         this->h_line_32[x * scale + sx] = src_row[x];
                     }
                 }
                 for (int sy = 0; sy < scale; sy++) {
                     memcpy(dest, this->h_line_32, scaled_w * sizeof(uint32_t));
                     dest += dest_pitch;
                 }
                 src += pitch;
             }
         } else {
            const uint8_t *src = static_cast<const uint8_t*>(data);
            for (int y = 0; y < height; y++) {
                const uint16_t *src_row = reinterpret_cast<const uint16_t*>(src);
                for (int x = 0; x < width; x++) {
                    uint16_t p16 = src_row[x];
                    uint16_t r = (p16 >> 10) & 0x1F;
                    uint16_t g = (p16 >> 5) & 0x1F;
                    uint16_t b = p16 & 0x1F;
                    this->h_line_32[x * scale] = (r << 19) | (g << 14) | (b << 9) | 0x80000000u;
                    for (int sx = 1; sx < scale; sx++) {
                        this->h_line_32[x * scale + sx] = this->h_line_32[x * scale];
                    }
                }
                for (int sy = 0; sy < scale; sy++) {
                    memcpy(dest, this->h_line_32, scaled_w * sizeof(uint32_t));
                    dest += dest_pitch;
                }
                src += pitch;
            }
        }
    } else {
        uint16_t *dest = static_cast<uint16_t*>(fb_ptr);
        dest += offset_y * fb_pitch_pixels + offset_x;
        size_t dest_pitch = fb_pitch_pixels;
        
   if (input_bpp == 32) {
             const uint8_t *src = static_cast<const uint8_t*>(data);
             for (int y = 0; y < height; y++) {
                 const uint32_t *src_row = reinterpret_cast<const uint32_t*>(src);
                 for (int x = 0; x < width; x++) {
                     convert_xrgb8888_to_rgb565(src_row + x, this->h_line_16 + x * scale, 1);
                     for (int sx = 1; sx < scale; sx++) {
                         this->h_line_16[x * scale + sx] = this->h_line_16[x * scale];
                     }
                 }
                 for (int sy = 0; sy < scale; sy++) {
                     memcpy(dest, this->h_line_16, scaled_w * sizeof(uint16_t));
                     dest += dest_pitch;
                 }
                 src += pitch;
             }
         } else {
            const uint8_t *src = static_cast<const uint8_t*>(data);
            for (int y = 0; y < height; y++) {
                const uint16_t *src_row = reinterpret_cast<const uint16_t*>(src);
                for (int x = 0; x < width; x++) {
                    uint16_t pix = src_row[x];
                    for (int sx = 0; sx < scale; sx++) {
                        this->h_line_16[x * scale + sx] = pix;
                    }
                }
                for (int sy = 0; sy < scale; sy++) {
                    memcpy(dest, this->h_line_16, scaled_w * sizeof(uint16_t));
                    dest += dest_pitch;
                }
                src += pitch;
            }
        }
  }
}

}

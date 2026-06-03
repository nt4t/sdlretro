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

fbdev_video::fbdev_video(int fb_fd, void *fb_ptr, size_t fb_size, struct fb_var_screeninfo vinfo)
    : fb_fd(fb_fd), fb_ptr(fb_ptr), fb_size(fb_size),
      fb_width(vinfo.xres), fb_height(vinfo.yres),
      fb_bpp(vinfo.bits_per_pixel), fb_pitch(vinfo.xres * vinfo.bits_per_pixel / 8) {
    g_cfg.get_resolution(output_width, output_height);
    if (output_width == 0) output_width = fb_width;
    if (output_height == 0) output_height = fb_height;
    
    game_pixel_format = 2;
    drawn = false;
    frame_count = 0;
    last_fps_time = 0;
    
    LOG(INFO, "fbdev_video: {}x{}, {}bpp, pitch={}", fb_width, fb_height, fb_bpp, fb_pitch);
}

fbdev_video::~fbdev_video() {
    delete[] h_line_16;
    delete[] h_line_32;
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
    
    uint64_t now = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, reinterpret_cast<timespec*>(&now));
    if (last_fps_time == 0) {
        last_fps_time = now;
    }
    
    if (scaling_mode == 0 && scale > 1) {
        render_scaled(data, width, height, pitch);
    } else {
        render_1to1(data, width, height, pitch);
    }
    
    if (fps_enabled) {
        uint64_t elapsed = now - last_fps_time;
        if (elapsed >= 1000000000ULL) {
            current_fps = static_cast<float>(frame_count) * 1000000000.0f / static_cast<float>(elapsed);
            frame_count = 0;
            last_fps_time = now;
        }
        
        if (current_fps > 0.f) {
            char fps_text[32];
            int len = snprintf(fps_text, sizeof(fps_text), "FPS: %.1f", current_fps);
            if (len > 0) {
                draw_text(10, 10, fps_text, 0, true);
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
    if (fb_bpp == 32) {
        uint32_t *ptr = static_cast<uint32_t*>(fb_ptr);
        size_t pixels = fb_width * fb_height;
        for (size_t i = 0; i < pixels; i++) {
            ptr[i] = 0x00000000;
        }
    } else {
        uint16_t *ptr = static_cast<uint16_t*>(fb_ptr);
        size_t pixels = fb_width * fb_height;
        for (size_t i = 0; i < pixels; i++) {
            ptr[i] = 0x0000;
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
    
    if (fb_bpp == 32) {
        uint32_t *ptr = static_cast<uint32_t*>(fb_ptr);
        ptr += y * fb_width + x;
        size_t row_pitch = fb_width;
        for (int row = 0; row < draw_h; row++) {
            for (int col = 0; col < draw_w; col++) {
                ptr[row * row_pitch + col] = color;
            }
        }
    } else {
        uint16_t *ptr = static_cast<uint16_t*>(fb_ptr);
        ptr += y * fb_width + x;
        size_t row_pitch = fb_width;
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

void fbdev_video::set_draw_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    draw_r = r;
    draw_g = g;
    draw_b = b;
    draw_a = a;
}

void fbdev_video::draw_text_impl(int x, int y, const char *text, int width, bool shadow) {
    int current_x = x;
    int current_y = y;
    
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
    
    for (const char *c = text; *c; c++) {
        if (*c == '\n') {
            current_x = x;
            current_y += 9;
            continue;
        }
        
        int idx = static_cast<unsigned char>(*c);
        if (idx >= 128) idx = 0;
        
        const font_data_t &fd = get_pixel_font_data(idx);
        
        if (shadow) {
            for (int py = 0; py < fd.h; py++) {
                const unsigned char *src = fd.data + py * ((fd.w + 7) / 8);
                for (int px = 0; px < fd.w; px++) {
                    if ((src[px / 8] >> (7 - (px % 8))) & 1) {
                        if (fb_bpp == 32) {
                            uint32_t *ptr = static_cast<uint32_t*>(fb_ptr);
                            ptr += (current_y + py + 1) * fb_width + (current_x + px + 1);
                            *ptr = shadow_color;
                        } else {
                            uint16_t *ptr = static_cast<uint16_t*>(fb_ptr);
                            ptr += (current_y + py + 1) * fb_width + (current_x + px + 1);
                            *ptr = static_cast<uint16_t>(shadow_color);
                        }
                    }
                }
            }
        }
        
        for (int py = 0; py < fd.h; py++) {
            const unsigned char *src = fd.data + py * ((fd.w + 7) / 8);
            for (int px = 0; px < fd.w; px++) {
                if ((src[px / 8] >> (7 - (px % 8))) & 1) {
                    if (fb_bpp == 32) {
                        uint32_t *ptr = static_cast<uint32_t*>(fb_ptr);
                        ptr += (current_y + py) * fb_width + (current_x + px);
                        *ptr = text_color;
                    } else {
                        uint16_t *ptr = static_cast<uint16_t*>(fb_ptr);
                        ptr += (current_y + py) * fb_width + (current_x + px);
                        *ptr = static_cast<uint16_t>(text_color);
                    }
                }
            }
        }
        
        current_x += fd.sw;
    }
}

void fbdev_video::convert_xrgb8888_to_rgb565(const uint32_t *src, uint16_t *dst, int pixels) {
    for (int i = 0; i < pixels; i++) {
        uint32_t p = src[i];
        uint16_t r = (p >> 16) & 0x1F;
        uint16_t g = (p >> 8) & 0x3F;
        uint16_t b = p & 0x1F;
        dst[i] = (r << 11) | (g << 5) | b;
    }
}

void fbdev_video::render_1to1(const void *data, int width, int height, size_t pitch) {
    int offset_x = (fb_width - width) / 2;
    int offset_y = (fb_height - height) / 2;
    
    if (fb_bpp == 32) {
        uint32_t *dest = static_cast<uint32_t*>(fb_ptr);
        dest += offset_y * fb_width + offset_x;
        size_t output_pitch = fb_width;
        
        if (game_pixel_format == 1) {
            const uint32_t *src = static_cast<const uint32_t*>(data);
            size_t input_row_bytes = pitch;
            for (int h = 0; h < height; h++) {
                memcpy(dest, src, width * sizeof(uint32_t));
                src += input_row_bytes / 4;
                dest += output_pitch;
            }
        } else {
            const uint16_t *src = static_cast<const uint16_t*>(data);
            size_t input_row_bytes = pitch;
            for (int h = 0; h < height; h++) {
                for (int x = 0; x < width; x++) {
                    uint16_t p16 = src[x];
                    uint16_t r = (p16 >> 10) & 0x1F;
                    uint16_t g = (p16 >> 5) & 0x1F;
                    uint16_t b = p16 & 0x1F;
                    dest[x] = (r << 19) | (g << 14) | (b << 9) | 0x80000000u;
                }
                src += input_row_bytes / 2;
                dest += output_pitch;
            }
        }
    } else {
        uint16_t *dest = static_cast<uint16_t*>(fb_ptr);
        dest += offset_y * fb_width + offset_x;
        size_t output_pitch = fb_width;
        
        if (game_pixel_format == 1) {
            const uint32_t *src = static_cast<const uint32_t*>(data);
            size_t input_row_bytes = pitch;
            for (int h = 0; h < height; h++) {
                convert_xrgb8888_to_rgb565(src, dest, width);
                src += input_row_bytes / 4;
                dest += output_pitch;
            }
        } else {
            const uint16_t *src = static_cast<const uint16_t*>(data);
            size_t input_row_bytes = pitch;
            for (int h = 0; h < height; h++) {
                memcpy(dest, src, width * sizeof(uint16_t));
                src += input_row_bytes / 2;
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
    
    if (fb_bpp == 32) {
        uint32_t *dest = static_cast<uint32_t*>(fb_ptr);
        dest += offset_y * fb_width + offset_x;
        size_t dest_pitch = fb_width;
        
        if (game_pixel_format == 1) {
            const uint32_t *src = static_cast<const uint32_t*>(data);
            size_t input_row_bytes = pitch;
            
            for (int y = 0; y < height; y++) {
                const uint32_t *src_row = src + y * (input_row_bytes / 4);
                
                for (int x = 0; x < width; x++) {
                    uint32_t p = src_row[x];
                    for (int sx = 0; sx < scale; sx++) {
                        h_line_32[x * scale + sx] = p;
                    }
                }
                
                for (int sy = 0; sy < scale; sy++) {
                    memcpy(dest, h_line_32, scaled_w * sizeof(uint32_t));
                    dest += dest_pitch;
                }
            }
        } else {
            const uint16_t *src = static_cast<const uint16_t*>(data);
            size_t input_row_bytes = pitch;
            
            for (int y = 0; y < height; y++) {
                const uint16_t *src_row = src + y * (input_row_bytes / 2);
                
                for (int x = 0; x < width; x++) {
                    uint16_t p16 = src_row[x];
                    uint16_t r = (p16 >> 10) & 0x1F;
                    uint16_t g = (p16 >> 5) & 0x1F;
                    uint16_t b = p16 & 0x1F;
                    h_line_32[x * scale] = (r << 19) | (g << 14) | (b << 9) | 0x80000000u;
                    for (int sx = 1; sx < scale; sx++) {
                        h_line_32[x * scale + sx] = h_line_32[x * scale];
                    }
                }
                
                for (int sy = 0; sy < scale; sy++) {
                    memcpy(dest, h_line_32, scaled_w * sizeof(uint32_t));
                    dest += dest_pitch;
                }
            }
        }
    } else {
        uint16_t *dest = static_cast<uint16_t*>(fb_ptr);
        dest += offset_y * fb_width + offset_x;
        size_t dest_pitch = fb_width;
        
        if (game_pixel_format == 1) {
            const uint32_t *src = static_cast<const uint32_t*>(data);
            size_t input_row_bytes = pitch;
            
            for (int y = 0; y < height; y++) {
                const uint32_t *src_row = src + y * (input_row_bytes / 4);
                
                for (int x = 0; x < width; x++) {
                    uint32_t p = src_row[x];
                    uint16_t r = (p >> 16) & 0x1F;
                    uint16_t g = (p >> 8) & 0x3F;
                    uint16_t b = p & 0x1F;
                    uint16_t pix = (r << 11) | (g << 5) | b;
                    
                    for (int sx = 0; sx < scale; sx++) {
                        h_line_16[x * scale + sx] = pix;
                    }
                }
                
                for (int sy = 0; sy < scale; sy++) {
                    memcpy(dest, h_line_16, scaled_w * sizeof(uint16_t));
                    dest += dest_pitch;
                }
            }
        } else {
            const uint16_t *src = static_cast<const uint16_t*>(data);
            size_t input_row_bytes = pitch;
            
            for (int y = 0; y < height; y++) {
                const uint16_t *src_row = src + y * (input_row_bytes / 2);
                
                for (int x = 0; x < width; x++) {
                    uint16_t pix = src_row[x];
                    for (int sx = 0; sx < scale; sx++) {
                        h_line_16[x * scale + sx] = pix;
                    }
                }
                
                for (int sy = 0; sy < scale; sy++) {
                    memcpy(dest, h_line_16, scaled_w * sizeof(uint16_t));
                    dest += dest_pitch;
                }
            }
        }
    }
}

}

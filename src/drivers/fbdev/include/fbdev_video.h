#pragma once

#include "video_base.h"

#include <linux/fb.h>

#include <cstddef>
#include <cstdint>

namespace drivers {

class fbdev_video: public video_base {
public:
    fbdev_video(int fb_fd, void *fb_ptr, size_t fb_size, struct fb_var_screeninfo vinfo);
    ~fbdev_video() override;

    void window_resized(int width, int height, bool fullscreen) override;
    bool game_resolution_changed(int width, int height, int max_width, int max_height, uint32_t pixel_format) override;
    void render(const void *data, int width, int height, size_t pitch) override;
    void frame_render() override;
    bool frame_drawn() override;

    void *get_framebuffer(uint32_t *width, uint32_t *height, size_t *pitch, int *format) override;
    void get_resolution(int &width, int &height) override;
    void clear() override;
    void flip() override;

    void draw_text(int x, int y, const char *text, int width, bool shadow) override;
    void get_text_width_and_height(const char *text, int &w, int &t, int &b) const override;
    void fill_rectangle(int x, int y, int w, int h) override;

    void gui_popup() override;
    void gui_leave() override;
    void gui_predraw() override;

    void set_fps_enabled(bool enabled) { fps_enabled = enabled; }
    bool get_fps_enabled() const { return fps_enabled; }

    void set_draw_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;

private:
    void draw_text_impl(int x, int y, const char *text, int width, bool shadow);
    void render_scaled(const void *data, int width, int height, size_t pitch);
    void render_1to1(const void *data, int width, int height, size_t pitch);
    void convert_xrgb8888_to_rgb565(const uint32_t *src, uint16_t *dst, int pixels);

    int fb_fd = -1;
    void *fb_ptr = nullptr;
    size_t fb_size = 0;
    
    int fb_width = 0;
    int fb_height = 0;
    int fb_bpp = 0;
    size_t fb_pitch = 0;
    
    int game_width = 0;
    int game_height = 0;
    int game_max_width = 0;
    int game_max_height = 0;
    uint32_t game_pixel_format = 2;
    
    int output_width = 0;
    int output_height = 0;
    int scale = 1;
    int scaling_mode = 0;
    
    uint16_t *h_line_16 = nullptr;
    uint32_t *h_line_32 = nullptr;
    size_t h_line_size = 0;
    
    void *game_frame_buffer = nullptr;
    size_t game_frame_size = 0;
    
    bool drawn = false;
    bool fps_enabled = false;
    int frame_count = 0;
    float current_fps = 0.f;
    uint64_t last_fps_time = 0;
    uint64_t last_console_fps_time = 0;
    
    uint8_t draw_r = 255;
    uint8_t draw_g = 255;
    uint8_t draw_b = 255;
    uint8_t draw_a = 255;
};

}

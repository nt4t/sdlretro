#pragma once

#include "video_base.h"

#include <cstdint>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

namespace drivers {

class x11_shm_video: public video_base {
public:
    x11_shm_video();
    ~x11_shm_video() override;

    bool init_video(int width, int height);
    void deinit_video();

    int get_renderer_type() override { return 0; }
    bool init_hw_renderer(retro_hw_render_callback*) override { return false; }
    void inited_hw_renderer() override {}
    void deinit_hw_renderer() override {}
    void window_resized(int width, int height, bool fullscreen) override;
    bool game_resolution_changed(int width, int height, int max_width, int max_height, uint32_t pixel_format) override;
    void render(const void *data, int width, int height, size_t pitch) override;
    void frame_render() override;
    void *get_framebuffer(uint32_t *width, uint32_t *height, size_t *pitch, int *format) override { return nullptr; }
    bool frame_drawn() override { return drawn; }
    void get_resolution(int &width, int &height) override {
        width = curr_width; height = curr_height;
    }
    void clear() override;
    void flip() override {}
    void process_x11_events() override;
    int get_font_size() const override { return 12; }
    void set_draw_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;
    void draw_rectangle(int x, int y, int w, int h) override;
    void fill_rectangle(int x, int y, int w, int h) override;
    void draw_text(int x, int y, const char *text, int width, bool shadow) override;
    void get_text_width_and_height(const char *text, int &w, int &t, int &b) const override;
    void gui_predraw() override;
    void config_changed() override {}

private:
    void draw_text_impl(int x, int y, const char *text, int width, bool shadow);
    void draw_overlay();

    Display *display = nullptr;
    Window window = None;
    GC gc = None;
    XImage *shm_image = nullptr;
    XShmSegmentInfo shm_info{};
    char *shm_data = nullptr;
    bool shm_avail = false;

    int curr_width = 0, curr_height = 0;
    int game_width = 0, game_height = 0;
    int game_max_width = 0, game_max_height = 0;
    bool drawn = false;
    bool overlay_dirty = false;

    struct {
        uint8_t r = 255, g = 255, b = 255, a = 255;
    } draw_color;

    struct {
        int x = 0, y = 0, w = 0, h = 0;
        std::string text;
        bool shadow = false;
    } overlay_rect;
};

}

#ifdef SDLRETRO_X11_SHM

#include "x11_shm_video.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <cstring>
#include <cstdio>

namespace drivers {

x11_shm_video::x11_shm_video() {
    memset(&draw_color, 0, sizeof(draw_color));
    memset(&overlay_rect, 0, sizeof(overlay_rect));
}

x11_shm_video::~x11_shm_video() {
    deinit_video();
}

bool x11_shm_video::init_video(int width, int height) {
    display = XOpenDisplay(nullptr);
    if (!display) {
        fprintf(stderr, "x11_shm: XOpenDisplay failed\n");
        return false;
    }

    int screen = DefaultScreen(display);
    Visual *visual = DefaultVisual(display, screen);
    int depth = DefaultDepth(display, screen);

    window = XCreateSimpleWindow(display, RootWindow(display, screen),
        0, 0, width, height, 0,
        BlackPixel(display, screen), WhitePixel(display, screen));

    if (!window) {
        fprintf(stderr, "x11_shm: XCreateSimpleWindow failed\n");
        XCloseDisplay(display);
        display = nullptr;
        return false;
    }

    XSelectInput(display, window, KeyPressMask | KeyReleaseMask |
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

    XMapWindow(display, window);

    XSync(display, False);

    if (!XShmQueryExtension(display)) {
        fprintf(stderr, "x11_shm: XShm extension not available\n");
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        display = nullptr;
        return false;
    }

    shm_image = XShmCreateImage(display, visual, depth, ZPixmap,
        nullptr, nullptr, width * 4, 32);
    if (!shm_image) {
        fprintf(stderr, "x11_shm: XShmCreateImage failed\n");
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        display = nullptr;
        return false;
    }

    int shmid = shmget(IPC_PRIVATE, width * height * 4, IPC_CREAT | 0600);
    if (shmid < 0) {
        fprintf(stderr, "x11_shm: shmget failed\n");
        XDestroyImage(shm_image);
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        display = nullptr;
        return false;
    }

    shm_data = (char *)shmat(shmid, nullptr, 0);
    if (shm_data == (char *)-1) {
        fprintf(stderr, "x11_shm: shmat failed\n");
        shmctl(shmid, IPC_RMID, nullptr);
        XDestroyImage(shm_image);
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        display = nullptr;
        return false;
    }

    shm_image->data = shm_data;
    shm_segment = (ShmSeg *)(intptr_t)shmid;
    shm_avail = true;

    if (!XShmAttach(display, shm_segment)) {
        fprintf(stderr, "x11_shm: XShmAttach failed\n");
        shmdt(shm_data);
        shmctl(shmid, IPC_RMID, nullptr);
        XDestroyImage(shm_image);
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        display = nullptr;
        return false;
    }

    XSync(display, False);

    gc = XCreateGC(display, window, 0, nullptr);

    curr_width = width;
    curr_height = height;
    game_width = width;
    game_height = height;

    return true;
}

void x11_shm_video::deinit_video() {
    if (shm_image) {
        if (shm_avail && shm_segment) {
            XShmDetach(display, shm_segment);
        }
        if (shm_data) {
            shmdt(shm_data);
            shm_data = nullptr;
        }
        XDestroyImage(shm_image);
        shm_image = nullptr;
    }

    if (shm_segment) {
        int shmid = (int)(intptr_t)shm_segment;
        shmctl(shmid, IPC_RMID, nullptr);
        shm_segment = nullptr;
    }

    if (gc) {
        XFreeGC(display, gc);
        gc = None;
    }

    if (window) {
        XDestroyWindow(display, window);
        window = None;
    }

    if (display) {
        XCloseDisplay(display);
        display = nullptr;
    }

    curr_width = 0;
    curr_height = 0;
}

void x11_shm_video::window_resized(int width, int height, bool fullscreen) {
    if (display && window) {
        XResizeWindow(display, window, width, height);
        XSync(display, False);
    }
    curr_width = width;
    curr_height = height;
}

bool x11_shm_video::game_resolution_changed(int width, int height, int max_width, int max_height, uint32_t pixel_format) {
    game_width = width;
    game_height = height;
    game_max_width = max_width;
    game_max_height = max_height;

    if (shm_image && (width != curr_width || height != curr_height)) {
        int shmid = 0;
        if (shm_avail && shm_segment) {
            shmid = (int)(intptr_t)shm_segment;
            XShmDetach(display, shm_segment);
            shm_segment = nullptr;
        }
        if (shm_data) {
            shmdt(shm_data);
            shm_data = nullptr;
        }
        if (shmid) {
            shmctl(shmid, IPC_RMID, nullptr);
        }
        XDestroyImage(shm_image);
        shm_image = nullptr;
    }

    if (!shm_image) {
        XSendEvent(display, window, False, 0, nullptr);
        return false;
    }

    return true;
}

void x11_shm_video::render(const void *data, int width, int height, size_t pitch) {
    if (!shm_image || !shm_data || !display || !window) {
        return;
    }

    drawn = true;

    const uint32_t *src = (const uint32_t *)data;
    int src_stride = (int)pitch / 4;

    for (int y = 0; y < height && y < shm_image->height; y++) {
        uint32_t *dst = (uint32_t *)(shm_image->data + y * shm_image->bytes_per_line);
        const uint32_t *src_row = src + y * src_stride;
        memcpy(dst, src_row, width * 4);
    }

    XShmPutImage(display, window, gc, shm_image,
        0, 0, 0, 0, width, height, False);
}

void x11_shm_video::frame_render() {
    draw_overlay();
}

void x11_shm_video::clear() {
    if (!display || !window || !gc) return;
    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
    XFillRectangle(display, window, gc, 0, 0, curr_width, curr_height);
}

void x11_shm_video::set_draw_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    draw_color.r = r;
    draw_color.g = g;
    draw_color.b = b;
    draw_color.a = a;
}

void x11_shm_video::draw_rectangle(int x, int y, int w, int h) {
    if (!display || !window || !gc) return;
    unsigned long pixel = (draw_color.r << 16) | (draw_color.g << 8) | draw_color.b;
    XSetForeground(display, gc, pixel);
    XDrawRectangle(display, window, gc, x, y, w - 1, h - 1);
}

void x11_shm_video::fill_rectangle(int x, int y, int w, int h) {
    if (!display || !window || !gc) return;
    unsigned long pixel = (draw_color.r << 16) | (draw_color.g << 8) | draw_color.b;
    XSetForeground(display, gc, pixel);
    XFillRectangle(display, window, gc, x, y, w, h);
}

void x11_shm_video::draw_text(int x, int y, const char *text, int width, bool shadow) {
    (void)width;
    if (!display || !window || !gc) return;
    draw_text_impl(x, y, text, width, shadow);
}

void x11_shm_video::get_text_width_and_height(const char *text, int &w, int &t, int &b) const {
    if (!display) {
        w = 0; t = 0; b = 0;
        return;
    }
    XRectangle rect;
    XTextExtents16(display, (const XChar2b *)text, strlen(text), &rect);
    w = rect.width;
    t = 0;
    b = 12;
}

void x11_shm_video::draw_text_impl(int x, int y, const char *text, int width, bool shadow) {
    (void)width;
    if (!display || !window || !gc) return;

    unsigned long pixel = (draw_color.r << 16) | (draw_color.g << 8) | draw_color.b;
    XSetForeground(display, gc, pixel);

    if (shadow) {
        XDrawString(display, window, gc, x + 1, y + 2, text, strlen(text));
    }
    XDrawString(display, window, gc, x, y, text, strlen(text));
}

void x11_shm_video::gui_predraw() {
    overlay_dirty = true;
}

void x11_shm_video::draw_overlay() {
    if (!overlay_dirty || !display || !window || !gc) return;
    overlay_dirty = false;

    if (overlay_rect.w > 0 && overlay_rect.h > 0) {
        unsigned long bg_pixel = (0x40 << 16) | (0x40 << 8) | 0x40;
        XSetForeground(display, gc, bg_pixel);
        XFillRectangle(display, window, gc, overlay_rect.x, overlay_rect.y,
            overlay_rect.w, overlay_rect.h);

        unsigned long text_pixel = (0xFF << 16) | (0xFF << 8) | 0xFF;
        XSetForeground(display, gc, text_pixel);
        XDrawString(display, window, gc,
            overlay_rect.x + 10, overlay_rect.y + 20,
            overlay_rect.text.c_str(), overlay_rect.text.length());
    }
}

}

#endif

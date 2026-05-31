#ifdef SDLRETRO_X11_SHM

#include "x11_shm_video.h"

#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <cstring>
#include <cstdio>
#include <iostream>

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
    std::cerr << "x11_shm: XOpenDisplay OK\n";

    int screen = DefaultScreen(display);
    Visual *visual = DefaultVisual(display, screen);
    int depth = DefaultDepth(display, screen);
    std::cerr << "x11_shm: screen=" << screen << " depth=" << depth << "\n";

    window = XCreateSimpleWindow(display, RootWindow(display, screen),
        0, 0, width, height, 0,
        BlackPixel(display, screen), WhitePixel(display, screen));

    XStoreName(display, window, "SDLRetro");

    if (!window) {
        fprintf(stderr, "x11_shm: XCreateSimpleWindow failed\n");
        XCloseDisplay(display);
        display = nullptr;
        return false;
    }
    std::cerr << "x11_shm: XCreateSimpleWindow OK (win=" << window << ")\n";

    XSelectInput(display, window, KeyPressMask | KeyReleaseMask |
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

    XMapWindow(display, window);
    XRaiseWindow(display, window);
    XFlush(display);

    XSync(display, False);

    // Verify window is visible
    Window root2, child;
    int rx = 0, ry = 0;
    unsigned int wx = 0, wy = 0, bw2 = 0, dep2 = 0;
    if (XGetGeometry(display, window, &root2, &rx, &ry, &wx, &wy, &bw2, &dep2)) {
        std::cerr << "x11_shm: window geom: pos=(" << rx << "," << ry << ") size=" << wx << "x" << wy << " depth=" << dep2 << "\n";
    }

    if (!XShmQueryExtension(display)) {
        fprintf(stderr, "x11_shm: XShm extension not available\n");
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        display = nullptr;
        return false;
    }
    std::cerr << "x11_shm: XShmQueryExtension OK\n";

  shm_image = XShmCreateImage(display, visual, depth, ZPixmap,
        nullptr, &shm_info, width, height);
    if (!shm_image) {
        fprintf(stderr, "x11_shm: XShmCreateImage failed\n");
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        display = nullptr;
        return false;
    }
    shm_data = shm_info.shmaddr;
    std::cerr << "x11_shm: XShmCreateImage OK (shmid=" << shm_info.shmid
              << " addr=" << shm_data << ")\n";

    if (!XShmAttach(display, &shm_info)) {
        fprintf(stderr, "x11_shm: XShmAttach failed\n");
        shmdt(shm_data);
        shmctl(shm_info.shmid, IPC_RMID, nullptr);
        XDestroyImage(shm_image);
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        display = nullptr;
        return false;
    }
    shm_avail = true;
    std::cerr << "x11_shm: XShmAttach OK\n";

    XSync(display, False);

    gc = XCreateGC(display, window, 0, nullptr);

    curr_width = width;
    curr_height = height;
    game_width = width;
    game_height = height;

    std::cerr << "x11_shm: init_video SUCCESS\n";
    return true;
}

void x11_shm_video::deinit_video() {
    if (shm_image) {
        if (shm_avail) {
            XShmDetach(display, &shm_info);
        }
        if (shm_data) {
            shmdt(shm_data);
            shm_data = nullptr;
        }
        XDestroyImage(shm_image);
        shm_image = nullptr;
    }

    if (shm_avail) {
        shmctl(shm_info.shmid, IPC_RMID, nullptr);
        shm_avail = false;
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

    std::cerr << "x11_shm: game_resolution_changed " << width << "x" << height
              << " (curr=" << curr_width << "x" << curr_height
              << " shm=" << (void*)shm_image << " avail=" << shm_avail << ")\n";

    if (!shm_image) {
        std::cerr << "x11_shm: shm_image is null!\n";
        return false;
    }

    curr_width = width;
    curr_height = height;
    return true;
}

void x11_shm_video::render(const void *data, int width, int height, size_t pitch) {
    if (!shm_image || !shm_data || !display || !window) {
        return;
    }

    drawn = true;

    static int frame_count = 0;
    frame_count++;
    if (frame_count <= 3) {
        std::cerr << "x11_shm: render frame=" << frame_count
                  << " game=" << width << "x" << height
                  << " shm=" << shm_image->width << "x" << shm_image->height
                  << " bpp=" << shm_image->bits_per_pixel
                  << " bpl=" << shm_image->bytes_per_line
                  << " data=" << (void*)shm_image->data
                  << " shm_avail=" << shm_avail
                  << " shmid=" << shm_info.shmid << "\n";
    }

    const uint32_t *src = (const uint32_t *)data;
    int src_stride = (int)pitch / 4;

    for (int y = 0; y < height && y < shm_image->height; y++) {
        uint32_t *dst = (uint32_t *)(shm_image->data + y * shm_image->bytes_per_line);
        const uint32_t *src_row = src + y * src_stride;
        memcpy(dst, src_row, width * 4);
    }

    if (XShmPutImage(display, window, gc, shm_image,
        0, 0, 0, 0, width, height, False) == 0) {
        std::cerr << "x11_shm: XShmPutImage returned 0!\n";
    }
    XFlush(display);
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
    (void)text;
    w = (int)strlen(text) * 7;
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

void x11_shm_video::process_x11_events() {
    if (!display) return;
    XEvent event;
    while (XPending(display)) {
        XNextEvent(display, &event);
        if (event.type == KeyPress || event.type == KeyRelease) {
            KeySym keysym = XLookupKeysym((XKeyEvent*)&event, 0);
            int state = event.type == KeyPress ? 1 : 0;
            std::cerr << "x11_shm: key " << (event.type == KeyPress ? "down" : "up")
                      << " keysym=" << keysym << " state=" << state << "\n";
            if (x11_key_callback) {
                switch (keysym) {
                    case XK_Escape:     x11_key_callback(256, state); break;
                    case XK_F1:         x11_key_callback(265, state); break;
                    case XK_Tab:        x11_key_callback(270, state); break;
                    case XK_Return:     x11_key_callback(257, state); break;
                    case XK_space:      x11_key_callback(258, state); break;
                    case XK_Left:       x11_key_callback(263, state); break;
                    case XK_Right:      x11_key_callback(262, state); break;
                    case XK_Up:         x11_key_callback(264, state); break;
                    case XK_Down:       x11_key_callback(265, state); break;
                    case XK_BackSpace:  x11_key_callback(259, state); break;
                    case XK_Delete:     x11_key_callback(261, state); break;
                    default: break;
                }
            }
        } else if (event.type == ButtonPress || event.type == ButtonRelease) {
            int btn = event.xbutton.button;
            int state = event.type == ButtonPress ? 1 : 0;
            if (x11_key_callback) {
                switch (btn) {
                    case 1: x11_key_callback(1024 + 1, state); break;
                    case 2: x11_key_callback(1024 + 2, state); break;
                    case 3: x11_key_callback(1024 + 3, state); break;
                    default: break;
                }
            }
        } else if (event.type == MotionNotify) {
            // TODO: mouse motion
        }
    }
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

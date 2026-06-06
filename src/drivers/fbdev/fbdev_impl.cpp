#include "fbdev_impl.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/msync.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <dirent.h>

#include "driver_base.h"
#include "fbdev_video.h"
#include "fbdev_input.h"
#include "fbdev_audio.h"
#include "logger.h"

#include <core.h>
#include <cfg.h>

#include <cstdint>
#include <cstdio>

namespace drivers {

fbdev_impl::fbdev_impl() {
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        fprintf(stderr, "fbdev: Failed to open /dev/fb0: %m\n");
        return;
    }
    
    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        fprintf(stderr, "fbdev: Failed to get var screen info: %m\n");
        close(fb_fd);
        return;
    }
    
    struct fb_fix_screeninfo finfo;
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        fprintf(stderr, "fbdev: Failed to get fix screen info: %m\n");
        close(fb_fd);
        return;
    }
    
    size_t fb_size = finfo.smem_len;
    
    void *fb_ptr = nullptr;
    int mmap_flags = MAP_SHARED;
    fb_ptr = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, mmap_flags, fb_fd, 0);
#ifdef MAP_WRITECOMBINE
    if (fb_ptr == MAP_FAILED) {
        mmap_flags = MAP_SHARED | MAP_WRITECOMBINE;
        fb_ptr = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, mmap_flags, fb_fd, 0);
        if (fb_ptr == MAP_FAILED) {
            mmap_flags = MAP_SHARED;
            fb_ptr = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, mmap_flags, fb_fd, 0);
        }
    }
#endif
    if (fb_ptr == MAP_FAILED) {
        fprintf(stderr, "fbdev: Failed to mmap: %m\n");
        close(fb_fd);
        return;
    }
    
#ifdef MAP_WRITECOMBINE
    if (mmap_flags & MAP_WRITECOMBINE) {
        LOG(INFO, "fbdev: Using write-combining mmap");
    } else {
        LOG(INFO, "fbdev: mmap (no WC support)");
    }
#else
    LOG(INFO, "fbdev: mmap (WC not available)");
#endif
    
    madvise(fb_ptr, fb_size, MADV_SEQUENTIAL);
    
    struct ioc_fb_clean_cache ioc_clean = {};
    if (ioctl(fb_fd, IOC_FB_CLEAN_CACHE, &ioc_clean) == 0) {
        LOG(INFO, "fbdev: Cache cleaned via IOC_FB_CLEAN_CACHE");
    }
    
    video = std::make_shared<fbdev_video>(fb_fd, fb_ptr, fb_size, vinfo, finfo, mmap_flags);
    input = std::make_shared<fbdev_input>();
    audio = std::make_shared<fbdev_audio>();
    input->post_init();
    
    LOG(INFO, "Render backend: fbdev ({}x{}, {}bpp)", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
}

fbdev_impl::~fbdev_impl() {
    video.reset();
    input.reset();
    audio.reset();
}

bool fbdev_impl::process_events() {
    input->input_poll();
    
    auto *fb_input = static_cast<fbdev_input*>(input.get());
    for (const auto &ev : fb_input->get_stored_events()) {
        if (ev.type == EV_KEY) {
            uint16_t keycode = ev.code;
            bool pressed = ev.value != 0;
            
            if (pressed && keycode == KEY_F1) {
                if (!menu_button_pressed)
                    menu_button_pressed = true;
                else {
                    fb_input->clear_stored_events();
                    return true;
                }
            } else if (pressed && keycode == KEY_ESC) {
                fb_input->clear_stored_events();
                return true;
            } else if (pressed && keycode == KEY_F3) {
                auto *fb_video = static_cast<fbdev_video*>(video.get());
                fb_video->set_fps_enabled(!fb_video->get_fps_enabled());
                LOG(INFO, "FPS display: {}", fb_video->get_fps_enabled() ? "on" : "off");
            }
            
            uint16_t sdlk = fb_input->keycode_to_sdlk(keycode);
            if (sdlk != 0) {
                input->on_km_input(sdlk, pressed);
            }
        }
    }
    fb_input->clear_stored_events();
    return false;
}

bool fbdev_impl::init() {
    return true;
}

void fbdev_impl::deinit() {
}

void fbdev_impl::unload() {
}

}

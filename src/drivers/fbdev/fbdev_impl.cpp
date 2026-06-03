#include "fbdev_impl.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
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
    void *fb_ptr = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_ptr == MAP_FAILED) {
        fprintf(stderr, "fbdev: Failed to mmap: %m\n");
        close(fb_fd);
        return;
    }
    
    video = std::make_shared<fbdev_video>(fb_fd, fb_ptr, fb_size, vinfo);
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
    
    struct input_event ev;
    ssize_t bytes;
    auto *fb_input = static_cast<fbdev_input*>(input.get());
    for (int fd : fb_input->get_event_fds()) {
        while ((bytes = fb_input->read_event_from_fd(fd, &ev)) > 0) {
            if (ev.type == EV_KEY) {
                uint16_t keycode = ev.code;
                bool pressed = ev.value != 0;
                
                if (pressed && keycode == KEY_F1) {
                    if (!menu_button_pressed)
                        menu_button_pressed = true;
                    else
                        return true;
                } else if (pressed && keycode == KEY_ESC) {
                    return true;
                }
            }
        }
    }
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

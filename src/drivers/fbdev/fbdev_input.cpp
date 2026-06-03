#include "fbdev_input.h"

#include <cfg.h>
#include <logger.h>

#include <linux/input.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <cstring>
#include <cstdio>

namespace drivers {

static uint16_t keycode_to_sdlk(uint16_t keycode) {
    switch (keycode) {
        case KEY_1: return 49;
        case KEY_2: return 50;
        case KEY_3: return 51;
        case KEY_4: return 52;
        case KEY_5: return 53;
        case KEY_6: return 54;
        case KEY_7: return 55;
        case KEY_8: return 56;
        case KEY_9: return 57;
        case KEY_0: return 48;
        case KEY_A: return 97;
        case KEY_B: return 98;
        case KEY_C: return 99;
        case KEY_D: return 100;
        case KEY_E: return 101;
        case KEY_F: return 102;
        case KEY_G: return 103;
        case KEY_H: return 104;
        case KEY_I: return 105;
        case KEY_J: return 106;
        case KEY_K: return 107;
        case KEY_L: return 108;
        case KEY_M: return 109;
        case KEY_N: return 110;
        case KEY_O: return 111;
        case KEY_P: return 112;
        case KEY_Q: return 113;
        case KEY_R: return 114;
        case KEY_S: return 115;
        case KEY_T: return 116;
        case KEY_U: return 117;
        case KEY_V: return 118;
        case KEY_W: return 119;
        case KEY_X: return 120;
        case KEY_Y: return 121;
        case KEY_Z: return 122;
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT: return 304;
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL: return 29;
        case KEY_LEFTALT:
        case KEY_RIGHTALT: return 56;
        case KEY_ESC: return 27;
        case KEY_TAB: return 9;
        case KEY_ENTER: return 13;
        case KEY_SPACE: return 32;
        case KEY_LEFT: return 263;
        case KEY_RIGHT: return 262;
        case KEY_UP: return 265;
        case KEY_DOWN: return 264;
        case KEY_F1: return 282;
        case KEY_F2: return 283;
        case KEY_F3: return 284;
        case KEY_F4: return 285;
        case KEY_HOME: return 268;
        case KEY_PAGEUP: return 266;
        case KEY_PAGEDOWN: return 267;
        case KEY_BACKSPACE: return 8;
        default: return 0;
    }
}

fbdev_input::fbdev_input() {
    open_devices();
}

fbdev_input::~fbdev_input() {
    for (int fd : event_fds) {
        close(fd);
    }
    event_fds.clear();
}

int fbdev_input::open_devices() {
    DIR *dir = opendir("/dev/input");
    if (!dir) {
        fprintf(stderr, "fbdev_input: Failed to open /dev/input\n");
        return -1;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;
        
        std::string path = "/dev/input/";
        path += entry->d_name;
        
        int fd = open(path.c_str(), O_RDWR | O_NONBLOCK);
        if (fd < 0) continue;
        
        char name[256] = {};
        if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
            close(fd);
            continue;
        }
        
        LOG(INFO, "fbdev_input: Found device: %s (%s)", entry->d_name, name);
        event_fds.push_back(fd);
        
        if (fd > max_fd) max_fd = fd;
    }
    
    closedir(dir);
    
    if (event_fds.empty()) {
        fprintf(stderr, "fbdev_input: No input devices found\n");
    }
    
    return event_fds.size();
}

void fbdev_input::poll_events() {
    if (event_fds.empty()) return;
    
    fd_set fds;
    FD_ZERO(&fds);
    for (int fd : event_fds) {
        FD_SET(fd, &fds);
    }
    
    struct timeval tv = {0, 0};
    int ret = select(max_fd + 1, &fds, nullptr, nullptr, &tv);
    if (ret <= 0) return;
    
    struct input_event ev;
    ssize_t bytes;
    for (int fd : event_fds) {
        if (!FD_ISSET(fd, &fds)) continue;
        
        while ((bytes = read(fd, &ev, sizeof(ev))) >= (ssize_t)sizeof(ev)) {
            if (ev.type == EV_KEY) {
                uint16_t keycode = ev.code;
                uint16_t sdlk = keycode_to_sdlk(keycode);
                if (sdlk != 0) {
                    bool pressed = ev.value != 0;
                    on_km_input(sdlk, pressed);
                }
            }
        }
    }
}

int fbdev_input::read_event(struct input_event *ev) {
    if (event_fds.empty()) return -1;
    
    for (int fd : event_fds) {
        ssize_t bytes = read(fd, ev, sizeof(*ev));
        if (bytes >= (ssize_t)sizeof(*ev)) {
            return static_cast<int>(bytes);
        }
    }
    return -1;
}

void fbdev_input::post_init() {
    input_base::post_init();
    
    keymap = {
        107,   // RETRO_DEVICE_ID_JOYPAD_B (KEY_K)
        106,   // RETRO_DEVICE_ID_JOYPAD_Y (KEY_J)
        282,   // RETRO_DEVICE_ID_JOYPAD_SELECT (KEY_F1)
        118,   // RETRO_DEVICE_ID_JOYPAD_START (KEY_V)
        119,   // RETRO_DEVICE_ID_JOYPAD_UP (KEY_W)
        115,   // RETRO_DEVICE_ID_JOYPAD_DOWN (KEY_S)
        97,    // RETRO_DEVICE_ID_JOYPAD_LEFT (KEY_A)
        100,   // RETRO_DEVICE_ID_JOYPAD_RIGHT (KEY_D)
        108,   // RETRO_DEVICE_ID_JOYPAD_A (KEY_L)
        105,   // RETRO_DEVICE_ID_JOYPAD_X (KEY_I)
        113,   // RETRO_DEVICE_ID_JOYPAD_L (KEY_Q)
        101,   // RETRO_DEVICE_ID_JOYPAD_R (KEY_E)
        49,    // RETRO_DEVICE_ID_JOYPAD_L2 (KEY_1)
        51,    // RETRO_DEVICE_ID_JOYPAD_R2 (KEY_3)
        120,   // RETRO_DEVICE_ID_JOYPAD_L3 (KEY_X)
        121,   // RETRO_DEVICE_ID_JOYPAD_R3 (KEY_Y)
    };
    
    for (size_t i = 0; i < keymap.size(); ++i) {
        if (km_to_game_mapping.find(keymap[i]) == km_to_game_mapping.end()) {
            set_km_mapping(keymap[i], i);
        }
    }
    assign_port(0, 0);
}

void fbdev_input::input_poll() {
    poll_events();
}

void fbdev_input::port_connected(int index) {
}

void fbdev_input::port_disconnected(int device_id) {
}

void fbdev_input::get_input_name(uint64_t input, std::string &device_name, std::string &name) const {
}

uint64_t fbdev_input::get_input_from_name(const std::string &device_name, const std::string &name) const {
    return 0;
}

}

#pragma once

#include "input_base.h"

#include <linux/input.h>

#include <cstdint>
#include <array>
#include <vector>
#include <string>

namespace drivers {

class fbdev_input: public input_base {
public:
    fbdev_input();
    ~fbdev_input() override;

    void post_init() override;
    void input_poll() override;
    void port_connected(int index) override;
    void port_disconnected(int device_id) override;
    void get_input_name(uint64_t input, std::string &device_name, std::string &name) const override;
    uint64_t get_input_from_name(const std::string &device_name, const std::string &name) const override;

    int read_event(struct input_event *ev);
    void poll_events();
    const std::vector<int>& get_event_fds() const { return event_fds; }
    const std::vector<struct input_event>& get_stored_events() const { return stored_events; }
    void clear_stored_events() { stored_events.clear(); }
    static uint16_t keycode_to_sdlk(uint16_t keycode);

private:
    int open_devices();
    void process_key(uint16_t keycode, bool pressed);

    std::vector<int> event_fds;
    int max_fd = -1;
    std::array<uint16_t, 16> keymap = {};
    bool menu_button_pressed = false;
    std::vector<struct input_event> stored_events;
};

}

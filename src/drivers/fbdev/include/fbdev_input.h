#pragma once

#include "input_base.h"

#include <cstdint>
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

private:
    int open_devices();
    void process_key(uint16_t keycode, bool pressed);

    std::vector<int> event_fds;
    int max_fd = -1;
};

}

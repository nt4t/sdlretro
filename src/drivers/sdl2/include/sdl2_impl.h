#pragma once

#include "driver_base.h"

namespace drivers {

class sdl2_impl: public driver_base {
public:
    sdl2_impl();

    ~sdl2_impl() override;

    bool process_events() final;
    bool get_menu_button_pressed() const final { return menu_button_pressed; }

protected:
    bool init() final;
    void deinit() final;
    void unload() final;
};

}

#pragma once

#include "driver_base.h"

namespace drivers {

class fbdev_impl: public driver_base {
public:
    fbdev_impl();

    ~fbdev_impl() override;

    bool process_events() final;
    bool get_menu_button_pressed() const final { return menu_button_pressed; }

protected:
    bool init() final;
    void deinit() final;
    void unload() final;
};

}

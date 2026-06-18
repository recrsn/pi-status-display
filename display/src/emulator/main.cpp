#include "ui.h"
#include "hal.h"

int main(void) {
    hal_init(ui_on_data);
    ui_init();

    for (;;) {
        ui_tick();
    }
    return 0;
}

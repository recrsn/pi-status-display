#include "ui.h"
#include "hal.h"

extern "C" void app_main(void) {
    hal_init(ui_on_data);
    ui_init();

    for (;;) {
        ui_tick();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

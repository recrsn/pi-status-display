#include "ui.hpp"
#include "hal.h"

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void) {
    hal_init(ui_on_data);
    ui_init();

    for (;;) {
        ui_tick();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
#else
int main(void) {
    hal_init(ui_on_data);
    ui_init();

    for (;;) {
        ui_tick();
    }
    return 0;
}
#endif

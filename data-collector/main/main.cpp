#include "esp_log.h"
#include "wlmon.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PRINT_STATUS_DELAY_MS 2500

const char *TAG = "wlmon";

// TODO static or not?
static WLmon_Flash *wl_instance;

extern "C"
{


static void print_status_task(void *arg)
{
    for (;;) {
        print_wl_status_json(wl_instance);
        vTaskDelay(PRINT_STATUS_DELAY_MS / portTICK_PERIOD_MS);
    }
}

#if CONFIG_IDF_TARGET_LINUX
void app_main(int argc, char **argv)
#else
void app_main(void)
#endif
{
#if CONFIG_IDF_TARGET_LINUX
    // TODO on linux get partition from file with partition image
    const esp_partition_t *partition = get_wl_partition(argv[1]);
#else
    const esp_partition_t *partition = get_wl_partition(NULL);
#endif

    if (!partition) {
        ESP_LOGE(TAG, "Failed to get WL partition for analysis!");
        return;
    }

    // handle for potential vTaskDelete() on status received ACK from PC-side
    TaskHandle_t print_task_handle = NULL;

    wl_instance = wl_attach(partition);
    if (wl_instance != NULL) {
        xTaskCreate(print_status_task, "print_status_task", CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH, NULL, CONFIG_FREERTOS_TIMER_TASK_PRIORITY, &print_task_handle);
    } else {
        ESP_LOGE(TAG, "Failed to attach to WL in '%s' partition", partition->label);
    }
} // main()

} // extern "C"
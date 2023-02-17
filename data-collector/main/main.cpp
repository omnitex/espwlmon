#include "esp_log.h"
#include "wlmon.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PRINT_STATUS_DELAY_MS 2500

const char *TAG = "wlmon";

static esp_err_t result = ESP_OK;
static const esp_partition_t *partition = NULL;
static TaskHandle_t task_handle = NULL;

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

static void print_error_task(void *arg)
{
    esp_err_t result = *(esp_err_t*)arg;

    for (;;) {
        print_error_json(result);
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
    result = get_wl_partition(argv[1], &partition);
#else
    result = get_wl_partition(NULL, &partition);
#endif

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get WL partition for analysis!");
        result = ESP_ERR_NOT_FOUND;
        goto print_error;
    }

    result = wl_attach(partition, &wl_instance);
    if (result == ESP_OK) {
        xTaskCreate(print_status_task, "print_status_task", CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH, NULL, CONFIG_FREERTOS_TIMER_TASK_PRIORITY, &task_handle);
    } else {
        ESP_LOGE(TAG, "Failed to attach to WL in '%s' partition", partition->label);
print_error:
        xTaskCreate(print_error_task, "print_error_task", CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH, &result, CONFIG_FREERTOS_TIMER_TASK_PRIORITY, &task_handle);
    }
} // main()

} // extern "C"
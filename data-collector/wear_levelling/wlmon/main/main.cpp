#include "esp_log.h"
#include "wlmon.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PRINT_BUFFER_DELAY_MS 2500

const char *TAG = "wlmon";

static TaskHandle_t task_handle = NULL;

extern "C"
{

static void print_buffer_task(void *arg)
{
    char *buffer = (char *) arg;
    for (;;) {
        printf("%s", buffer);
        fflush(stdout);
        vTaskDelay(PRINT_BUFFER_DELAY_MS / portTICK_PERIOD_MS);
    }
}

static const char *malloc_error = "{\"error\":\"ESP_ERR_NO_MEM\"}\n";

void app_main()
{
    char *buffer;
    esp_err_t result;

    result = wlmon_get_status(&buffer);
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "wlmon_get_status() OK");
    } else {
        ESP_LOGE(TAG, "wlmon_get_status(): %s", esp_err_to_name(result));
        if (result == ESP_ERR_NO_MEM)
            buffer = (char *) malloc_error;
    }

    xTaskCreate(print_buffer_task, "print_buffer_task", CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH, buffer, CONFIG_FREERTOS_TIMER_TASK_PRIORITY, &task_handle);

} // main()

} // extern "C"
#include "esp_log.h"
#include "wlmon.h"

const char *TAG = "wlmon";

extern "C"
{

#if CONFIG_IDF_TARGET_LINUX
void app_main(int argc, char **argv)
#else
void app_main(void)
#endif
{
#if CONFIG_IDF_TARGET_LINUX
    const esp_partition_t *partition = get_wl_partition(argv[1]);
#else
    const esp_partition_t *partition = get_wl_partition(NULL);
#endif

    if (partition)
        print_config_json(partition);
    else
        ESP_LOGE(TAG, "Failed to get WL partition for analysis!");
}

}
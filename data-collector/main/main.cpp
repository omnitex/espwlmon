#include "esp_log.h"
#include "wlmon.h"

const char *TAG = "wlmon";

// use this for something?
static WLmon_Flash *wl_instance;

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

    if (!partition) {
        ESP_LOGE(TAG, "Failed to get WL partition for analysis!");
        return;
    }

    wl_instance = wl_attach(partition);
    if (wl_instance != NULL) {
        print_config_json(wl_instance);
        print_state_json(wl_instance);
    } else {
        ESP_LOGE(TAG, "Failed to attach to WL in '%s' partition", partition->label);
    }
} // main()

} // extern "C"
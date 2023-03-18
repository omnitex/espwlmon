#include <cstdlib>
#include <time.h>

#include "esp_log.h"
#include "wl_sim.h"
#include "random.h"
#include "WL_Flash.h"

static const char *TAG = "wl-sim";

extern size_t restarted;
extern size_t access_count;

//TODO parameters like this or arguments? or file?

// {constant,uniform}
#define ADDRESS_FUNCTION constant

#define ITERATIONS 1000000

#define RESTART_PROBABILITY 1 // restart after each erase, in percent
#define ERASE_SIZE (0x100)

#define USED_DISTRIBUTION "undetermined erase distribution"
#if ADDRESS_FUNCTION == constant
#undef USED_DISTRIBUTION
#define USED_DISTRIBUTION "constant erase distribution"
#endif
#if ADDRESS_FUNCTION == uniform
#undef USED_DISTRIBUTION
#define USED_DISTRIBUTION "uniform erase distribution"
#endif

int main()
{
    srand(time(0));

    ESP_LOGI(TAG, "===== SETUP =====");
    ESP_LOGI(TAG, "iterations: %u", ITERATIONS);
    ESP_LOGI(TAG, USED_DISTRIBUTION);


    for (size_t i = 0; i < ITERATIONS; i++) {

        erase_range(ADDRESS_FUNCTION(FLASH_SIZE), ERASE_SIZE);

#if 0
        // simulated device hard restart, current access count is lost without a pos update record
        int restart_prob = rand() % 1000;
        if (restart_prob < RESTART_PROBABILITY) {
            access_count = 0;
            restarted++;
        }
#endif

    }

    print_erase_counts();
    print_vars();
    print_reconstructed();

    return 0;
}


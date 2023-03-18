#include <cstdlib>
#include <time.h>

#include "esp_log.h"
#include "wl_sim.h"
#include "WL_Flash.h"

static const char *TAG = "wl-sim";

//TODO argparse

#define RANDOM false
#define ITERATIONS 100000
#define RESTART_PROBABILITY 1 // restart after each erase, in percent
#define ERASE_ADDRESS (FLASH_SIZE/3)
#define ERASE_SIZE (0x100)

int main()
{
    srand(time(0));

    ESP_LOGI(TAG, "===== SETUP =====");
    ESP_LOGI(TAG, "iterations: %u", ITERATIONS);
#if RANDOM
    ESP_LOGI(TAG, "erase_address RANDOMIZED");
#else
    ESP_LOGI(TAG, "erase_address: 0x%x", ERASE_ADDRESS);
#endif

    for (size_t i = 0; i < ITERATIONS; i++) {
#if !RANDOM
        erase_range(ERASE_ADDRESS, ERASE_SIZE);
#else
        int address = rand() % ERASE_ADDRESS;
        int size = rand() % ERASE_SIZE + 1;
        erase_range(address, size);

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


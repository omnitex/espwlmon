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

// {constant,uniform,zipf}
#define ADDRESS_FUNCTION zipf
// {0,1} if addr supplied to ADDRESS_FUNCTION should be zero
#define ZERO_ADDR 0
// {0,1} enable feistel network address randomization
#define FEISTEL 1
// number of iterations of main erase loop. BEWARE OF VERBOSE LOGGING
#define ITERATIONS 25000000
// {0,1} enable per sector verbose erase count logs
#define VERBOSE_ERASE_COUNTS 0
// block size of consecutive sectors erased in main loop
#define ERASE_BLOCK 16

#define RESTART_PROBABILITY 1 // restart after each erase, in per mille
#define ERASE_SIZE (0x100)

int main()
{
    srand(time(0));

#if FEISTEL == 1
    init_feistel();
#endif

    ESP_LOGI(TAG, "===== SETUP =====");
    ESP_LOGI(TAG, "iterations: %u", ITERATIONS);
    ESP_LOGI(TAG, "erase block: %u", ERASE_BLOCK);

    for (size_t i = 0; i < ITERATIONS; i++) {
#if ERASE_BLOCK == 1
        erase_range(ADDRESS_FUNCTION(FLASH_SIZE * !ZERO_ADDR), ERASE_SIZE);
#else
        erase_range(ADDRESS_FUNCTION(FLASH_SIZE * !ZERO_ADDR) + (i % ERASE_BLOCK) * SECTOR_SIZE, ERASE_SIZE);
#endif

#if 0
        // simulated device hard restart, current access count is lost without a pos update record
        int restart_prob = rand() % 1000;
        if (restart_prob < RESTART_PROBABILITY) {
            access_count = 0;
            restarted++;
        }
#endif

    }

    print_erase_counts(VERBOSE_ERASE_COUNTS);
    print_vars();
    print_reconstructed();

    return 0;
}


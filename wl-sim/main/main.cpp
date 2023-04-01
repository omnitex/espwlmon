#include <cstdlib>
#include <time.h>

#include "esp_log.h"
#include "wl_sim.h"
#include "random.h"
#include "WL_Flash.h"

static const char *TAG = "wl-sim";

extern size_t restarted;
extern size_t access_count;
extern size_t move_count;
extern uint32_t feistel_cycle_walks;

//TODO parameters like this or arguments? or file?

// {constant,uniform,zipf,linear}
#define ADDRESS_FUNCTION zipf
// {0,1} if addr supplied to ADDRESS_FUNCTION should be zero
#define ZERO_ADDR 0
// {0,1} enable feistel network address randomization
#define FEISTEL 1
// number of iterations of main erase loop. BEWARE OF VERBOSE LOGGING
#define ITERATIONS 25000000
// {0,1} enable per sector verbose erase count logs
#define VERBOSE_ERASE_COUNTS 0
// block size of consecutive sectors erased per iteration
#define ERASE_BLOCK 1

// restart after each erase range, in per mille
// 0 disables random restarting
#define RESTART_PROBABILITY 0
#define ERASE_SIZE (0x1000)

int main()
{
    srand(time(0));
    esp_err_t result;

    ESP_LOGI(TAG, "===== SETUP =====");
    ESP_LOGI(TAG, "sector_size=0x%x (%u)", SECTOR_SIZE, SECTOR_SIZE);
    ESP_LOGI(TAG, "sector_count=0x%x (%u)", SECTOR_COUNT, SECTOR_COUNT);

#if FEISTEL == 1
    init_feistel();
#endif

// test that feistel indeed maps 1:1, that no two sectors map to the same one
#if 0
    init_feistel();

    uint8_t occurences[SECTOR_COUNT] = {0};

    ESP_LOGI(TAG, "FLASH_SIZE=0x%x", FLASH_SIZE);

    size_t addr, sector_addr;

    auto nonzeros = 0;
    for (auto i = 0; i < SECTOR_COUNT; i++) {
        addr = feistel_network(i*SECTOR_SIZE);

        //addr = (FLASH_SIZE - move_count * PAGE_SIZE + addr) % FLASH_SIZE;
        sector_addr = addr/SECTOR_SIZE;

        //ESP_LOGI(TAG, "feistel(0x%x)=>0x%x", i, sector_addr);
        if (sector_addr >= SECTOR_COUNT) {
            ESP_LOGE(TAG, "sector_addr=%u", sector_addr);
        }

        occurences[sector_addr]++;
        nonzeros++;
    }
    ESP_LOGI(TAG, "after feistel: sector_count=%u, nonzeros=%u", SECTOR_COUNT, nonzeros);
    nonzeros = 0;

    for (auto i = 0; i < SECTOR_COUNT; i++) {
        if (occurences[i] != 0) {
            nonzeros++;
            if (occurences[i] > 1) {
                ESP_LOGE(TAG, "sector 0x%x occured %u times", i, occurences[i]);
            } else {
                //ESP_LOGI(TAG, "sector 0x%x occured %u times", i, occurences[i]);
            }
        } else if (occurences[i] == 0) {
            ESP_LOGE(TAG, "sector 0x%x occured %u times", i, occurences[i]);
        } else {
            ESP_LOGE(TAG, "sector 0x%x occured %u times", i, occurences[i]);
        }
    }
    ESP_LOGI(TAG, "after occurences: sector_count=%u, nonzeros=%u", SECTOR_COUNT, nonzeros);
    ESP_LOGI(TAG, "cycle_walks=%u", feistel_cycle_walks);

    return 0;
#endif

    ESP_LOGI(TAG, "iterations: %u", ITERATIONS);
    ESP_LOGI(TAG, "erase block: %u", ERASE_BLOCK);

    for (size_t i = 0; i < ITERATIONS; i++) {

        result = erase_range(ADDRESS_FUNCTION(FLASH_SIZE * !ZERO_ADDR), ERASE_SIZE * ERASE_BLOCK);

        if (result != ESP_OK)
            break;

#if RESTART_PROBABILITY != 0
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


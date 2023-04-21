#include <cstdlib>
#include <time.h>
#include <cstring>

#include "esp_log.h"
#include "wl_sim_random.h"
#include "wl_sim.h"
#include "WLsim_Flash.h"

static const char *TAG = "wl-sim";

extern uint32_t restarted;
extern size_t access_count;

typedef size_t (*address_function_t)(size_t);
typedef size_t (*block_size_function_t)(size_t);

// forward declaration
int feistel_test();

int main(int argc, char **argv)
{
    srand(time(0));
    esp_err_t result;

    // if single argument 'test', run the mapping correctness test
    if (argc == 2 && strcmp(argv[1], "test") == 0) {
        feistel_test();
        return 0;
    }

    // otherwise require all args for a simulation run
    // e.g. wl-sim.elf f z z 10 0
    // for Feistel enabled, zipf address access and zipf block size with maximum of 10 and 0 per mille chance for restart
    if (argc != 6) {
        printf("Need simulation params as arguments:\n\
\tENABLE_FEISTEL: f for Feistel, b for base mapping alg\n\
\tADDRESS_FUNC: z for zipf, c for const\n\
\tBLOCKS_SIZE_FUNC: z for zipf, c for const\n\
\tBLOCK_SIZE: N for max erase block size\n\
\tRESTART_PROB: P for restart probability after every erase [per mille]\n");
        return ESP_FAIL;
    }

    // quick and dirty arg parsing
    bool enable_feistel = argv[1][0] == 'f' ? true : false;
    address_function_t addr_func = argv[2][0] == 'z' ? &zipf : &constant;
    block_size_function_t block_func = argv[3][0] == 'z' ? &block_zipf : &block_constant;
    size_t erase_block_size = strtoul(argv[4], NULL, 10);
    int restart_prob = strtoul(argv[5], NULL, 10);

    //ESP_LOGI(TAG, "feistel: %u, addr_func: %p, erase_block_size: %u", enable_feistel, addr_func, erase_block_size);

    // if Feistel enabled from args, init keys and variables
    if (enable_feistel) {
        init_feistel(false);
    }

    // runs until any sector reaches erase lifetime, see erase_sector()
    for (;;) {
        result = erase_range(addr_func(FLASH_SIZE), ERASE_SIZE * block_func(erase_block_size));

        // non OK result returned only on sector reaching lifetime meaning we should end simulation run
        if (result != ESP_OK) {
            break;
        }

        // otherwise continue

        // if nonzero restart probability from arguments
        if (restart_prob != 0) {
            // generate number P in per mille to compare with given restart prob
            int P = rand() % 1000;
            if (P < restart_prob) {
                // and simulate a restart, loosing current value of access_count
                access_count = 0;
                restarted++;
            }
        }
    }

    // after simulation run complete, print output statistics
    print_output();

    return 0;
}

// test that feistel indeed maps 1:1, that no two sectors map to the same one
int feistel_test()
{
    // generate keys etc.
    init_feistel(true);
    ESP_LOGI(TAG, "Feistel initialized");

    // per sector tracker of how many times given sector was the output of Feistel network
    uint8_t occurences[SECTOR_COUNT] = {0};

    uint32_t nonzeros = 0;
    size_t addr, sector_addr;

    // go through all sectors
    for (uint32_t i = 0; i < SECTOR_COUNT; i++) {
        // get the randomized mapping for sector given by i
        addr = feistel_network(i * SECTOR_SIZE);

        // sector_addr ~ sector index (0, 1, 2 ~ SECTOR_COUNT-1)
        sector_addr = addr / SECTOR_SIZE;

        // if Feistel mapped given sector outside of possible indices, report error
        if (sector_addr >= SECTOR_COUNT) {
            ESP_LOGE(TAG, "sector_addr=%u", sector_addr);
        }

        // now use the fact that sector_addr ~ index
        // and mark that given sector was the output of Feistel
        occurences[sector_addr]++;
        nonzeros++;
    }
    // sector_count should equal nonzeros
    ESP_LOGI(TAG, "after feistel: sector_count=%u, nonzeros=%u", SECTOR_COUNT, nonzeros);
    nonzeros = 0;

    // now do second go through all sector and this time check that from previous loop
    // each sector was the output of Feistel exactly once => 1-to-1 mapping
    for (uint32_t i = 0; i < SECTOR_COUNT; i++) {
        // sector_addr ~ i
        if (occurences[i] != 0) {
            nonzeros++;
            // if given sector occurred multiple times, that is an error as mapping should be 1:1
            if (occurences[i] > 1) {
                ESP_LOGE(TAG, "sector 0x%x occured %u times", i, occurences[i]);
            } else {
                // this is the correct state, nonzero occurrence but not greater than 1
                //ESP_LOGI(TAG, "sector 0x%x occured %u times", i, occurences[i]);
            }
            // any other state is also erroneous
        } else if (occurences[i] == 0) {
            ESP_LOGE(TAG, "sector 0x%x occured %u times", i, occurences[i]);
        } else {
            ESP_LOGE(TAG, "sector 0x%x occured %u times", i, occurences[i]);
        }
    }
    // once again sector_count should equal nonzeros
    ESP_LOGI(TAG, "after occurences: sector_count=%u, nonzeros=%u", SECTOR_COUNT, nonzeros);
    print_vars();

    return 0;
}


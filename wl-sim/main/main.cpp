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
    // e.g. wl-sim f z z 10 0
    // for Feistel enabled, zipf address access and zipf block size with maximum of 10 and 0 per mille chance for restart
    if (argc != 6) {
        printf("Need simulation params as arguments:\n\
\tMAPPING_ALG: f for Feistel, b for base mapping alg\n\
\tADDRESS_FUNC: z for zipf, c for const\n\
\tBLOCKS_SIZE_FUNC: z for zipf, c for const\n\
\tBLOCK_SIZE: N for max erase block size\n\
\tRESTART_PROB: P for restart probability after every erase [per mille]\n");
        return ESP_FAIL;
    }

    // argument parsing

    bool enable_feistel = false;
    if (strcmp(argv[1], "f") == 0) {
        enable_feistel = true;
    } else if (strcmp(argv[1], "b") == 0) {
        // enable_feistel already false
    } else {
        fprintf(stderr, "First argument '%s' invalid, defaulting to base mapping...\n", argv[1]);
        // enable_feistel already false
    }

    address_function_t addr_func = &constant;
    if (strcmp(argv[2], "z") == 0) {
        addr_func = &zipf;
    } else if (strcmp(argv[2], "c") == 0) {
        // addr_func already constant
    } else {
        fprintf(stderr, "Second argument '%s' invalid, defaulting to constant address...\n", argv[2]);
        // addr_func already constant
    }

    block_size_function_t block_func = &block_constant;
    if (strcmp(argv[3], "z") == 0) {
        block_func = &block_zipf;
    } else if (strcmp(argv[3], "c") == 0) {
        // block_func already constant
    } else {
        fprintf(stderr, "Third argument '%s' invalid, defaulting to constant erase block size...\n", argv[3]);
        // block_func already constant
    }

    // last two arguments are numbers
    char *end = NULL;
    int erase_block_size = strtol(argv[4], &end, 10);
    if (*end != '\0') {
        fprintf(stderr, "Invalid erase block size '%s'!\n", argv[4]);
        return ESP_FAIL;
    } else if (erase_block_size <= 0) {
        fprintf(stderr, "Invalid erase block size %i, must be > 0\n", erase_block_size);
        return ESP_FAIL;
    }

    int restart_prob = strtol(argv[5], &end, 10);
    if (*end != '\0') {
        fprintf(stderr, "Invalid restart probability %s'!\n", argv[5]);
        return ESP_FAIL;
    } else if (restart_prob < 0) {
        fprintf(stderr, "Invalid restart probability %i, must be > 0 or 0 to turn off random restarting\n", restart_prob);
        return ESP_FAIL;
    }

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

    // now do second go through all sectors and this time check that from previous loop
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


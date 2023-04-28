#include <cmath>
#include "esp_log.h"
#include "esp_err.h"

#include "wl_sim.h"

#define SECTOR_ERASE_ENDURANCE 100000

static const char *TAG = "WLsim_Flash";

/*
 * BEWARE THIS FILE CONTAINS A SIMPLIFIED COPY OF WL_Advanced FUNCTIONALITY FOR SIMULATION PURPOSES ONLY
 * DO NOT REFER TO THIS IMPLEMENTATION FOR UNDERSTANDING FEISTEL NETWORK ADDRESS RANDOMIZATION OR ANYTHING OTHER THAN SIMULATION
 * FOR THIS REASON COMMENTS ARE SPARSE AND VARIABLES ARE SOMEWHAT A MESS
 */

// exported for zeroing in main on simulated restart
size_t access_count = 0;
// exported to main for incrementing on simulated restart
uint32_t restarted = 0;

// main mapping counters
static size_t pos = 0;
static size_t move_count = 0;
static uint32_t cycle_count = 0;

// array for keeping per sector erase counts
// + 1 to also include dummy sector, which is reserved in FLASH_SIZE calculation
// so there are SECTOR_COUNT usable and addressable sectors, but for calculating statistics
// we need to make space for additional dummy sector which can also be the result of mapping
static uint32_t erase_counts[SECTOR_COUNT + 1] = {0};

// 3 keys for 3 stage unbalanced Feistel network
static uint8_t keys[3] = {0};
// bit lengths of full sector address (B) and lengths of two parts for splitting in Feistel network (MSB, LSB)
static uint8_t B = 0, MSB = 0, LSB = 0;
// counters for debug and simulation output purposes
static uint32_t feistel_calls = 0;
static uint32_t feistel_cycle_walks;
// was Feistel initialized and should be used in calcAddr?
static bool feistel = false;

void init_feistel(bool verbose)
{
    feistel = true;

    uint16_t sector_count = SECTOR_COUNT;
    if (verbose) {
        ESP_LOGI(TAG, "%s: SECTOR_COUNT=%u", __func__, sector_count);
        ESP_LOGI(TAG, "sizeof(size_t)=%u", sizeof(size_t));
    }

    // B = log2(sector_count)
    for (B = 0; sector_count; B++) {
        sector_count >>= 1;
    }

    LSB = (B + 1) / 2;
    MSB = B - LSB;
    if (verbose) {
        ESP_LOGI(TAG, "%s: B=%u, MSB=%u, LSB=%u", __func__, B, MSB, LSB);
    }

    uint8_t key_len = (B + 1) / 2;

    if (verbose) {
        ESP_LOGI(TAG, "%s: key_len=%u", __func__, key_len);
    }

    for (uint8_t i = 0; i < 3; i++) {
        keys[i] = rand() % UINT8_MAX;
    }

    if (verbose) {
        ESP_LOGI(TAG, "%s: generated 8bit keys (%u, %u, %u) ", __func__, keys[0], keys[1], keys[2]);
    }
}

static uint32_t feistel_function(uint32_t msb, uint32_t key)
{
    //ESP_LOGV(TAG, "%s: msb=0x%x key=0x%x", __func__, msb, key);
    //ESP_LOGV(TAG, "%s: msb xor key = 0x%x", __func__, (msb ^ key));
    ESP_LOGV(TAG, "%s: return (msb xor key)^2 = 0x%x", __func__, (msb ^ key) * (msb ^ key));
    return (msb ^ key) * (msb ^ key);
}

size_t feistel_network(size_t logical_addr)
{
    feistel_calls++;
    size_t addr = logical_addr;

round:
    size_t sector_addr = addr / SECTOR_SIZE;

    //               |       B       |
    //               |<-MSB->|<-LSB->|
    // logical_addr: |  msb  |  lsb  |

    size_t LSB_mask = ~( (~(size_t)0) << LSB );

    size_t msb, lsb, _msb, _lsb, randomized_addr;

    for (uint8_t i = 0; i < 3; i++) {
        msb = sector_addr >> LSB;
        lsb = sector_addr & LSB_mask;

        _msb = msb;
        // mask output of F to also be |LSB| for XORing with lsb
        _lsb = (lsb ^ (feistel_function(msb, keys[i]) & LSB_mask));

        // swap lsb and msb
        sector_addr = (_lsb << MSB) | _msb;
        ESP_LOGV(TAG, "%s: msb=0x%x lsb=0x%x sector_addr=0x%x", __func__, _msb, _lsb, sector_addr);
    }
    randomized_addr = sector_addr;
    ESP_LOGD(TAG, "%s: randomized_addr=0x%x", __func__, randomized_addr);

    if (randomized_addr >= SECTOR_COUNT) {
        addr = randomized_addr * SECTOR_SIZE;
        feistel_cycle_walks++;
        goto round;
    }

    return randomized_addr * SECTOR_SIZE;
}

size_t calcAddr(size_t addr)
{
    size_t intermediate_addr = addr;
    if (feistel) {
        intermediate_addr = feistel_network(addr);
    }

    size_t result = (FLASH_SIZE - move_count * PAGE_SIZE + intermediate_addr) % FLASH_SIZE;
    size_t dummy_addr = pos * PAGE_SIZE;

    if (result < dummy_addr) {
    } else {
        result += PAGE_SIZE;
    }

    ESP_LOGV(TAG, "%s - addr= 0x%08x -> result= 0x%08x, dummy_addr= 0x%08x", __func__, (uint32_t) addr, (uint32_t) result, (uint32_t)dummy_addr);
    return result;
}

esp_err_t updateWL()
{
    esp_err_t result = ESP_OK;

    access_count++;
    if (access_count < MAX_COUNT) {
        ESP_LOGV(TAG, "%s EARLY RETURN - access_count= 0x%08x, pos= 0x%08x, move_count= 0x%08x", __func__, (uint32_t) access_count, (uint32_t) pos, (uint32_t) move_count);
        return result;
    }

    access_count = 0;

    pos++;
    if (pos >= MAX_POS) {
        pos = 0;
        // one loop more
        move_count++;
        if (move_count >= (MAX_POS - 1)) {
            move_count = 0;
            cycle_count++;
        }
    }

    ESP_LOGV(TAG, "%s - access_count= 0x%08x, pos= 0x%08x, move_count= 0x%08x", __func__, (uint32_t) access_count, (uint32_t) pos, (uint32_t) move_count);
    return result;
}

esp_err_t erase_sector(size_t sector)
{
    updateWL();
    size_t virt_addr = calcAddr(sector * SECTOR_SIZE);
    size_t phy_sector = virt_addr / SECTOR_SIZE;

    ESP_LOGV(TAG, "%s - virt_addr= 0x%08x, phy_sector= 0x%08x", __func__, (uint32_t) virt_addr, (uint32_t) phy_sector);

    // possible physical sector locations are SECTOR_COUNT+1 due to dummy sector
    erase_counts[phy_sector]++;

    // reached maximum lifetime of a sector
    // stop erasing and propagate to calculating normalized endurance (NE)
    if (erase_counts[phy_sector] >= SECTOR_ERASE_ENDURANCE) {
        //ESP_LOGW(TAG, "%s: sector %u reached %u", __func__, phy_sector, erase_counts[phy_sector]);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t erase_range(size_t start_address, size_t size)
{
    esp_err_t result = ESP_OK;


    size_t erase_count = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    size_t start_sector = start_address / SECTOR_SIZE;

    ESP_LOGV(TAG, "%s - start_address= 0x%08x, size= 0x%08x, erase_count= 0x%08x, start_sector= 0x%08x",
             __func__, (uint32_t) start_address, (uint32_t) size, (uint32_t) erase_count, (uint32_t) start_sector);

    for (size_t i = 0; i < erase_count; i++) {
        result = erase_sector(start_sector + i);
        if (result != ESP_OK) {
            // will propagate fail return code
            break;
        }
    }

    return result;
}

void print_vars()
{
    ESP_LOGD(TAG, "===== VARS =====");
    ESP_LOGD(TAG, "access_count = %lu", access_count);
    ESP_LOGD(TAG, "pos = %lu", pos);
    ESP_LOGD(TAG, "move_count = %lu", move_count);
    ESP_LOGD(TAG, "cycle_count = %u", cycle_count);
    if (feistel) {
        ESP_LOGI(TAG, "feistel_calls = %lu", feistel_calls);
        ESP_LOGI(TAG, "feistel cycle walks = %u", feistel_cycle_walks);
    }
}

void print_output()
{
    uint64_t sum = 0;
    uint32_t min = UINT32_MAX, max = 0, nonzeros = 0;

    // + 1 to include dummy sector as it can also be the result of mapping
    for (size_t i = 0; i < SECTOR_COUNT + 1; i++) {
        uint32_t count = erase_counts[i];
        if (count != 0) {
            sum += count;
            nonzeros++;

            if (count < min) {
                min = count;
            }
            if (count > max) {
                max = count;
            }
        }
    }
    // normalized endurance [%]
    //       Total Writes Before System Failure
    // NE = ------------------------------------ x 100%
    //               Wmax x Num Sectors
    // and +1 for dummy sector here as well
    double NE = ((double)sum / (double)(SECTOR_ERASE_ENDURANCE * (SECTOR_COUNT + 1)) * 100);

    printf("NE %f cycle_walks %u restarted %u\n", NE, feistel_cycle_walks, restarted);
}

void print_reconstructed()
{
    ESP_LOGD(TAG, "===== RECONSTRUCT =====");

    size_t dummy_addr = pos * PAGE_SIZE;
    ESP_LOGD(TAG, "dummy_addr: 0x%lx", dummy_addr);

    size_t resolution = (MAX_POS - 1) * UPDATERATE;
    ESP_LOGD(TAG, "resolution: 0x%lx (%ld)", resolution, resolution);

    uint32_t erase_from_pos = pos * UPDATERATE;
    uint32_t erase_from_mc = move_count * MAX_POS * UPDATERATE + erase_from_pos;
    uint32_t erase_from_cc = cycle_count * MAX_POS * (MAX_POS - 1) * UPDATERATE + erase_from_mc;

    ESP_LOGD(TAG, "erase count from mc&pos: %ld", erase_from_mc);
    ESP_LOGD(TAG, "erase count including cycle_count: %ld", erase_from_cc);
}

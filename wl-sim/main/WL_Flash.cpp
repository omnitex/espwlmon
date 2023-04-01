#include <cmath>
#include "esp_log.h"
#include "esp_err.h"

#include "wl_sim.h"

static const char *TAG = "WL_Flash";

#define SECTOR_ERASE_ENDURANCE 100000

size_t access_count = 0;
size_t pos = 0;
size_t move_count = 0;
uint32_t cycle_count = 0;
uint32_t erase_count[SECTOR_COUNT + 1] = {0};
size_t last_erase_sector = 0;
size_t restarted = 0;

// key_len is half of bit length of address space
// so 8bit key is for (up to) 16bit address space
// 65K sectors is plenty: up to 32MB of 512B sectors
// or 256MB of 4K sectors
uint8_t keys[3] = {0};
uint8_t B = 0, MSB = 0, LSB = 0;
uint32_t feistel_calls = 0;
uint32_t feistel_cycle_walks = 0;
static bool feistel = false;

void init_feistel()
{
    feistel = true;

    uint16_t sector_count = SECTOR_COUNT;
    ESP_LOGI(TAG, "%s: N=%u", __func__, sector_count);
    ESP_LOGI(TAG, "sizeof(size_t)=%u", sizeof(size_t));

    // B = log2(sector_count)
    for (B = 0; sector_count; B++)
        sector_count >>= 1;

    LSB = (B+1)/2;
    MSB = B - LSB;
    ESP_LOGI(TAG, "%s: B=%u, MSB=%u, LSB=%u", __func__, B, MSB, LSB);

    uint8_t key_len = (B + 1)/2;

    ESP_LOGI(TAG, "%s: key_len=%u", __func__, key_len);

    for (auto i = 0; i < 3; i++)
        keys[i] = rand() % UINT8_MAX;

    ESP_LOGI(TAG, "%s: generated 8bit keys (%u, %u, %u) ", __func__, keys[0], keys[1], keys[2]);

}

static uint32_t feistel_function(uint32_t msb, uint32_t key)
{
    ESP_LOGV(TAG, "%s: msb=0x%x key=0x%x", __func__, msb, key);
    ESP_LOGV(TAG, "%s: msb xor key = 0x%x", __func__, (msb ^ key));
    ESP_LOGV(TAG, "%s: return (msb xor key)^2 = 0x%x", __func__, (msb ^ key)*(msb ^ key));
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

    size_t B_mask = ~( (~(size_t)0) << B );

    size_t LSB_mask = ~( (~(size_t)0) << LSB );
    size_t MSB_mask = ( (~(size_t)0) << LSB ) & B_mask;

    size_t msb, lsb, _msb, _lsb, randomized_addr;

    for (auto i = 0; i < 3; i++) {
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
#if 0
        ESP_LOGE(TAG, "%s: logical=0x%x => randomized 0x%x outside of domain 0x%x, cycle walk", __func__, logical_addr, randomized_addr, SECTOR_COUNT);

        if (feistel_cycle_walks > 20) {
            ESP_LOGE(TAG, "%s: feistel cycle walks critical, exiting", __func__);
            exit(1);
        }
#endif
        goto round;
    }

    return randomized_addr * SECTOR_SIZE;
}

size_t calcAddr(size_t addr)
{
    size_t intermediate_addr = addr;
    if (feistel)
        intermediate_addr = feistel_network(addr);

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

    // copy data to dummy block
    // data_addr = ...
    // dummy_addr = ...
    // erase_range(dummy_addr...
    // for (copy count) read(...) write(...)
    // done.. block moved
    // write pos updates...

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
    // write main state...

    ESP_LOGD(TAG, "%s - access_count= 0x%08x, pos= 0x%08x, move_count= 0x%08x", __func__, (uint32_t) access_count, (uint32_t) pos, (uint32_t) move_count); 
    return result;
}

esp_err_t erase_sector(size_t sector)
{
    esp_err_t result = ESP_OK;

    updateWL();
    size_t virt_addr = calcAddr(sector * SECTOR_SIZE);
    size_t phy_sector = virt_addr / SECTOR_SIZE;

    ESP_LOGV(TAG, "%s - virt_addr= 0x%08x, phy_sector= 0x%08x", __func__, (uint32_t) virt_addr, (uint32_t) phy_sector);

    erase_count[phy_sector]++;

    // reached maximum lifetime of a sector
    // stop erasing and propagate to calculating normalized endurance (NE)
    if (erase_count[phy_sector] >= SECTOR_ERASE_ENDURANCE) {
        ESP_LOGI(TAG, "%s: sector %u reached %u", __func__, phy_sector, erase_count[phy_sector]);
        return ESP_FAIL;
    }

    last_erase_sector = phy_sector;

    return result;
}

// forward declaration
void print_erase_counts(bool);

esp_err_t erase_range(size_t start_address, size_t size)
{
    esp_err_t result = ESP_OK;


    size_t erase_count = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    size_t start_sector = start_address / SECTOR_SIZE;

    ESP_LOGD(TAG, "%s - start_address= 0x%08x, size= 0x%08x, erase_count= 0x%08x, start_sector= 0x%08x",
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

void print_erase_counts(bool verbose)
{
    uint64_t sum = 0;
    uint32_t min = UINT32_MAX, max = 0, nonzeros = 0;

    ESP_LOGI(TAG, "===== RESULTS =====");
    for (size_t i = 0; i <= SECTOR_COUNT; i++) {
        uint32_t count = erase_count[i];
        if (count != 0) {
            if (verbose) {
                ESP_LOGI(TAG,"%ld: {%d}", i, count);
            }
            sum += count;
            nonzeros++;

            if (count < min) min = count;
            if (count > max) max = count;
        }
    }
    // normalized endurance [%]
    //       Total Writes Before System Failure
    // NE = ------------------------------------ x 100%
    //               Wmax x Num Sectors
    double NE = ((double)sum / (double)(SECTOR_ERASE_ENDURANCE * SECTOR_COUNT)) * 100;

    auto mean = sum/nonzeros;

    // standard deviation and variance
    sum = 0;
    for (auto i = 0; i <= SECTOR_COUNT; i++) {
        uint32_t count = erase_count[i];
        if (count != 0) {
           sum += (count - mean) * (count - mean);
        }
    }
    // TODO sample v population? nonzeros are just a sample from all, right?
    // if nonzeros == 1 can't do -1 for sample
    double var = sum / (nonzeros);
    double dev = std::sqrt(var);
    ESP_LOGI(TAG, "MIN: %u\tMAX: %u\tMEAN: %lu\tVAR: %f\tDEV: %f", min, max, mean, var, dev);
    ESP_LOGI(TAG, "NE: %f", NE);
}

void print_vars()
{
    ESP_LOGI(TAG, "===== VARS =====");
    ESP_LOGI(TAG, "access_count = %lu", access_count);
    ESP_LOGI(TAG, "pos = %lu", pos);
    ESP_LOGI(TAG, "move_count = %lu", move_count);
    ESP_LOGI(TAG, "cycle_count = %u", cycle_count);
    ESP_LOGI(TAG, "restarted = %lu", restarted);
    if (feistel) {
        ESP_LOGI(TAG, "feistel_calls = %lu", feistel_calls);
        ESP_LOGI(TAG, "feistel cycle walks = %u", feistel_cycle_walks);
    }
}
 
void print_reconstructed()
{
    ESP_LOGI(TAG,"===== RECONSTRUCT =====");

    size_t dummy_addr = pos * PAGE_SIZE;
    ESP_LOGI(TAG,"dummy_addr: 0x%lx", dummy_addr);

    size_t resolution = (MAX_POS - 1) * UPDATERATE;
    ESP_LOGI(TAG,"resolution: 0x%lx (%ld)", resolution, resolution);

    auto erase_from_pos = pos * UPDATERATE;
    auto erase_from_mc = move_count * MAX_POS * UPDATERATE + erase_from_pos;
    auto erase_from_cc = cycle_count * MAX_POS * (MAX_POS-1) * UPDATERATE + erase_from_mc;

    ESP_LOGI(TAG,"erase count from mc&pos: %ld", erase_from_mc);
    ESP_LOGI(TAG,"erase count including cycle_count: %ld", erase_from_cc);
}


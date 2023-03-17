#include "esp_log.h"
#include "esp_err.h"

#include "wl_sim.h"

static const char *TAG = "WL_Flash";

size_t access_count = 0;
size_t pos = 0;
size_t move_count = 0;
uint32_t cycle_count = 0;
uint32_t erase_count[SECTOR_COUNT + 1] = {0};
size_t last_erase_sector = 0;
size_t restarted = 0;

size_t calcAddr(size_t addr)
{
    size_t result = (FLASH_SIZE - move_count * PAGE_SIZE + addr) % FLASH_SIZE;
    size_t dummy_addr = pos * PAGE_SIZE;

    if (result < dummy_addr) {
    } else {
        result += PAGE_SIZE;
    }

    ESP_LOGD(TAG, "%s - addr= 0x%08x -> result= 0x%08x, dummy_addr= 0x%08x", __func__, (uint32_t) addr, (uint32_t) result, (uint32_t)dummy_addr); 
    return result;
}

int updateWL()
{
    int result = ESP_OK;

    access_count++;
    if (access_count < MAX_COUNT) {
        ESP_LOGD(TAG, "%s EARLY RETURN - access_count= 0x%08x, pos= 0x%08x, move_count= 0x%08x", __func__, (uint32_t) access_count, (uint32_t) pos, (uint32_t) move_count); 
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
    if (pos == MAX_POS/4 || pos == MAX_POS/2 || pos == 3*MAX_POS/4 || pos >= MAX_POS) {
        ESP_LOGI(TAG,"pos=%lu, breakpoints %u, %u, %u, %u", pos, MAX_POS/4, MAX_POS/2, 3*MAX_POS/4, MAX_POS);

        if (pos >= MAX_POS) {
            pos = 0;
        }
        // one loop more
        move_count++;
        if (move_count >= (MAX_POS - 1)) {
            move_count = 0;
            pos = 0;
            cycle_count++;
        }
        // write main state
    }

    ESP_LOGD(TAG, "%s - access_count= 0x%08x, pos= 0x%08x, move_count= 0x%08x", __func__, (uint32_t) access_count, (uint32_t) pos, (uint32_t) move_count); 
    return result;
}

int erase_sector(size_t sector)
{
    int result = ESP_OK;

    updateWL();
    size_t virt_addr = calcAddr(sector * SECTOR_SIZE);
    size_t phy_sector = virt_addr / SECTOR_SIZE;

    ESP_LOGD(TAG, "%s - virt_addr= 0x%08x, phy_sector= 0x%08x", __func__, (uint32_t) virt_addr, (uint32_t) phy_sector);

    erase_count[phy_sector]++;

    last_erase_sector = phy_sector;

    return result;
}

int erase_range(size_t start_address, size_t size)
{
    int result = ESP_OK;


    size_t erase_count = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    size_t start_sector = start_address / SECTOR_SIZE;

    ESP_LOGD(TAG, "%s - start_address= 0x%08x, size= 0x%08x, erase_count= 0x%08x, start_sector= 0x%08x",
            __func__, (uint32_t) start_address, (uint32_t) size, (uint32_t) erase_count, (uint32_t) start_sector); 

    for (size_t i = 0; i < erase_count; i++)
        erase_sector(start_sector + i);

    return result;
}

void print_erase_counts()
{
    uint64_t sum = 0;
    uint32_t min = UINT32_MAX, max = 0, nonzeros = 0;


    ESP_LOGI(TAG, "===== RESULTS =====");
    for (size_t i = 0; i <= SECTOR_COUNT; i++) {
        uint32_t count = erase_count[i];
        if (count != 0) {
            ESP_LOGI(TAG,"%ld: {%d}", i, count);
            sum += count;
            nonzeros++;

            if (count < min) min = count;
            if (count > max) max = count;
        }
    }
    ESP_LOGI(TAG, "MIN: %u\tMAX: %u\tAVG: %lu", min, max, sum/nonzeros);
}

void print_vars()
{
    ESP_LOGI(TAG, "===== VARS =====");
    ESP_LOGI(TAG, "access_count = %lu", access_count);
    ESP_LOGI(TAG, "pos = %lu", pos);
    ESP_LOGI(TAG, "move_count = %lu", move_count);
    ESP_LOGI(TAG, "cycle_count = %u", cycle_count);
    ESP_LOGI(TAG, "restarted = %lu", restarted);
}

#if 0
int main()
{
    srand(time(0));

    ESP_LOGI(TAG,"FULL_MEM_SIZE = %u (0x%x)", FULL_MEM_SIZE, FULL_MEM_SIZE);
    ESP_LOGI(TAG,"FLASH_SIZE = %u (0x%x)", FLASH_SIZE, FLASH_SIZE);
    ESP_LOGI(TAG,"MAX_POS = %u (0x%x)", MAX_POS, MAX_POS);
    ESP_LOGI(TAG,"MAX_POS - 1 = %u => * SECTOR_SIZE (%u) = %u", MAX_POS-1, SECTOR_SIZE, (MAX_POS-1)*SECTOR_SIZE);

#define RANDOM false
#define RESTART_PROBABILITY 1 // restart after each erase, in percent
#define ITERATIONS (100000)
#define ERASE_ADDRESS (FLASH_SIZE/3)
#define ERASE_SIZE (0x10)

    ESP_LOGI(TAG,"===== SETUP =====");
    ESP_LOGI(TAG,"iterations: %u", ITERATIONS);
#if RANDOM
    ESP_LOGI(TAG,"erase_address RANDOMIZED");
#else
    ESP_LOGI(TAG,"erase_address: 0x%x", ERASE_ADDRESS);
#endif
    ESP_LOGI(TAG,"erase_size: 0x%x", ERASE_SIZE);

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

    print_erase_count();
    print_vars();

    ESP_LOGI(TAG,"===== ANALYSIS =====");
    ESP_LOGI(TAG,"===== INCORRENT NUMBERS AS MC INCREMENTED MORE OFTEN =====");

    size_t dummy_addr = pos * PAGE_SIZE;
    ESP_LOGI(TAG,"dummy_addr: 0x%lx", dummy_addr);

    size_t resolution = (MAX_POS - 1) * UPDATERATE;
    ESP_LOGI(TAG,"resolution: 0x%lx (%ld)", resolution, resolution);

    ESP_LOGI(TAG,"last erase at %lu", last_erase_sector);

    ESP_LOGI(TAG,"number of sectors at resolution erases == move count");

    auto erase_from_pos = pos * UPDATERATE;
    auto erase_from_mc = move_count * MAX_POS * UPDATERATE + erase_from_pos;
    auto erase_from_cc = cycle_count * MAX_POS * (MAX_POS-1) * UPDATERATE + erase_from_mc;

    ESP_LOGI(TAG,"erase count from mc&pos: %ld", erase_from_mc);

    ESP_LOGI(TAG,"erase count including cycle_count: %ld", erase_from_cc);

    return 0;
}
#endif

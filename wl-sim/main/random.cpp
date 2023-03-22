#include <cassert>
#include <cmath>
#include <random>
#include "esp_log.h"
#include "random.h"
#include "dirty_zipfian_int_distribution.h"

static const char *TAG = "random";

size_t uniform(size_t max_addr)
{
    return rand() % max_addr;

}

size_t constant(size_t max_addr)
{
    return max_addr/2;
}

size_t zipf(size_t max_addr)
{
    auto max_sector = max_addr / SECTOR_SIZE;

    static std::default_random_engine generator;
    static dirtyzipf::dirty_zipfian_int_distribution<int> distribution(0, max_sector, 0.99);

    return distribution(generator) * SECTOR_SIZE;
}


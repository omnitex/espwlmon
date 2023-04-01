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

    auto ret = distribution(generator) * SECTOR_SIZE;

    return ret;
}

size_t linear(size_t max_addr)
{
    unsigned int max_sector = max_addr / SECTOR_SIZE;

    static std::random_device rd;
    static std::mt19937 gen{rd()};

    static std::vector<unsigned int> i{0, max_sector/2, 3*max_sector/2, max_sector};
    static std::vector<unsigned int> w{0, 1, 1, 0};
    static std::piecewise_linear_distribution<> d{i.begin(), i.end(), w.begin()};

    double x = d(gen);
    // TODO why does it generate outside of max_sector?
    // make sure x is in allowed range
    while (x >= max_sector) { x = d(gen); };

    unsigned int gen_sector = (unsigned int)x;
    //ESP_LOGI(TAG, "%s: generated %f => %u", __func__, x, gen_sector);
    return gen_sector * SECTOR_SIZE;
}

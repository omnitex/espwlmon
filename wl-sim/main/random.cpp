#include <cassert>
#include <cmath>
#include "random.h"

size_t uniform(size_t max_addr)
{
    return rand() % max_addr;

}

size_t constant(size_t max_addr)
{
    return CONSTANT_ADDRESS;
}


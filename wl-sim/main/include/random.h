#ifndef __RANDOM_H__
#define __RANDOM_H__

#include "wl_sim.h"

#define CONSTANT_ADDRESS (FLASH_SIZE/3)

size_t constant(size_t max_addr);
size_t uniform(size_t max_addr);


#endif


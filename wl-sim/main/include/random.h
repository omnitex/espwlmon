#ifndef __RANDOM_H__
#define __RANDOM_H__

#include "wl_sim.h"

#define CONSTANT_ADDRESS (FLASH_SIZE/3)

// fixed middle of address space
size_t constant(size_t max_addr);

// self explanatory
size_t uniform(size_t max_addr);

/* start heavy
 *  --
 *    \
 *     \~~__
 */
size_t zipf(size_t max_addr);

/* piece-wise linear distribution
 *     /~--~\
 *  _~/      \~_
 */
size_t linear(size_t max_addr);

#endif


#pragma once

#include "wl_sim.h"

#define CONSTANT_ADDRESS (FLASH_SIZE/3)

// fixed middle of address space
size_t constant(size_t max_addr);

// self explanatory
size_t uniform(size_t max_addr);

/* start heavy distribution for <0, max_addr>
 *  --
 *    \
 *     \~~__
 */
size_t zipf(size_t max_addr);

/*
 * Just returns the block given as argument
 */
size_t block_constant(size_t erase_block);

/*
 * Zipf distribution for <1, erase_block>
 */
size_t block_zipf(size_t erase_block);

#ifndef __WL_FLASH_H__
#define __WL_FLASH_H__

#include "esp_err.h"

//uint32_t restarted;

esp_err_t erase_range(size_t start_address, size_t size);
void print_erase_counts();
void print_vars();
void print_reconstructed();
void init_feistel();

#endif


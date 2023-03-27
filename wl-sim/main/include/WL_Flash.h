#ifndef __WL_FLASH_H__
#define __WL_FLASH_H__

#include "esp_err.h"

esp_err_t erase_range(size_t start_address, size_t size);
void print_erase_counts(bool verbose);
void print_vars();
void print_reconstructed();
void init_feistel();

size_t feistel_network(size_t logical_addr);

#endif


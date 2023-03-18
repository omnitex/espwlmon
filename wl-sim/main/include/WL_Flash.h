#ifndef __WL_FLASH_H__
#define __WL_FLASH_H__

#include "esp_err.h"

esp_err_t erase_range(size_t start_address, size_t size);
void print_erase_counts();
void print_vars();
void print_reconstructed();

#endif


#pragma once

#include "esp_err.h"

void init_feistel(bool verbose);
size_t feistel_network(size_t logical_addr);
esp_err_t erase_range(size_t start_address, size_t size);

void print_output();

void print_vars();
void print_reconstructed();

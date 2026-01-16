#ifndef QUICKSORT_H
#define QUICKSORT_H

#include <stdint.h>

void quicksort(uint8_t *arr, int left, int right);
uint8_t compute_sorted_xor_checksum(uint8_t *arr, int len);

#endif // QUICKSORT_H

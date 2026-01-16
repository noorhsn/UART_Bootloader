#include "quicksort.h"
#include <stdint.h>

static int partition(uint8_t *a, int l, int r) {
    uint8_t pivot = a[r];
    int i = l - 1;
    for (int j = l; j < r; ++j) {
        if (a[j] <= pivot) {
            ++i;
            uint8_t t = a[i]; a[i] = a[j]; a[j] = t;
        }
    }
    uint8_t t = a[i+1]; a[i+1] = a[r]; a[r] = t;
    return i+1;
}

void quicksort(uint8_t *arr, int left, int right) {
    if (left < right) {
        int p = partition(arr, left, right);
        quicksort(arr, left, p-1);
        quicksort(arr, p+1, right);
    }
}

uint8_t compute_sorted_xor_checksum(uint8_t *arr, int len) {
    if (len <= 1) return 0;
    quicksort(arr, 0, len-1);
    uint8_t checksum = 0;
    for (int i = 0; i+1 < len; i += 2) {
        checksum ^= arr[i] ^ arr[i+1];
    }
    // if odd length, XOR last byte with 0
    if (len & 1) checksum ^= arr[len-1] ^ 0;
    return checksum;
}

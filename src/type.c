#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include "type.h"

size_t rpnmath_type_sizeof(size_t bitsize) {
  // Round up bit size to byte size
  return (bitsize + 7) / 8;
}

size_t rpnmath_type_allignof(size_t bytesize) {
  // Return alignment requirement, max is sizeof(void*)
  if (bytesize >= sizeof(void*)) {
    return sizeof(void*);
  }
  if (bytesize >= 8) return 8;
  if (bytesize >= 4) return 4;
  if (bytesize >= 2) return 2;
  return 1;
}

void rpnmath_type_int(rpnmath_type_t *type, size_t bitwidth) {
  type->kind = RPNMATH_TYPEKIND_INT;
  type->size = bitwidth;
  type->_int = bitwidth;
}

// Helper function to get the native C type size for a given bit width
size_t rpnmath_type_native_size(size_t bitwidth) {
  if (bitwidth <= 8) return 1;    // char
  if (bitwidth <= 16) return 2;   // short
  if (bitwidth <= 32) return 4;   // int
  if (bitwidth <= 64) return 8;   // long long
  
  // For anything over 64 bits, use TODO macro
  printf("TODO: Support for integers over 64 bits not implemented\n");
  abort();
}

// Helper function to determine if an operation would overflow
int rpnmath_type_would_overflow_add(long long a, long long b) {
  if (b > 0 && a > LLONG_MAX - b) return 1;
  if (b < 0 && a < LLONG_MIN - b) return 1;
  return 0;
}

int rpnmath_type_would_overflow_sub(long long a, long long b) {
  if (b < 0 && a > LLONG_MAX + b) return 1;
  if (b > 0 && a < LLONG_MIN + b) return 1;
  return 0;
}

int rpnmath_type_would_overflow_mul(long long a, long long b) {
  if (a == 0 || b == 0) return 0;
  if (a > 0 && b > 0 && a > LLONG_MAX / b) return 1;
  if (a < 0 && b < 0 && a < LLONG_MAX / b) return 1;
  if (a > 0 && b < 0 && b < LLONG_MIN / a) return 1;
  if (a < 0 && b > 0 && a < LLONG_MIN / b) return 1;
  return 0;
}

// Helper function to promote type to larger size if needed
void rpnmath_type_promote(rpnmath_type_t *type, size_t min_bitwidth) {
  if (type->size < min_bitwidth) {
    type->size = min_bitwidth;
    type->_int = min_bitwidth;
  }
}
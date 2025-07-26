#ifndef RPNMATH_TYPE_H
#define RPNMATH_TYPE_H

typedef enum rpnmath_typekind {
  RPNMATH_TYPEKIND_VOID, // Error
  RPNMATH_TYPEKIND_INT,
} rpnmath_typekind_t;

typedef struct rpnmath_type {
  rpnmath_typekind_t kind;
  size_t size; // size is in bits (size is needed in bits for packing optimizations)
  union {
    size_t _int;
  };
} rpnmath_type_t;

size_t rpnmath_type_sizeof(size_t bitsize); // rounds up bit size to byte size
size_t rpnmath_type_allignof(size_t bytesize); // max allignment is sizeof(void*)

void rpnmath_type_int(rpnmath_type_t *type, size_t bitwidth);

size_t rpnmath_type_native_size(size_t bitwidth);

int rpnmath_type_would_overflow_add(long long a, long long b);
int rpnmath_type_would_overflow_sub(long long a, long long b);
int rpnmath_type_would_overflow_mul(long long a, long long b);
void rpnmath_type_promote(rpnmath_type_t *type, size_t min_bitwidth);

#endif // RPNMATH_TYPE_H
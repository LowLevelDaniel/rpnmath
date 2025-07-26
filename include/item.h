#ifndef RPNMATH_ITEM_H
#define RPNMATH_ITEM_H

#include <stddef.h>
#include "type.h"

typedef enum rpnmath_itemkind {
  RPNMATH_ITEMKIND_VOID,
  RPNMATH_ITEMKIND_CONST, // Constant
  RPNMATH_ITEMKIND_BINOP, // Binary Operation
} rpnmath_itemkind_t;

typedef enum rpnmath_binop {
  RPNMATH_BINOP_ADD,
  RPNMATH_BINOP_SUB,
  RPNMATH_BINOP_MUL,
  RPNMATH_BINOP_DIV, 
} rpnmath_binop_t;

typedef struct rpnmath_item {
  rpnmath_itemkind_t kind;
} rpnmath_item_t;

typedef struct rpnmath_item_const {
  rpnmath_itemkind_t kind;
  rpnmath_type_t type;
  void *data;
  size_t size; // if an i23 is used then size will be 3 bytes as it is rounded up
} rpnmath_item_const_t;

typedef struct rpnmath_item_binop {
  rpnmath_itemkind_t kind;
  rpnmath_binop_t operation;
} rpnmath_item_binop_t;

#endif // RPNMATH_ITEM_H
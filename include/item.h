#ifndef RPNMATH_ITEM_H
#define RPNMATH_ITEM_H

#include <stddef.h>
#include "type.h"

typedef enum rpnmath_itemkind {
  RPNMATH_ITEMKIND_VOID,
  RPNMATH_ITEMKIND_CONST,  // Constant
  RPNMATH_ITEMKIND_VARREF, // Variable Reference ($0, $1, etc.)
  RPNMATH_ITEMKIND_OP,     // Operation (binary ops, assignment, return, etc.)
} rpnmath_itemkind_t;

typedef enum rpnmath_op {
  RPNMATH_OP_ADD,    // Binary: 2 args, 1 return
  RPNMATH_OP_SUB,    // Binary: 2 args, 1 return
  RPNMATH_OP_MUL,    // Binary: 2 args, 1 return
  RPNMATH_OP_DIV,    // Binary: 2 args, 1 return
  RPNMATH_OP_ASSIGN, // Assignment: 2 args (value, variable), 0 returns
  RPNMATH_OP_RETURN, // Return: 1 arg, 1 return (stops execution)
} rpnmath_op_t;

typedef struct rpnmath_item {
  rpnmath_itemkind_t kind;
} rpnmath_item_t;

typedef struct rpnmath_item_const {
  rpnmath_itemkind_t kind;
  rpnmath_type_t type;
  void *data;
  size_t size; // if an i23 is used then size will be 3 bytes as it is rounded up
} rpnmath_item_const_t;

typedef struct rpnmath_item_varref {
  rpnmath_itemkind_t kind;
  size_t variable_id; // variable identifier ($0 = 0, $1 = 1, etc.)
} rpnmath_item_varref_t;

typedef struct rpnmath_item_op {
  rpnmath_itemkind_t kind;
  rpnmath_op_t operation;
} rpnmath_item_op_t;

// Helper functions to get operation properties
int rpnmath_op_arg_count(rpnmath_op_t op);
int rpnmath_op_return_count(rpnmath_op_t op);
const char* rpnmath_op_name(rpnmath_op_t op);

#endif // RPNMATH_ITEM_H
#ifndef RPNMATH_ITEMS_H
#define RPNMATH_ITEMS_H

#include <stddef.h>
#include "type.h"

typedef enum rpnmath_itemkind {
  RPNMATH_ITEMKIND_VOID, // No Operation
  RPNMATH_ITEMKIND_CONST,  // Constant
  RPNMATH_ITEMKIND_OP, // Operation
  RPNMATH_ITEMKIND_LREF, // Local Reference ($0, $1, etc.)
  RPNMATH_ITEMKIND_VOP, // Variable Operation
  RPNMATH_ITEMKIND_CFOP, // Control Flow
} rpnmath_itemkind_t;

typedef enum rpnmath_op {
  RPNMATH_OP_ADD,    // (Value,Value)-> Value
  RPNMATH_OP_SUB,    // (Value,Value)-> Value
  RPNMATH_OP_MUL,    // (Value,Value)-> Value
  RPNMATH_OP_DIV,    // (Value,Value)-> Value
  RPNMATH_OP_ASSIGN, // (Value, Variable) -> Void
  // Comparison operations
  RPNMATH_OP_EQ,     // (Value,Value)-> Bool (==)
  RPNMATH_OP_NE,     // (Value,Value)-> Bool (!=)
  RPNMATH_OP_LT,     // (Value,Value)-> Bool (<)
  RPNMATH_OP_LE,     // (Value,Value)-> Bool (<=)
  RPNMATH_OP_GT,     // (Value,Value)-> Bool (>)
  RPNMATH_OP_GE,     // (Value,Value)-> Bool (>=)
} rpnmath_op_t;

typedef enum rpnmath_vop {
  RPNMATH_VOP_RET, // (...) -> Exit
  RPNMATH_VOP_CALL, // (...) -> (...)
} rpnmath_vop_t;

typedef enum rpnmath_cfop {
  RPNMATH_CFOP_IF, // COND
  RPNMATH_CFOP_ELIF, // COND
  RPNMATH_CFOP_ELSE, //
  RPNMATH_CFOP_LOOP, //
  RPNMATH_CFOP_WHILE, // COND
  RPNMATH_CFOP_MERGE, //
  RPNMATH_CFOP_END, // END
  RPNMATH_CFOP_PHI, // PHI node for SSA
} rpnmath_cfop_t;

typedef struct rpnmath_item {
  rpnmath_itemkind_t kind;
} rpnmath_item_t;

typedef struct rpnmath_item_const {
  rpnmath_itemkind_t kind;
  rpnmath_type_t type;
  void *data;
  size_t size; // if an i23 is used then size will be 3 bytes as it is rounded up
} rpnmath_item_const_t;

typedef struct rpnmath_item_localref {
  rpnmath_itemkind_t kind;
  size_t variable_id; // variable identifier ($0 = 0, $1 = 1, etc.)
} rpnmath_item_localref_t;

typedef struct rpnmath_item_op {
  rpnmath_itemkind_t kind;
  rpnmath_op_t operation;
} rpnmath_item_op_t;

typedef struct rpnmath_item_vop {
  // Example 10 ret/1 to return 10
  rpnmath_itemkind_t kind;
  rpnmath_vop_t operation;
  size_t argcount;
  size_t retcount;
} rpnmath_item_vop_t;

typedef struct rpnmath_item_cfop {
  rpnmath_itemkind_t kind;
  rpnmath_cfop_t operation;
  union {
    struct {
      size_t target_var;    // For PHI: target variable
      size_t source_count;  // For PHI: number of source variables
      size_t *source_vars;  // For PHI: array of source variable IDs
    } phi;
    struct {
      size_t block_id;      // For control flow: target block ID
    } cf;
  };
} rpnmath_item_cfop_t;

// Helper functions to get operation properties
int rpnmath_op_arg_count(rpnmath_op_t op);
int rpnmath_op_return_count(rpnmath_op_t op);
const char* rpnmath_op_name(rpnmath_op_t op);

int rpnmath_vop_arg_count(rpnmath_vop_t op, size_t argcount);
int rpnmath_vop_return_count(rpnmath_vop_t op, size_t retcount);
const char* rpnmath_vop_name(rpnmath_vop_t op);

const char* rpnmath_cfop_name(rpnmath_cfop_t op);

#endif // RPNMATH_ITEMS_H
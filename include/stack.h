#ifndef RPNMATH_STACK_H
#define RPNMATH_STACK_H

#include <stddef.h>
#include "item.h"

#define RPNMATH_MAX_VARIABLES 256

typedef struct rpnmath_variable {
  rpnmath_item_const_t value;
  int is_assigned; // 0 = unassigned, 1 = assigned (for SSA enforcement)
} rpnmath_variable_t;

typedef struct rpnmath_stack {
  char *data;
  size_t size; // current size (bytes used)
  size_t capacity; // size the stack can expand to
  rpnmath_variable_t variables[RPNMATH_MAX_VARIABLES]; // variable storage
} rpnmath_stack_t;

// Initialize the stack
void rpnmath_stack_init(rpnmath_stack_t *stack, size_t sizehint);

// Clean up the stack
void rpnmath_stack_cleanup(rpnmath_stack_t *stack);

// Check if stack is empty
int rpnmath_stack_isempty(rpnmath_stack_t *stack);

// Push operations
void rpnmath_stack_pushc(rpnmath_stack_t *stack, rpnmath_item_const_t *item);
void rpnmath_stack_pushvr(rpnmath_stack_t *stack, rpnmath_item_varref_t *item);
void rpnmath_stack_pushop(rpnmath_stack_t *stack, rpnmath_item_op_t *item);

// Pop operations
rpnmath_itemkind_t rpnmath_stack_peekk(rpnmath_stack_t *stack);
rpnmath_item_const_t rpnmath_stack_popc(rpnmath_stack_t *stack);
rpnmath_item_varref_t rpnmath_stack_popvr(rpnmath_stack_t *stack);
rpnmath_item_op_t rpnmath_stack_popop(rpnmath_stack_t *stack);

// Variable operations
int rpnmath_stack_assign_variable(rpnmath_stack_t *stack, size_t var_id, rpnmath_item_const_t *value);
rpnmath_item_const_t rpnmath_stack_get_variable(rpnmath_stack_t *stack, size_t var_id);

// Count items
int rpnmath_stack_count_constants(rpnmath_stack_t *stack);

// Execute
int rpnmath_stack_execute(rpnmath_stack_t *stack, rpnmath_item_const_t *result);

#endif // RPNMATH_STACK_H
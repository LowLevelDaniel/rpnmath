#ifndef RPNMATH_STACK_H
#define RPNMATH_STACK_H

#include <stddef.h>
#include "item.h"

typedef struct rpnmath_stack {
  char *data;
  size_t size; // current size (bytes used)
  size_t capacity; // size the stack can expand to
} rpnmath_stack_t;

// Initialize the stack
void rpnmath_stack_init(rpnmath_stack_t *stack, size_t sizehint);

// Clean up the stack
void rpnmath_stack_cleanup(rpnmath_stack_t *stack);

// Check if stack is empty
int rpnmath_stack_isempty(rpnmath_stack_t *stack);

// Push
void rpnmath_stack_pushc(rpnmath_stack_t *stack, rpnmath_item_const_t *item);
void rpnmath_stack_pushbo(rpnmath_stack_t *stack, rpnmath_item_binop_t *item);

// Pop
rpnmath_itemkind_t rpnmath_stack_peekk(rpnmath_stack_t *stack);
rpnmath_item_const_t rpnmath_stack_popc(rpnmath_stack_t *stack);
rpnmath_item_binop_t rpnmath_stack_popbo(rpnmath_stack_t *stack);

// Count items
int rpnmath_stack_count_constants(rpnmath_stack_t *stack);

// Execute
void rpnmath_stack_execute(rpnmath_stack_t *stack);

#endif // RPNMATH_STACK_H
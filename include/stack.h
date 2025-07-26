#ifndef RPNMATH_STACK_H
#define RPNMATH_STACK_H

#include <stddef.h>
#include "item.h"

#define RPNMATH_MAX_VARIABLES 256
#define RPNMATH_MAX_BLOCKS 128
#define RPNMATH_MAX_BLOCK_STACK 32

typedef struct rpnmath_variable {
  rpnmath_item_const_t value;
  int is_assigned; // 0 = unassigned, 1 = assigned (for SSA enforcement)
  size_t version;  // SSA version number
  size_t block_id; // Block where this variable was assigned
} rpnmath_variable_t;

typedef struct rpnmath_block {
  size_t id;
  size_t start_pos;     // Start position in bytecode
  size_t end_pos;       // End position in bytecode
  size_t parent_block;  // Parent block ID
  int is_loop;          // 1 if this is a loop block
  int condition_result; // For conditional blocks: 0 = false, 1 = true, -1 = not evaluated
} rpnmath_block_t;

typedef struct rpnmath_stack {
  char *data;
  size_t size; // current size (bytes used)
  size_t capacity; // size the stack can expand to
  rpnmath_variable_t variables[RPNMATH_MAX_VARIABLES]; // variable storage
  
  // Block management
  rpnmath_block_t blocks[RPNMATH_MAX_BLOCKS];
  size_t block_count;
  size_t current_block;
  
  // Block execution stack
  size_t block_stack[RPNMATH_MAX_BLOCK_STACK];
  size_t block_stack_size;
  
  // Variable versioning for SSA
  size_t variable_versions[RPNMATH_MAX_VARIABLES];
} rpnmath_stack_t;

// Initialize the stack
void rpnmath_stack_init(rpnmath_stack_t *stack, size_t sizehint);

// Clean up the stack
void rpnmath_stack_cleanup(rpnmath_stack_t *stack);

// Check if stack is empty
int rpnmath_stack_isempty(rpnmath_stack_t *stack);

// Push operations
void rpnmath_stack_pushc(rpnmath_stack_t *stack, rpnmath_item_const_t *item);
void rpnmath_stack_pushlr(rpnmath_stack_t *stack, rpnmath_item_localref_t *item);
void rpnmath_stack_pushop(rpnmath_stack_t *stack, rpnmath_item_op_t *item);
void rpnmath_stack_pushvop(rpnmath_stack_t *stack, rpnmath_item_vop_t *item);
void rpnmath_stack_pushcfop(rpnmath_stack_t *stack, rpnmath_item_cfop_t *item);

// Pop operations
rpnmath_itemkind_t rpnmath_stack_peekk(rpnmath_stack_t *stack);
rpnmath_item_const_t rpnmath_stack_popc(rpnmath_stack_t *stack);
rpnmath_item_localref_t rpnmath_stack_poplr(rpnmath_stack_t *stack);
rpnmath_item_op_t rpnmath_stack_popop(rpnmath_stack_t *stack);
rpnmath_item_vop_t rpnmath_stack_popvop(rpnmath_stack_t *stack);
rpnmath_item_cfop_t rpnmath_stack_popcfop(rpnmath_stack_t *stack);

// Variable operations
int rpnmath_stack_assign_variable(rpnmath_stack_t *stack, size_t var_id, rpnmath_item_const_t *value);
rpnmath_item_const_t rpnmath_stack_get_variable(rpnmath_stack_t *stack, size_t var_id);

// Block management
size_t rpnmath_stack_create_block(rpnmath_stack_t *stack, size_t parent_block, int is_loop);
void rpnmath_stack_enter_block(rpnmath_stack_t *stack, size_t block_id);
void rpnmath_stack_exit_block(rpnmath_stack_t *stack);
int rpnmath_stack_evaluate_condition(rpnmath_stack_t *stack);

// Phi node operations
void rpnmath_stack_create_phi(rpnmath_stack_t *stack, size_t target_var, size_t *source_vars, size_t source_count);
int rpnmath_stack_resolve_phi(rpnmath_stack_t *stack, rpnmath_item_cfop_t *phi_op);

// Count items
int rpnmath_stack_count_constants(rpnmath_stack_t *stack);

// Execute
int rpnmath_stack_execute(rpnmath_stack_t *stack, rpnmath_item_const_t *result);

#endif // RPNMATH_STACK_H
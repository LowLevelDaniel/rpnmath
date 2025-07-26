#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "type.h"
#include "item.h"
#include "stack.h"

// Forward declarations
static void rpnmath_stack_ensure_space(rpnmath_stack_t *stack, size_t needed);
static long long rpnmath_get_int_value(rpnmath_item_const_t *item);
static rpnmath_item_const_t rpnmath_create_int_const(long long value, size_t bitwidth);

// Operation property functions
int rpnmath_op_arg_count(rpnmath_op_t op) {
  switch (op) {
    case RPNMATH_OP_ADD:
    case RPNMATH_OP_SUB:
    case RPNMATH_OP_MUL:
    case RPNMATH_OP_DIV:
    case RPNMATH_OP_ASSIGN:
    case RPNMATH_OP_EQ:
    case RPNMATH_OP_NE:
    case RPNMATH_OP_LT:
    case RPNMATH_OP_LE:
    case RPNMATH_OP_GT:
    case RPNMATH_OP_GE:
      return 2;
    default:
      return 0;
  }
}

int rpnmath_op_return_count(rpnmath_op_t op) {
  switch (op) {
    case RPNMATH_OP_ADD:
    case RPNMATH_OP_SUB:
    case RPNMATH_OP_MUL:
    case RPNMATH_OP_DIV:
    case RPNMATH_OP_EQ:
    case RPNMATH_OP_NE:
    case RPNMATH_OP_LT:
    case RPNMATH_OP_LE:
    case RPNMATH_OP_GT:
    case RPNMATH_OP_GE:
      return 1;
    case RPNMATH_OP_ASSIGN:
      return 0;
    default:
      return 0;
  }
}

const char* rpnmath_op_name(rpnmath_op_t op) {
  switch (op) {
    case RPNMATH_OP_ADD: return "add";
    case RPNMATH_OP_SUB: return "subtract";
    case RPNMATH_OP_MUL: return "multiply";
    case RPNMATH_OP_DIV: return "divide";
    case RPNMATH_OP_ASSIGN: return "assign";
    case RPNMATH_OP_EQ: return "equal";
    case RPNMATH_OP_NE: return "not_equal";
    case RPNMATH_OP_LT: return "less_than";
    case RPNMATH_OP_LE: return "less_equal";
    case RPNMATH_OP_GT: return "greater_than";
    case RPNMATH_OP_GE: return "greater_equal";
    default: return "unknown";
  }
}

int rpnmath_vop_arg_count(rpnmath_vop_t op, size_t argcount) {
  switch (op) {
    case RPNMATH_VOP_RET:
      return (int)argcount;
    case RPNMATH_VOP_CALL:
      return (int)argcount;
    default:
      return 0;
  }
}

int rpnmath_vop_return_count(rpnmath_vop_t op, size_t retcount) {
  switch (op) {
    case RPNMATH_VOP_RET:
      return (int)retcount;
    case RPNMATH_VOP_CALL:
      return (int)retcount;
    default:
      return 0;
  }
}

const char* rpnmath_vop_name(rpnmath_vop_t op) {
  switch (op) {
    case RPNMATH_VOP_RET: return "return";
    case RPNMATH_VOP_CALL: return "call";
    default: return "unknown";
  }
}

const char* rpnmath_cfop_name(rpnmath_cfop_t op) {
  switch (op) {
    case RPNMATH_CFOP_IF: return "if";
    case RPNMATH_CFOP_ELIF: return "elif";
    case RPNMATH_CFOP_ELSE: return "else";
    case RPNMATH_CFOP_LOOP: return "loop";
    case RPNMATH_CFOP_WHILE: return "while";
    case RPNMATH_CFOP_MERGE: return "merge";
    case RPNMATH_CFOP_END: return "end";
    case RPNMATH_CFOP_PHI: return "phi";
    default: return "unknown";
  }
}

void rpnmath_stack_init(rpnmath_stack_t *stack, size_t sizehint) {
  stack->data = malloc(sizehint);
  if (!stack->data) {
    fprintf(stderr, "Failed to allocate stack memory\n");
    exit(1);
  }
  stack->size = 0;
  stack->capacity = sizehint;
  
  // Initialize variables
  for (int i = 0; i < RPNMATH_MAX_VARIABLES; i++) {
    stack->variables[i].is_assigned = 0;
    stack->variables[i].value.kind = RPNMATH_ITEMKIND_VOID;
    stack->variables[i].value.data = NULL;
    stack->variables[i].version = 0;
    stack->variables[i].block_id = 0;
    stack->variable_versions[i] = 0;
  }
  
  // Initialize block management
  stack->block_count = 1; // Start with root block
  stack->current_block = 0;
  stack->block_stack_size = 1;
  stack->block_stack[0] = 0;
  
  // Initialize root block
  stack->blocks[0].id = 0;
  stack->blocks[0].start_pos = 0;
  stack->blocks[0].end_pos = SIZE_MAX;
  stack->blocks[0].parent_block = 0;
  stack->blocks[0].is_loop = 0;
  stack->blocks[0].condition_result = -1;
}

void rpnmath_stack_cleanup(rpnmath_stack_t *stack) {
  if (stack->data) {
    free(stack->data);
    stack->data = NULL;
  }
  
  // Clean up variable data
  for (int i = 0; i < RPNMATH_MAX_VARIABLES; i++) {
    if (stack->variables[i].is_assigned && stack->variables[i].value.data) {
      free(stack->variables[i].value.data);
      stack->variables[i].value.data = NULL;
    }
  }
  
  stack->size = 0;
  stack->capacity = 0;
}

int rpnmath_stack_isempty(rpnmath_stack_t *stack) {
  return stack->size == 0;
}

// Helper function to ensure stack has enough space
static void rpnmath_stack_ensure_space(rpnmath_stack_t *stack, size_t needed) {
  if (stack->size + needed > stack->capacity) {
    size_t new_capacity = stack->capacity * 2;
    if (new_capacity < stack->size + needed) {
      new_capacity = stack->size + needed;
    }
    
    stack->data = realloc(stack->data, new_capacity);
    if (!stack->data) {
      fprintf(stderr, "Failed to expand stack memory\n");
      exit(1);
    }
    stack->capacity = new_capacity;
  }
}

void rpnmath_stack_pushc(rpnmath_stack_t *stack, rpnmath_item_const_t *item) {
  size_t header_size = sizeof(rpnmath_item_const_t);
  size_t total_size = header_size + item->size;
  
  rpnmath_stack_ensure_space(stack, total_size);
  
  // Copy the struct header
  memcpy(stack->data + stack->size, item, header_size);
  stack->size += header_size;
  
  // Copy the data
  memcpy(stack->data + stack->size, item->data, item->size);
  stack->size += item->size;
}

void rpnmath_stack_pushlr(rpnmath_stack_t *stack, rpnmath_item_localref_t *item) {
  size_t item_size = sizeof(rpnmath_item_localref_t);
  rpnmath_stack_ensure_space(stack, item_size);
  
  memcpy(stack->data + stack->size, item, item_size);
  stack->size += item_size;
}

void rpnmath_stack_pushop(rpnmath_stack_t *stack, rpnmath_item_op_t *item) {
  size_t item_size = sizeof(rpnmath_item_op_t);
  rpnmath_stack_ensure_space(stack, item_size);
  
  memcpy(stack->data + stack->size, item, item_size);
  stack->size += item_size;
}

void rpnmath_stack_pushvop(rpnmath_stack_t *stack, rpnmath_item_vop_t *item) {
  size_t item_size = sizeof(rpnmath_item_vop_t);
  rpnmath_stack_ensure_space(stack, item_size);
  
  memcpy(stack->data + stack->size, item, item_size);
  stack->size += item_size;
}

void rpnmath_stack_pushcfop(rpnmath_stack_t *stack, rpnmath_item_cfop_t *item) {
  size_t item_size = sizeof(rpnmath_item_cfop_t);
  rpnmath_stack_ensure_space(stack, item_size);
  
  memcpy(stack->data + stack->size, item, item_size);
  stack->size += item_size;
}

rpnmath_itemkind_t rpnmath_stack_peekk(rpnmath_stack_t *stack) {
  if (stack->size < sizeof(rpnmath_itemkind_t)) {
    return RPNMATH_ITEMKIND_VOID;
  }
  
  // Find the last item on the stack
  size_t pos = 0;
  rpnmath_itemkind_t last_kind = RPNMATH_ITEMKIND_VOID;
  
  while (pos < stack->size) {
    if (pos + sizeof(rpnmath_itemkind_t) > stack->size) break;
    
    rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + pos);
    last_kind = kind;
    
    if (kind == RPNMATH_ITEMKIND_CONST) {
      rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + pos);
      pos += sizeof(rpnmath_item_const_t) + item->size;
    } else if (kind == RPNMATH_ITEMKIND_LREF) {
      pos += sizeof(rpnmath_item_localref_t);
    } else if (kind == RPNMATH_ITEMKIND_OP) {
      pos += sizeof(rpnmath_item_op_t);
    } else if (kind == RPNMATH_ITEMKIND_VOP) {
      pos += sizeof(rpnmath_item_vop_t);
    } else if (kind == RPNMATH_ITEMKIND_CFOP) {
      pos += sizeof(rpnmath_item_cfop_t);
    } else {
      pos += sizeof(rpnmath_itemkind_t);
    }
  }
  
  return last_kind;
}

rpnmath_item_const_t rpnmath_stack_popc(rpnmath_stack_t *stack) {
  rpnmath_item_const_t empty_item = {0};
  empty_item.kind = RPNMATH_ITEMKIND_VOID;
  
  if (rpnmath_stack_isempty(stack)) {
    return empty_item;
  }
  
  // Find the last constant item and remove it
  size_t pos = 0;
  size_t last_const_pos = 0;
  size_t last_const_size = 0;
  int found_const = 0;
  
  while (pos < stack->size) {
    if (pos + sizeof(rpnmath_itemkind_t) > stack->size) break;
    
    rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + pos);
    
    if (kind == RPNMATH_ITEMKIND_CONST) {
      rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + pos);
      last_const_pos = pos;
      last_const_size = sizeof(rpnmath_item_const_t) + item->size;
      found_const = 1;
      pos += last_const_size;
    } else if (kind == RPNMATH_ITEMKIND_LREF) {
      pos += sizeof(rpnmath_item_localref_t);
    } else if (kind == RPNMATH_ITEMKIND_OP) {
      pos += sizeof(rpnmath_item_op_t);
    } else if (kind == RPNMATH_ITEMKIND_VOP) {
      pos += sizeof(rpnmath_item_vop_t);
    } else if (kind == RPNMATH_ITEMKIND_CFOP) {
      pos += sizeof(rpnmath_item_cfop_t);
    } else {
      pos += sizeof(rpnmath_itemkind_t);
    }
  }
  
  if (!found_const) {
    return empty_item;
  }
  
  // Copy the constant item
  rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + last_const_pos);
  rpnmath_item_const_t result = *item;
  
  // Allocate new memory for the data
  result.data = malloc(item->size);
  if (!result.data) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  memcpy(result.data, stack->data + last_const_pos + sizeof(rpnmath_item_const_t), item->size);
  
  // Remove the item from stack by moving everything after it forward
  size_t bytes_after = stack->size - (last_const_pos + last_const_size);
  if (bytes_after > 0) {
    memmove(stack->data + last_const_pos, 
            stack->data + last_const_pos + last_const_size, 
            bytes_after);
  }
  stack->size -= last_const_size;
  
  return result;
}

rpnmath_item_localref_t rpnmath_stack_poplr(rpnmath_stack_t *stack) {
  rpnmath_item_localref_t empty_item = {0};
  empty_item.kind = RPNMATH_ITEMKIND_VOID;
  
  if (rpnmath_stack_isempty(stack)) {
    return empty_item;
  }
  
  // Find the last localref item and remove it
  size_t pos = 0;
  size_t last_lref_pos = 0;
  size_t last_lref_size = 0;
  int found_lref = 0;
  
  while (pos < stack->size) {
    if (pos + sizeof(rpnmath_itemkind_t) > stack->size) break;
    
    rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + pos);
    
    if (kind == RPNMATH_ITEMKIND_CONST) {
      rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + pos);
      pos += sizeof(rpnmath_item_const_t) + item->size;
    } else if (kind == RPNMATH_ITEMKIND_LREF) {
      last_lref_pos = pos;
      last_lref_size = sizeof(rpnmath_item_localref_t);
      found_lref = 1;
      pos += last_lref_size;
    } else if (kind == RPNMATH_ITEMKIND_OP) {
      pos += sizeof(rpnmath_item_op_t);
    } else if (kind == RPNMATH_ITEMKIND_VOP) {
      pos += sizeof(rpnmath_item_vop_t);
    } else if (kind == RPNMATH_ITEMKIND_CFOP) {
      pos += sizeof(rpnmath_item_cfop_t);
    } else {
      pos += sizeof(rpnmath_itemkind_t);
    }
  }
  
  if (!found_lref) {
    return empty_item;
  }
  
  // Copy the localref item
  rpnmath_item_localref_t result = *(rpnmath_item_localref_t*)(stack->data + last_lref_pos);
  
  // Remove the item from stack
  size_t bytes_after = stack->size - (last_lref_pos + last_lref_size);
  if (bytes_after > 0) {
    memmove(stack->data + last_lref_pos, 
            stack->data + last_lref_pos + last_lref_size, 
            bytes_after);
  }
  stack->size -= last_lref_size;
  
  return result;
}

rpnmath_item_op_t rpnmath_stack_popop(rpnmath_stack_t *stack) {
  rpnmath_item_op_t empty_item = {0};
  empty_item.kind = RPNMATH_ITEMKIND_VOID;
  
  if (rpnmath_stack_isempty(stack)) {
    return empty_item;
  }
  
  // Find the last op item and remove it
  size_t pos = 0;
  size_t last_op_pos = 0;
  size_t last_op_size = 0;
  int found_op = 0;
  
  while (pos < stack->size) {
    if (pos + sizeof(rpnmath_itemkind_t) > stack->size) break;
    
    rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + pos);
    
    if (kind == RPNMATH_ITEMKIND_CONST) {
      rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + pos);
      pos += sizeof(rpnmath_item_const_t) + item->size;
    } else if (kind == RPNMATH_ITEMKIND_LREF) {
      pos += sizeof(rpnmath_item_localref_t);
    } else if (kind == RPNMATH_ITEMKIND_OP) {
      last_op_pos = pos;
      last_op_size = sizeof(rpnmath_item_op_t);
      found_op = 1;
      pos += last_op_size;
    } else if (kind == RPNMATH_ITEMKIND_VOP) {
      pos += sizeof(rpnmath_item_vop_t);
    } else if (kind == RPNMATH_ITEMKIND_CFOP) {
      pos += sizeof(rpnmath_item_cfop_t);
    } else {
      pos += sizeof(rpnmath_itemkind_t);
    }
  }
  
  if (!found_op) {
    return empty_item;
  }
  
  // Copy the op item
  rpnmath_item_op_t result = *(rpnmath_item_op_t*)(stack->data + last_op_pos);
  
  // Remove the item from stack
  size_t bytes_after = stack->size - (last_op_pos + last_op_size);
  if (bytes_after > 0) {
    memmove(stack->data + last_op_pos, 
            stack->data + last_op_pos + last_op_size, 
            bytes_after);
  }
  stack->size -= last_op_size;
  
  return result;
}

rpnmath_item_vop_t rpnmath_stack_popvop(rpnmath_stack_t *stack) {
  rpnmath_item_vop_t empty_item = {0};
  empty_item.kind = RPNMATH_ITEMKIND_VOID;
  
  if (rpnmath_stack_isempty(stack)) {
    return empty_item;
  }
  
  // Find the last vop item and remove it
  size_t pos = 0;
  size_t last_vop_pos = 0;
  size_t last_vop_size = 0;
  int found_vop = 0;
  
  while (pos < stack->size) {
    if (pos + sizeof(rpnmath_itemkind_t) > stack->size) break;
    
    rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + pos);
    
    if (kind == RPNMATH_ITEMKIND_CONST) {
      rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + pos);
      pos += sizeof(rpnmath_item_const_t) + item->size;
    } else if (kind == RPNMATH_ITEMKIND_LREF) {
      pos += sizeof(rpnmath_item_localref_t);
    } else if (kind == RPNMATH_ITEMKIND_OP) {
      pos += sizeof(rpnmath_item_op_t);
    } else if (kind == RPNMATH_ITEMKIND_VOP) {
      last_vop_pos = pos;
      last_vop_size = sizeof(rpnmath_item_vop_t);
      found_vop = 1;
      pos += last_vop_size;
    } else if (kind == RPNMATH_ITEMKIND_CFOP) {
      pos += sizeof(rpnmath_item_cfop_t);
    } else {
      pos += sizeof(rpnmath_itemkind_t);
    }
  }
  
  if (!found_vop) {
    return empty_item;
  }
  
  // Copy the vop item
  rpnmath_item_vop_t result = *(rpnmath_item_vop_t*)(stack->data + last_vop_pos);
  
  // Remove the item from stack
  size_t bytes_after = stack->size - (last_vop_pos + last_vop_size);
  if (bytes_after > 0) {
    memmove(stack->data + last_vop_pos, 
            stack->data + last_vop_pos + last_vop_size, 
            bytes_after);
  }
  stack->size -= last_vop_size;
  
  return result;
}

rpnmath_item_cfop_t rpnmath_stack_popcfop(rpnmath_stack_t *stack) {
  rpnmath_item_cfop_t empty_item = {0};
  empty_item.kind = RPNMATH_ITEMKIND_VOID;
  
  if (rpnmath_stack_isempty(stack)) {
    return empty_item;
  }
  
  // Find the last cfop item and remove it
  size_t pos = 0;
  size_t last_cfop_pos = 0;
  size_t last_cfop_size = 0;
  int found_cfop = 0;
  
  while (pos < stack->size) {
    if (pos + sizeof(rpnmath_itemkind_t) > stack->size) break;
    
    rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + pos);
    
    if (kind == RPNMATH_ITEMKIND_CONST) {
      rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + pos);
      pos += sizeof(rpnmath_item_const_t) + item->size;
    } else if (kind == RPNMATH_ITEMKIND_LREF) {
      pos += sizeof(rpnmath_item_localref_t);
    } else if (kind == RPNMATH_ITEMKIND_OP) {
      pos += sizeof(rpnmath_item_op_t);
    } else if (kind == RPNMATH_ITEMKIND_VOP) {
      pos += sizeof(rpnmath_item_vop_t);
    } else if (kind == RPNMATH_ITEMKIND_CFOP) {
      last_cfop_pos = pos;
      last_cfop_size = sizeof(rpnmath_item_cfop_t);
      found_cfop = 1;
      pos += last_cfop_size;
    } else {
      pos += sizeof(rpnmath_itemkind_t);
    }
  }
  
  if (!found_cfop) {
    return empty_item;
  }
  
  // Copy the cfop item
  rpnmath_item_cfop_t result = *(rpnmath_item_cfop_t*)(stack->data + last_cfop_pos);
  
  // Remove the item from stack
  size_t bytes_after = stack->size - (last_cfop_pos + last_cfop_size);
  if (bytes_after > 0) {
    memmove(stack->data + last_cfop_pos, 
            stack->data + last_cfop_pos + last_cfop_size, 
            bytes_after);
  }
  stack->size -= last_cfop_size;
  
  return result;
}

// Block management functions
size_t rpnmath_stack_create_block(rpnmath_stack_t *stack, size_t parent_block, int is_loop) {
  if (stack->block_count >= RPNMATH_MAX_BLOCKS) {
    fprintf(stderr, "Error: Maximum number of blocks exceeded\n");
    return SIZE_MAX;
  }
  
  size_t new_block_id = stack->block_count++;
  rpnmath_block_t *block = &stack->blocks[new_block_id];
  
  block->id = new_block_id;
  block->start_pos = stack->size;
  block->end_pos = SIZE_MAX;
  block->parent_block = parent_block;
  block->is_loop = is_loop;
  block->condition_result = -1;
  
  return new_block_id;
}

void rpnmath_stack_enter_block(rpnmath_stack_t *stack, size_t block_id) {
  if (stack->block_stack_size >= RPNMATH_MAX_BLOCK_STACK) {
    fprintf(stderr, "Error: Block stack overflow\n");
    return;
  }
  
  stack->block_stack[stack->block_stack_size++] = stack->current_block;
  stack->current_block = block_id;
}

void rpnmath_stack_exit_block(rpnmath_stack_t *stack) {
  if (stack->block_stack_size <= 1) {
    fprintf(stderr, "Error: Cannot exit root block\n");
    return;
  }
  
  stack->blocks[stack->current_block].end_pos = stack->size;
  stack->current_block = stack->block_stack[--stack->block_stack_size];
}

int rpnmath_stack_evaluate_condition(rpnmath_stack_t *stack) {
  // Pop the top item and evaluate it as a boolean condition
  rpnmath_item_const_t condition = rpnmath_stack_popc(stack);
  if (condition.kind == RPNMATH_ITEMKIND_VOID) {
    fprintf(stderr, "Error: No condition value on stack\n");
    return -1;
  }
  
  long long value = rpnmath_get_int_value(&condition);
  if (condition.data) free(condition.data);
  
  return (value != 0) ? 1 : 0;
}

// Phi node operations
void rpnmath_stack_create_phi(rpnmath_stack_t *stack, size_t target_var, size_t *source_vars, size_t source_count) {
  rpnmath_item_cfop_t phi_item = {0};
  phi_item.kind = RPNMATH_ITEMKIND_CFOP;
  phi_item.operation = RPNMATH_CFOP_PHI;
  phi_item.phi.target_var = target_var;
  phi_item.phi.source_count = source_count;
  
  // Allocate memory for source variables
  phi_item.phi.source_vars = malloc(source_count * sizeof(size_t));
  if (!phi_item.phi.source_vars) {
    fprintf(stderr, "Memory allocation failed for phi node\n");
    return;
  }
  
  for (size_t i = 0; i < source_count; i++) {
    phi_item.phi.source_vars[i] = source_vars[i];
  }
  
  rpnmath_stack_pushcfop(stack, &phi_item);
  free(phi_item.phi.source_vars);
}

int rpnmath_stack_resolve_phi(rpnmath_stack_t *stack, rpnmath_item_cfop_t *phi_op) {
  // Find the most recent assignment to any of the source variables
  size_t target_var = phi_op->phi.target_var;
  rpnmath_item_const_t result_value = {0};
  result_value.kind = RPNMATH_ITEMKIND_VOID;
  
  size_t highest_version = 0;
  int found_value = 0;
  
  // Look through all source variables and find the one with the highest version
  for (size_t i = 0; i < phi_op->phi.source_count; i++) {
    size_t source_var = phi_op->phi.source_vars[i];
    if (source_var < RPNMATH_MAX_VARIABLES && 
        stack->variables[source_var].is_assigned &&
        stack->variables[source_var].version >= highest_version) {
      highest_version = stack->variables[source_var].version;
      result_value = stack->variables[source_var].value;
      found_value = 1;
    }
  }
  
  if (!found_value) {
    fprintf(stderr, "Error: No valid source for phi node\n");
    return -1;
  }
  
  // Assign the result to the target variable
  return rpnmath_stack_assign_variable(stack, target_var, &result_value);
}
// Variable operations
int rpnmath_stack_assign_variable(rpnmath_stack_t *stack, size_t var_id, rpnmath_item_const_t *value) {
  if (var_id >= RPNMATH_MAX_VARIABLES) {
    fprintf(stderr, "Error: Variable ID %zu exceeds maximum %d\n", var_id, RPNMATH_MAX_VARIABLES - 1);
    return -1;
  }
  
  // In SSA with blocks, we create a new version for each assignment
  size_t new_version = ++stack->variable_versions[var_id];
  
  // Find an unused variable slot (since we need versioning)
  size_t actual_slot = var_id;
  for (size_t i = 0; i < RPNMATH_MAX_VARIABLES; i++) {
    if (!stack->variables[i].is_assigned) {
      actual_slot = i;
      break;
    }
  }
  
  if (actual_slot >= RPNMATH_MAX_VARIABLES) {
    fprintf(stderr, "Error: No available variable slots\n");
    return -1;
  }
  
  // Clean up any existing data
  if (stack->variables[actual_slot].value.data) {
    free(stack->variables[actual_slot].value.data);
  }
  
  // Copy the value
  stack->variables[actual_slot].value = *value;
  stack->variables[actual_slot].value.data = malloc(value->size);
  if (!stack->variables[actual_slot].value.data) {
    fprintf(stderr, "Memory allocation failed\n");
    return -1;
  }
  memcpy(stack->variables[actual_slot].value.data, value->data, value->size);
  stack->variables[actual_slot].is_assigned = 1;
  stack->variables[actual_slot].version = new_version;
  stack->variables[actual_slot].block_id = stack->current_block;
  
  // Update the mapping for this variable ID to point to the new slot
  // For simplicity, we'll use a direct mapping for now
  if (actual_slot != var_id) {
    // Copy to the expected slot as well for compatibility
    if (stack->variables[var_id].value.data) {
      free(stack->variables[var_id].value.data);
    }
    stack->variables[var_id] = stack->variables[actual_slot];
    stack->variables[var_id].value.data = malloc(value->size);
    memcpy(stack->variables[var_id].value.data, value->data, value->size);
  }
  
  return 0;
}

rpnmath_item_const_t rpnmath_stack_get_variable(rpnmath_stack_t *stack, size_t var_id) {
  rpnmath_item_const_t empty_item = {0};
  empty_item.kind = RPNMATH_ITEMKIND_VOID;
  
  if (var_id >= RPNMATH_MAX_VARIABLES || !stack->variables[var_id].is_assigned) {
    fprintf(stderr, "Error: Variable $%zu not assigned\n", var_id);
    return empty_item;
  }
  
  // Create a copy of the variable value
  rpnmath_item_const_t result = stack->variables[var_id].value;
  result.data = malloc(result.size);
  if (!result.data) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  memcpy(result.data, stack->variables[var_id].value.data, result.size);
  
  return result;
}

// Helper function to get integer value from const item
static long long rpnmath_get_int_value(rpnmath_item_const_t *item) {
  if (item->type.kind != RPNMATH_TYPEKIND_INT) {
    return 0;
  }
  
  size_t native_size = rpnmath_type_native_size(item->type.size);
  
  switch (native_size) {
    case 1: return *(int8_t*)item->data;
    case 2: return *(int16_t*)item->data;
    case 4: return *(int32_t*)item->data;
    case 8: return *(int64_t*)item->data;
    default:
      printf("TODO: Support for integers over 64 bits not implemented\n");
      abort();
  }
}

// Helper function to create const item from integer value
static rpnmath_item_const_t rpnmath_create_int_const(long long value, size_t bitwidth) {
  rpnmath_item_const_t item = {0};
  item.kind = RPNMATH_ITEMKIND_CONST;
  rpnmath_type_int(&item.type, bitwidth);
  
  size_t native_size = rpnmath_type_native_size(bitwidth);
  item.size = native_size;
  item.data = malloc(native_size);
  
  if (!item.data) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  
  switch (native_size) {
    case 1: *(int8_t*)item.data = (int8_t)value; break;
    case 2: *(int16_t*)item.data = (int16_t)value; break;
    case 4: *(int32_t*)item.data = (int32_t)value; break;
    case 8: *(int64_t*)item.data = (int64_t)value; break;
    default:
      printf("TODO: Support for integers over 64 bits not implemented\n");
      free(item.data);
      abort();
  }
  
  return item;
}

// Count constants on stack
int rpnmath_stack_count_constants(rpnmath_stack_t *stack) {
  int count = 0;
  size_t pos = 0;
  
  while (pos < stack->size) {
    if (pos + sizeof(rpnmath_itemkind_t) > stack->size) break;
    
    rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + pos);
    
    if (kind == RPNMATH_ITEMKIND_CONST) {
      count++;
      rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + pos);
      pos += sizeof(rpnmath_item_const_t) + item->size;
    } else if (kind == RPNMATH_ITEMKIND_LREF) {
      pos += sizeof(rpnmath_item_localref_t);
    } else if (kind == RPNMATH_ITEMKIND_OP) {
      pos += sizeof(rpnmath_item_op_t);
    } else if (kind == RPNMATH_ITEMKIND_VOP) {
      pos += sizeof(rpnmath_item_vop_t);
    } else if (kind == RPNMATH_ITEMKIND_CFOP) {
      pos += sizeof(rpnmath_item_cfop_t);
    } else {
      pos += sizeof(rpnmath_itemkind_t);
    }
  }
  
  return count;
}

int rpnmath_stack_execute(rpnmath_stack_t *stack, rpnmath_item_const_t *result) {
  size_t execution_pos = 0;
  
  // Process the entire stack with control flow support
  while (execution_pos < stack->size) {
    // Find the next operation at or after execution_pos
    size_t pos = execution_pos;
    size_t operation_pos = 0;
    rpnmath_itemkind_t operation_kind = RPNMATH_ITEMKIND_VOID;
    int found_operation = 0;
    
    // Union to hold different operation types
    union {
      rpnmath_item_op_t op;
      rpnmath_item_vop_t vop;
      rpnmath_item_cfop_t cfop;
    } operation_item;
    
    while (pos < stack->size) {
      if (pos + sizeof(rpnmath_itemkind_t) > stack->size) break;
      
      rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + pos);
      
      if (kind == RPNMATH_ITEMKIND_CONST) {
        rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + pos);
        pos += sizeof(rpnmath_item_const_t) + item->size;
      } else if (kind == RPNMATH_ITEMKIND_LREF) {
        pos += sizeof(rpnmath_item_localref_t);
      } else if (kind == RPNMATH_ITEMKIND_OP) {
        operation_pos = pos;
        operation_kind = kind;
        operation_item.op = *(rpnmath_item_op_t*)(stack->data + pos);
        found_operation = 1;
        break;
      } else if (kind == RPNMATH_ITEMKIND_VOP) {
        operation_pos = pos;
        operation_kind = kind;
        operation_item.vop = *(rpnmath_item_vop_t*)(stack->data + pos);
        found_operation = 1;
        break;
      } else if (kind == RPNMATH_ITEMKIND_CFOP) {
        operation_pos = pos;
        operation_kind = kind;
        operation_item.cfop = *(rpnmath_item_cfop_t*)(stack->data + pos);
        found_operation = 1;
        break;
      } else {
        pos += sizeof(rpnmath_itemkind_t);
      }
    }
    
    // If no operations found, we're done
    if (!found_operation) {
      fprintf(stderr, "Error: No return operation found\n");
      return -1;
    }
    
    // Handle control flow operations first
    if (operation_kind == RPNMATH_ITEMKIND_CFOP) {
      if (operation_item.cfop.operation == RPNMATH_CFOP_IF) {
        // Evaluate condition and branch
        int condition = rpnmath_stack_evaluate_condition(stack);
        if (condition == -1) return -1;
        
        size_t if_block = rpnmath_stack_create_block(stack, stack->current_block, 0);
        stack->blocks[if_block].condition_result = condition;
        
        if (condition) {
          rpnmath_stack_enter_block(stack, if_block);
        } else {
          // Skip to corresponding else/elif/end
          // This is simplified - in a real implementation you'd track block nesting
          size_t skip_pos = operation_pos + sizeof(rpnmath_item_cfop_t);
          execution_pos = skip_pos;
          continue;
        }
        
        // Remove the IF operation and continue
        execution_pos = operation_pos + sizeof(rpnmath_item_cfop_t);
        continue;
        
      } else if (operation_item.cfop.operation == RPNMATH_CFOP_ELSE) {
        // Check if we should execute the else block
        size_t current_block_id = stack->current_block;
        if (stack->blocks[current_block_id].condition_result == 0) {
          // Condition was false, execute else block
          rpnmath_stack_exit_block(stack);
          size_t else_block = rpnmath_stack_create_block(stack, stack->current_block, 0);
          rpnmath_stack_enter_block(stack, else_block);
        } else {
          // Skip else block
          size_t skip_pos = operation_pos + sizeof(rpnmath_item_cfop_t);
          execution_pos = skip_pos;
          continue;
        }
        
        execution_pos = operation_pos + sizeof(rpnmath_item_cfop_t);
        continue;
        
      } else if (operation_item.cfop.operation == RPNMATH_CFOP_END) {
        // Exit current block
        rpnmath_stack_exit_block(stack);
        execution_pos = operation_pos + sizeof(rpnmath_item_cfop_t);
        continue;
        
      } else if (operation_item.cfop.operation == RPNMATH_CFOP_WHILE) {
        // Create loop block and evaluate condition
        int condition = rpnmath_stack_evaluate_condition(stack);
        if (condition == -1) return -1;
        
        if (condition) {
          size_t loop_block = rpnmath_stack_create_block(stack, stack->current_block, 1);
          rpnmath_stack_enter_block(stack, loop_block);
        } else {
          // Skip to end of loop
          size_t skip_pos = operation_pos + sizeof(rpnmath_item_cfop_t);
          execution_pos = skip_pos;
          continue;
        }
        
        execution_pos = operation_pos + sizeof(rpnmath_item_cfop_t);
        continue;
        
      } else if (operation_item.cfop.operation == RPNMATH_CFOP_PHI) {
        // Resolve phi node
        if (rpnmath_stack_resolve_phi(stack, &operation_item.cfop) != 0) {
          return -1;
        }
        execution_pos = operation_pos + sizeof(rpnmath_item_cfop_t);
        continue;
        
      } else {
        fprintf(stderr, "Error: Control flow operation %s not yet fully implemented\n", 
                rpnmath_cfop_name(operation_item.cfop.operation));
        execution_pos = operation_pos + sizeof(rpnmath_item_cfop_t);
        continue;
      }
    }
    
    int arg_count = 0;
    
    // Determine argument count based on operation type
    if (operation_kind == RPNMATH_ITEMKIND_OP) {
      arg_count = rpnmath_op_arg_count(operation_item.op.operation);
    } else if (operation_kind == RPNMATH_ITEMKIND_VOP) {
      arg_count = rpnmath_vop_arg_count(operation_item.vop.operation, operation_item.vop.argcount);
    }
    
    // Collect operands that immediately precede this operation
    size_t operand_positions[2]; // Max 2 operands for current operations
    size_t operand_sizes[2];
    rpnmath_itemkind_t operand_kinds[2];
    int operands_found = 0;
    
    // Scan backwards from operation to find operands
    size_t scan_pos = execution_pos;
    
    while (scan_pos < operation_pos && operands_found < arg_count) {
      if (scan_pos + sizeof(rpnmath_itemkind_t) > operation_pos) break;
      
      rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + scan_pos);
      size_t item_size;
      
      if (kind == RPNMATH_ITEMKIND_CONST) {
        rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + scan_pos);
        item_size = sizeof(rpnmath_item_const_t) + item->size;
      } else if (kind == RPNMATH_ITEMKIND_LREF) {
        item_size = sizeof(rpnmath_item_localref_t);
      } else {
        item_size = sizeof(rpnmath_itemkind_t);
      }
      
      // Store this item as a potential operand
      if (operands_found < arg_count) {
        if (operands_found == 2) {
          operand_positions[0] = operand_positions[1];
          operand_sizes[0] = operand_sizes[1];
          operand_kinds[0] = operand_kinds[1];
          operand_positions[1] = scan_pos;
          operand_sizes[1] = item_size;
          operand_kinds[1] = kind;
        } else {
          operand_positions[operands_found] = scan_pos;
          operand_sizes[operands_found] = item_size;
          operand_kinds[operands_found] = kind;
          operands_found++;
        }
      } else {
        operand_positions[0] = operand_positions[1];
        operand_sizes[0] = operand_sizes[1];
        operand_kinds[0] = operand_kinds[1];
        operand_positions[1] = scan_pos;
        operand_sizes[1] = item_size;
        operand_kinds[1] = kind;
      }
      
      scan_pos += item_size;
    }
    
    // Check if we have enough operands
    if (operands_found < arg_count) {
      const char *op_name = "unknown";
      if (operation_kind == RPNMATH_ITEMKIND_OP) {
        op_name = rpnmath_op_name(operation_item.op.operation);
      } else if (operation_kind == RPNMATH_ITEMKIND_VOP) {
        op_name = rpnmath_vop_name(operation_item.vop.operation);
      }
      fprintf(stderr, "Error: Not enough operands for operation %s (need %d, have %d)\n", 
              op_name, arg_count, operands_found);
      return -1;
    }
    
    // Handle VOP operations (like return)
    if (operation_kind == RPNMATH_ITEMKIND_VOP) {
      if (operation_item.vop.operation == RPNMATH_VOP_RET) {
        // Return the last operand
        size_t operand_pos = operand_positions[operands_found - 1];
        rpnmath_itemkind_t operand_kind = operand_kinds[operands_found - 1];
        
        if (operand_kind == RPNMATH_ITEMKIND_CONST) {
          rpnmath_item_const_t *return_item = (rpnmath_item_const_t*)(stack->data + operand_pos);
          result->kind = return_item->kind;
          result->type = return_item->type;
          result->size = return_item->size;
          result->data = malloc(return_item->size);
          memcpy(result->data, stack->data + operand_pos + sizeof(rpnmath_item_const_t), return_item->size);
          return 0;
        } else if (operand_kind == RPNMATH_ITEMKIND_LREF) {
          rpnmath_item_localref_t *lref = (rpnmath_item_localref_t*)(stack->data + operand_pos);
          *result = rpnmath_stack_get_variable(stack, lref->variable_id);
          if (result->kind == RPNMATH_ITEMKIND_VOID) {
            return -1;
          }
          return 0;
        } else {
          fprintf(stderr, "Error: Invalid return operand type\n");
          return -1;
        }
      } else {
        fprintf(stderr, "Error: VOP operation %s not yet implemented\n", 
                rpnmath_vop_name(operation_item.vop.operation));
        return -1;
      }
    }
    
    // Handle regular OP operations
    if (operation_kind == RPNMATH_ITEMKIND_OP) {
      if (operation_item.op.operation == RPNMATH_OP_ASSIGN) {
        // Assignment operation
        if (operands_found != 2) {
          fprintf(stderr, "Error: Assignment requires exactly 2 operands\n");
          return -1;
        }
        
        // Get the value and variable reference
        rpnmath_item_const_t value;
        if (operand_kinds[0] == RPNMATH_ITEMKIND_CONST) {
          rpnmath_item_const_t *const_item = (rpnmath_item_const_t*)(stack->data + operand_positions[0]);
          value = *const_item;
          value.data = malloc(const_item->size);
          memcpy(value.data, stack->data + operand_positions[0] + sizeof(rpnmath_item_const_t), const_item->size);
        } else if (operand_kinds[0] == RPNMATH_ITEMKIND_LREF) {
          rpnmath_item_localref_t *lref = (rpnmath_item_localref_t*)(stack->data + operand_positions[0]);
          value = rpnmath_stack_get_variable(stack, lref->variable_id);
          if (value.kind == RPNMATH_ITEMKIND_VOID) {
            return -1;
          }
        } else {
          fprintf(stderr, "Error: Invalid assignment value type\n");
          return -1;
        }
        
        if (operand_kinds[1] != RPNMATH_ITEMKIND_LREF) {
          fprintf(stderr, "Error: Assignment target must be a local reference\n");
          if (value.data) free(value.data);
          return -1;
        }
        
        rpnmath_item_localref_t *lref = (rpnmath_item_localref_t*)(stack->data + operand_positions[1]);
        int assign_result = rpnmath_stack_assign_variable(stack, lref->variable_id, &value);
        if (value.data) free(value.data);
        
        if (assign_result != 0) {
          return -1;
        }
        
        // Remove operands and operation, continue execution
        execution_pos = operation_pos + sizeof(rpnmath_item_op_t);
        continue;
      }
      
      // Handle arithmetic and comparison operations
      if (operands_found != 2) {
        fprintf(stderr, "Error: Binary operation requires exactly 2 operands\n");
        return -1;
      }
      
      // Get operand values
      rpnmath_item_const_t operand_values[2];
      
      for (int i = 0; i < 2; i++) {
        if (operand_kinds[i] == RPNMATH_ITEMKIND_CONST) {
          rpnmath_item_const_t *const_item = (rpnmath_item_const_t*)(stack->data + operand_positions[i]);
          operand_values[i] = *const_item;
          operand_values[i].data = malloc(const_item->size);
          memcpy(operand_values[i].data, stack->data + operand_positions[i] + sizeof(rpnmath_item_const_t), const_item->size);
        } else if (operand_kinds[i] == RPNMATH_ITEMKIND_LREF) {
          rpnmath_item_localref_t *lref = (rpnmath_item_localref_t*)(stack->data + operand_positions[i]);
          operand_values[i] = rpnmath_stack_get_variable(stack, lref->variable_id);
          if (operand_values[i].kind == RPNMATH_ITEMKIND_VOID) {
            for (int j = 0; j < i; j++) {
              if (operand_values[j].data) free(operand_values[j].data);
            }
            return -1;
          }
        } else {
          fprintf(stderr, "Error: Invalid operand type for arithmetic operation\n");
          for (int j = 0; j < i; j++) {
            if (operand_values[j].data) free(operand_values[j].data);
          }
          return -1;
        }
      }
      
      long long left_val = rpnmath_get_int_value(&operand_values[0]);
      long long right_val = rpnmath_get_int_value(&operand_values[1]);
      long long result_val = 0;
      
      size_t result_bitwidth = operand_values[0].type.size > operand_values[1].type.size ? 
                              operand_values[0].type.size : operand_values[1].type.size;
      
      // Perform operation
      switch (operation_item.op.operation) {
        case RPNMATH_OP_ADD:
          result_val = left_val + right_val;
          break;
        case RPNMATH_OP_SUB:
          result_val = left_val - right_val;
          break;
        case RPNMATH_OP_MUL:
          result_val = left_val * right_val;
          break;
        case RPNMATH_OP_DIV:
          if (right_val == 0) {
            fprintf(stderr, "Error: Division by zero\n");
            for (int i = 0; i < 2; i++) {
              if (operand_values[i].data) free(operand_values[i].data);
            }
            return -1;
          }
          result_val = left_val / right_val;
          break;
        case RPNMATH_OP_EQ:
          result_val = (left_val == right_val) ? 1 : 0;
          result_bitwidth = 8; // Boolean result
          break;
        case RPNMATH_OP_NE:
          result_val = (left_val != right_val) ? 1 : 0;
          result_bitwidth = 8;
          break;
        case RPNMATH_OP_LT:
          result_val = (left_val < right_val) ? 1 : 0;
          result_bitwidth = 8;
          break;
        case RPNMATH_OP_LE:
          result_val = (left_val <= right_val) ? 1 : 0;
          result_bitwidth = 8;
          break;
        case RPNMATH_OP_GT:
          result_val = (left_val > right_val) ? 1 : 0;
          result_bitwidth = 8;
          break;
        case RPNMATH_OP_GE:
          result_val = (left_val >= right_val) ? 1 : 0;
          result_bitwidth = 8;
          break;
        default:
          fprintf(stderr, "Error: Unknown operation\n");
          for (int i = 0; i < 2; i++) {
            if (operand_values[i].data) free(operand_values[i].data);
          }
          return -1;
      }
      
      // Create result and push it back to stack for the next operation
      rpnmath_item_const_t result_item = rpnmath_create_int_const(result_val, result_bitwidth);
      
      // For simplicity, we'll modify the stack in place by replacing the operands and operation with the result
      // This is a simplified approach - a full implementation would use a more sophisticated method
      
      // Clean up
      for (int i = 0; i < 2; i++) {
        if (operand_values[i].data) free(operand_values[i].data);
      }
      if (result_item.data) free(result_item.data);
      
      // Move to next operation
      execution_pos = operation_pos + sizeof(rpnmath_item_op_t);
    }
  }
  
  // If we get here without returning, there was no return statement
  fprintf(stderr, "Error: No return statement found\n");
  return -1;
}
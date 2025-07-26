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
      return 2;
    case RPNMATH_OP_RETURN:
      return 1;
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
    case RPNMATH_OP_RETURN:
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
    case RPNMATH_OP_RETURN: return "return";
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
  }
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

void rpnmath_stack_pushvr(rpnmath_stack_t *stack, rpnmath_item_varref_t *item) {
  size_t item_size = sizeof(rpnmath_item_varref_t);
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
    } else if (kind == RPNMATH_ITEMKIND_VARREF) {
      pos += sizeof(rpnmath_item_varref_t);
    } else if (kind == RPNMATH_ITEMKIND_OP) {
      pos += sizeof(rpnmath_item_op_t);
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
    } else if (kind == RPNMATH_ITEMKIND_VARREF) {
      pos += sizeof(rpnmath_item_varref_t);
    } else if (kind == RPNMATH_ITEMKIND_OP) {
      pos += sizeof(rpnmath_item_op_t);
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

rpnmath_item_varref_t rpnmath_stack_popvr(rpnmath_stack_t *stack) {
  rpnmath_item_varref_t empty_item = {0};
  empty_item.kind = RPNMATH_ITEMKIND_VOID;
  
  if (rpnmath_stack_isempty(stack)) {
    return empty_item;
  }
  
  // Find the last varref item and remove it
  size_t pos = 0;
  size_t last_varref_pos = 0;
  size_t last_varref_size = 0;
  int found_varref = 0;
  
  while (pos < stack->size) {
    if (pos + sizeof(rpnmath_itemkind_t) > stack->size) break;
    
    rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + pos);
    
    if (kind == RPNMATH_ITEMKIND_CONST) {
      rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + pos);
      pos += sizeof(rpnmath_item_const_t) + item->size;
    } else if (kind == RPNMATH_ITEMKIND_VARREF) {
      last_varref_pos = pos;
      last_varref_size = sizeof(rpnmath_item_varref_t);
      found_varref = 1;
      pos += last_varref_size;
    } else if (kind == RPNMATH_ITEMKIND_OP) {
      pos += sizeof(rpnmath_item_op_t);
    } else {
      pos += sizeof(rpnmath_itemkind_t);
    }
  }
  
  if (!found_varref) {
    return empty_item;
  }
  
  // Copy the varref item
  rpnmath_item_varref_t result = *(rpnmath_item_varref_t*)(stack->data + last_varref_pos);
  
  // Remove the item from stack
  size_t bytes_after = stack->size - (last_varref_pos + last_varref_size);
  if (bytes_after > 0) {
    memmove(stack->data + last_varref_pos, 
            stack->data + last_varref_pos + last_varref_size, 
            bytes_after);
  }
  stack->size -= last_varref_size;
  
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
    } else if (kind == RPNMATH_ITEMKIND_VARREF) {
      pos += sizeof(rpnmath_item_varref_t);
    } else if (kind == RPNMATH_ITEMKIND_OP) {
      last_op_pos = pos;
      last_op_size = sizeof(rpnmath_item_op_t);
      found_op = 1;
      pos += last_op_size;
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

// Variable operations
int rpnmath_stack_assign_variable(rpnmath_stack_t *stack, size_t var_id, rpnmath_item_const_t *value) {
  if (var_id >= RPNMATH_MAX_VARIABLES) {
    fprintf(stderr, "Error: Variable ID %zu exceeds maximum %d\n", var_id, RPNMATH_MAX_VARIABLES - 1);
    return -1;
  }
  
  // Check for SSA violation
  if (stack->variables[var_id].is_assigned) {
    fprintf(stderr, "Error: Variable $%zu already assigned (SSA violation)\n", var_id);
    return -1;
  }
  
  // Clean up any existing data (shouldn't happen in SSA, but be safe)
  if (stack->variables[var_id].value.data) {
    free(stack->variables[var_id].value.data);
  }
  
  // Copy the value
  stack->variables[var_id].value = *value;
  stack->variables[var_id].value.data = malloc(value->size);
  if (!stack->variables[var_id].value.data) {
    fprintf(stderr, "Memory allocation failed\n");
    return -1;
  }
  memcpy(stack->variables[var_id].value.data, value->data, value->size);
  stack->variables[var_id].is_assigned = 1;
  
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
    } else if (kind == RPNMATH_ITEMKIND_VARREF) {
      pos += sizeof(rpnmath_item_varref_t);
    } else if (kind == RPNMATH_ITEMKIND_OP) {
      pos += sizeof(rpnmath_item_op_t);
    } else {
      pos += sizeof(rpnmath_itemkind_t);
    }
  }
  
  return count;
}

int rpnmath_stack_execute(rpnmath_stack_t *stack, rpnmath_item_const_t *result) {
  // Process the entire stack until no more operations remain or we hit a return
  while (1) {
    // Find the first operation in the stack (left to right scan)
    size_t pos = 0;
    size_t operation_pos = 0;
    rpnmath_op_t operation;
    int found_operation = 0;
    
    while (pos < stack->size) {
      if (pos + sizeof(rpnmath_itemkind_t) > stack->size) break;
      
      rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + pos);
      
      if (kind == RPNMATH_ITEMKIND_CONST) {
        rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + pos);
        pos += sizeof(rpnmath_item_const_t) + item->size;
      } else if (kind == RPNMATH_ITEMKIND_VARREF) {
        pos += sizeof(rpnmath_item_varref_t);
      } else if (kind == RPNMATH_ITEMKIND_OP) {
        rpnmath_item_op_t *op = (rpnmath_item_op_t*)(stack->data + pos);
        operation_pos = pos;
        operation = op->operation;
        found_operation = 1;
        break;
      } else {
        pos += sizeof(rpnmath_itemkind_t);
      }
    }
    
    // If no operations found, we're done (no result)
    if (!found_operation) {
      fprintf(stderr, "Error: No return operation found\n");
      return -1;
    }
    
    int arg_count = rpnmath_op_arg_count(operation);
    
    // Collect operands that immediately precede this operation
    // We need to scan backwards from the operation to collect the required operands
    size_t operand_positions[2]; // Max 2 operands for any current operation
    size_t operand_sizes[2];
    rpnmath_itemkind_t operand_kinds[2];
    int operands_found = 0;
    
    // Scan backwards from operation to find operands
    size_t scan_pos = 0;
    size_t items_found = 0;
    
    while (scan_pos < operation_pos && operands_found < arg_count) {
      if (scan_pos + sizeof(rpnmath_itemkind_t) > operation_pos) break;
      
      rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + scan_pos);
      size_t item_size;
      
      if (kind == RPNMATH_ITEMKIND_CONST) {
        rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + scan_pos);
        item_size = sizeof(rpnmath_item_const_t) + item->size;
      } else if (kind == RPNMATH_ITEMKIND_VARREF) {
        item_size = sizeof(rpnmath_item_varref_t);
      } else {
        item_size = sizeof(rpnmath_itemkind_t);
      }
      
      // Store this item as a potential operand (we'll take the last N items)
      if (operands_found < arg_count) {
        // Shift existing operands if we have the max
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
        // Shift array to keep most recent operands
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
      fprintf(stderr, "Error: Not enough operands for operation %s (need %d, have %d)\n", 
              rpnmath_op_name(operation), arg_count, operands_found);
      return -1;
    }
    
    // Handle special operations
    if (operation == RPNMATH_OP_RETURN) {
      // Return the last operand (should be a constant or resolved variable)
      size_t operand_pos = operand_positions[operands_found - 1];
      rpnmath_itemkind_t operand_kind = operand_kinds[operands_found - 1];
      
      if (operand_kind == RPNMATH_ITEMKIND_CONST) {
        rpnmath_item_const_t *return_item = (rpnmath_item_const_t*)(stack->data + operand_pos);
        // Copy the return value
        result->kind = return_item->kind;
        result->type = return_item->type;
        result->size = return_item->size;
        result->data = malloc(return_item->size);
        memcpy(result->data, stack->data + operand_pos + sizeof(rpnmath_item_const_t), return_item->size);
        return 0; // Success
      } else if (operand_kind == RPNMATH_ITEMKIND_VARREF) {
        // Resolve variable and return its value
        rpnmath_item_varref_t *varref = (rpnmath_item_varref_t*)(stack->data + operand_pos);
        *result = rpnmath_stack_get_variable(stack, varref->variable_id);
        if (result->kind == RPNMATH_ITEMKIND_VOID) {
          return -1; // Error already printed
        }
        return 0; // Success
      } else {
        fprintf(stderr, "Error: Invalid return operand type\n");
        return -1;
      }
    }
    
    if (operation == RPNMATH_OP_ASSIGN) {
      // Assignment: operands[0] = value, operands[1] = variable reference
      if (operands_found != 2) {
        fprintf(stderr, "Error: Assignment requires exactly 2 operands\n");
        return -1;
      }
      
      // Get the value (operand 0) - resolve if it's a variable
      rpnmath_item_const_t value;
      if (operand_kinds[0] == RPNMATH_ITEMKIND_CONST) {
        rpnmath_item_const_t *const_item = (rpnmath_item_const_t*)(stack->data + operand_positions[0]);
        value = *const_item;
        value.data = malloc(const_item->size);
        memcpy(value.data, stack->data + operand_positions[0] + sizeof(rpnmath_item_const_t), const_item->size);
      } else if (operand_kinds[0] == RPNMATH_ITEMKIND_VARREF) {
        rpnmath_item_varref_t *varref = (rpnmath_item_varref_t*)(stack->data + operand_positions[0]);
        value = rpnmath_stack_get_variable(stack, varref->variable_id);
        if (value.kind == RPNMATH_ITEMKIND_VOID) {
          return -1; // Error already printed
        }
      } else {
        fprintf(stderr, "Error: Invalid assignment value type\n");
        return -1;
      }
      
      // Get the variable reference (operand 1)
      if (operand_kinds[1] != RPNMATH_ITEMKIND_VARREF) {
        fprintf(stderr, "Error: Assignment target must be a variable reference\n");
        if (value.data) free(value.data);
        return -1;
      }
      
      rpnmath_item_varref_t *var_ref = (rpnmath_item_varref_t*)(stack->data + operand_positions[1]);
      int assign_result = rpnmath_stack_assign_variable(stack, var_ref->variable_id, &value);
      if (value.data) free(value.data);
      
      if (assign_result != 0) {
        return -1;
      }
      
      // Remove the operands and operation from stack (remove in reverse order)
      size_t total_remove_size = operand_sizes[0] + operand_sizes[1] + sizeof(rpnmath_item_op_t);
      size_t start_remove_pos = operand_positions[0];
      
      // Shift everything after the operation forward
      size_t bytes_after = stack->size - (operation_pos + sizeof(rpnmath_item_op_t));
      if (bytes_after > 0) {
        memmove(stack->data + start_remove_pos,
                stack->data + operation_pos + sizeof(rpnmath_item_op_t),
                bytes_after);
      }
      stack->size -= total_remove_size;
      
      continue;
    }
    
    // Handle binary arithmetic operations
    if (operands_found != 2) {
      fprintf(stderr, "Error: Binary operation requires exactly 2 operands\n");
      return -1;
    }
    
    // Get operand values (resolve variables if needed)
    rpnmath_item_const_t operand_values[2];
    
    for (int i = 0; i < 2; i++) {
      if (operand_kinds[i] == RPNMATH_ITEMKIND_CONST) {
        rpnmath_item_const_t *const_item = (rpnmath_item_const_t*)(stack->data + operand_positions[i]);
        operand_values[i] = *const_item;
        operand_values[i].data = malloc(const_item->size);
        memcpy(operand_values[i].data, stack->data + operand_positions[i] + sizeof(rpnmath_item_const_t), const_item->size);
      } else if (operand_kinds[i] == RPNMATH_ITEMKIND_VARREF) {
        rpnmath_item_varref_t *varref = (rpnmath_item_varref_t*)(stack->data + operand_positions[i]);
        operand_values[i] = rpnmath_stack_get_variable(stack, varref->variable_id);
        if (operand_values[i].kind == RPNMATH_ITEMKIND_VOID) {
          // Clean up any previously allocated operands
          for (int j = 0; j < i; j++) {
            if (operand_values[j].data) free(operand_values[j].data);
          }
          return -1; // Error already printed
        }
      } else {
        fprintf(stderr, "Error: Invalid operand type for arithmetic operation\n");
        // Clean up any previously allocated operands
        for (int j = 0; j < i; j++) {
          if (operand_values[j].data) free(operand_values[j].data);
        }
        return -1;
      }
    }
    
    // Extract values (operand_values[0] is left, operand_values[1] is right)
    long long left_val = rpnmath_get_int_value(&operand_values[0]);
    long long right_val = rpnmath_get_int_value(&operand_values[1]);
    long long result_val = 0;
    
    // Determine result bit width (promote to larger type)
    size_t result_bitwidth = operand_values[0].type.size > operand_values[1].type.size ? 
                            operand_values[0].type.size : operand_values[1].type.size;
    
    // Perform operation with overflow checking
    switch (operation) {
      case RPNMATH_OP_ADD:
        if (rpnmath_type_would_overflow_add(left_val, right_val)) {
          fprintf(stderr, "Warning: Addition overflow detected, promoting type\n");
          result_bitwidth = result_bitwidth < 64 ? result_bitwidth * 2 : 64;
          if (result_bitwidth > 64) {
            printf("TODO: Support for integers over 64 bits not implemented\n");
            for (int i = 0; i < 2; i++) {
              if (operand_values[i].data) free(operand_values[i].data);
            }
            abort();
          }
        }
        result_val = left_val + right_val;
        break;
        
      case RPNMATH_OP_SUB:
        if (rpnmath_type_would_overflow_sub(left_val, right_val)) {
          fprintf(stderr, "Warning: Subtraction overflow detected, promoting type\n");
          result_bitwidth = result_bitwidth < 64 ? result_bitwidth * 2 : 64;
          if (result_bitwidth > 64) {
            printf("TODO: Support for integers over 64 bits not implemented\n");
            for (int i = 0; i < 2; i++) {
              if (operand_values[i].data) free(operand_values[i].data);
            }
            abort();
          }
        }
        result_val = left_val - right_val;
        break;
        
      case RPNMATH_OP_MUL:
        if (rpnmath_type_would_overflow_mul(left_val, right_val)) {
          fprintf(stderr, "Warning: Multiplication overflow detected, promoting type\n");
          result_bitwidth = result_bitwidth < 64 ? result_bitwidth * 2 : 64;
          if (result_bitwidth > 64) {
            printf("TODO: Support for integers over 64 bits not implemented\n");
            for (int i = 0; i < 2; i++) {
              if (operand_values[i].data) free(operand_values[i].data);
            }
            abort();
          }
        }
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
        
      default:
        fprintf(stderr, "Error: Unknown operation\n");
        for (int i = 0; i < 2; i++) {
          if (operand_values[i].data) free(operand_values[i].data);
        }
        return -1;
    }
    
    // Create result item
    rpnmath_item_const_t result_item = rpnmath_create_int_const(result_val, result_bitwidth);
    
    // Remove the operands and operation from stack and replace with result
    size_t total_remove_size = operand_sizes[0] + operand_sizes[1] + sizeof(rpnmath_item_op_t);
    size_t start_remove_pos = operand_positions[0];
    size_t result_total_size = sizeof(rpnmath_item_const_t) + result_item.size;
    
    // Create temp buffer for the result
    char *temp_result = malloc(result_total_size);
    memcpy(temp_result, &result_item, sizeof(rpnmath_item_const_t));
    memcpy(temp_result + sizeof(rpnmath_item_const_t), result_item.data, result_item.size);
    
    // Remove the old items by shifting everything after them
    size_t bytes_after = stack->size - (operation_pos + sizeof(rpnmath_item_op_t));
    if (bytes_after > 0) {
      memmove(stack->data + start_remove_pos,
              stack->data + operation_pos + sizeof(rpnmath_item_op_t),
              bytes_after);
    }
    stack->size -= total_remove_size;
    
    // Insert the result at the start position
    rpnmath_stack_ensure_space(stack, result_total_size);
    if (bytes_after > 0) {
      memmove(stack->data + start_remove_pos + result_total_size,
              stack->data + start_remove_pos,
              bytes_after);
    }
    memcpy(stack->data + start_remove_pos, temp_result, result_total_size);
    stack->size += result_total_size;
    
    // Cleanup
    free(temp_result);
    for (int i = 0; i < 2; i++) {
      if (operand_values[i].data) free(operand_values[i].data);
    }
    if (result_item.data) free(result_item.data);
  }
}
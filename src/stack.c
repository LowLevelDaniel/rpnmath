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

void rpnmath_stack_init(rpnmath_stack_t *stack, size_t sizehint) {
  stack->data = malloc(sizehint);
  if (!stack->data) {
    fprintf(stderr, "Failed to allocate stack memory\n");
    exit(1);
  }
  stack->size = 0;
  stack->capacity = sizehint;
}

void rpnmath_stack_cleanup(rpnmath_stack_t *stack) {
  if (stack->data) {
    free(stack->data);
    stack->data = NULL;
  }
  stack->size = 0;
  stack->capacity = 0;
}

int rpnmath_stack_isempty(rpnmath_stack_t *stack) {
  return stack->size == 0;
}

// Helper function to ensure stack has enough space
static void rpnmath_stack_ensure_space(rpnmath_stack_t *stack, size_t needed);
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

void rpnmath_stack_pushbo(rpnmath_stack_t *stack, rpnmath_item_binop_t *item) {
  size_t item_size = sizeof(rpnmath_item_binop_t);
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
    } else if (kind == RPNMATH_ITEMKIND_BINOP) {
      pos += sizeof(rpnmath_item_binop_t);
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
    } else if (kind == RPNMATH_ITEMKIND_BINOP) {
      pos += sizeof(rpnmath_item_binop_t);
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

rpnmath_item_binop_t rpnmath_stack_popbo(rpnmath_stack_t *stack) {
  rpnmath_item_binop_t empty_item = {0};
  empty_item.kind = RPNMATH_ITEMKIND_VOID;
  
  if (rpnmath_stack_isempty(stack)) {
    return empty_item;
  }
  
  // Find the last binop item and remove it
  size_t pos = 0;
  size_t last_binop_pos = 0;
  size_t last_binop_size = 0;
  int found_binop = 0;
  
  while (pos < stack->size) {
    if (pos + sizeof(rpnmath_itemkind_t) > stack->size) break;
    
    rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + pos);
    
    if (kind == RPNMATH_ITEMKIND_CONST) {
      rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + pos);
      pos += sizeof(rpnmath_item_const_t) + item->size;
    } else if (kind == RPNMATH_ITEMKIND_BINOP) {
      last_binop_pos = pos;
      last_binop_size = sizeof(rpnmath_item_binop_t);
      found_binop = 1;
      pos += last_binop_size;
    } else {
      pos += sizeof(rpnmath_itemkind_t);
    }
  }
  
  if (!found_binop) {
    return empty_item;
  }
  
  // Copy the binop item
  rpnmath_item_binop_t result = *(rpnmath_item_binop_t*)(stack->data + last_binop_pos);
  
  // Remove the item from stack
  size_t bytes_after = stack->size - (last_binop_pos + last_binop_size);
  if (bytes_after > 0) {
    memmove(stack->data + last_binop_pos, 
            stack->data + last_binop_pos + last_binop_size, 
            bytes_after);
  }
  stack->size -= last_binop_size;
  
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
    } else if (kind == RPNMATH_ITEMKIND_BINOP) {
      pos += sizeof(rpnmath_item_binop_t);
    } else {
      pos += sizeof(rpnmath_itemkind_t);
    }
  }
  
  return count;
}

void rpnmath_stack_execute(rpnmath_stack_t *stack) {
  // Process the entire stack until no more operators remain
  while (1) {
    // Find the first operator in the stack (left to right scan)
    size_t pos = 0;
    size_t operator_pos = 0;
    size_t operator_size = 0;
    rpnmath_binop_t operation;
    int found_operator = 0;
    
    while (pos < stack->size) {
      if (pos + sizeof(rpnmath_itemkind_t) > stack->size) break;
      
      rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + pos);
      
      if (kind == RPNMATH_ITEMKIND_CONST) {
        rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + pos);
        pos += sizeof(rpnmath_item_const_t) + item->size;
      } else if (kind == RPNMATH_ITEMKIND_BINOP) {
        rpnmath_item_binop_t *binop = (rpnmath_item_binop_t*)(stack->data + pos);
        operator_pos = pos;
        operator_size = sizeof(rpnmath_item_binop_t);
        operation = binop->operation;
        found_operator = 1;
        break;
      } else {
        pos += sizeof(rpnmath_itemkind_t);
      }
    }
    
    // If no operators found, we're done
    if (!found_operator) {
      break;
    }
    
    // Count constants before this operator
    int constants_before = 0;
    size_t scan_pos = 0;
    while (scan_pos < operator_pos) {
      if (scan_pos + sizeof(rpnmath_itemkind_t) > operator_pos) break;
      
      rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + scan_pos);
      
      if (kind == RPNMATH_ITEMKIND_CONST) {
        constants_before++;
        rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + scan_pos);
        scan_pos += sizeof(rpnmath_item_const_t) + item->size;
      } else if (kind == RPNMATH_ITEMKIND_BINOP) {
        scan_pos += sizeof(rpnmath_item_binop_t);
      } else {
        scan_pos += sizeof(rpnmath_itemkind_t);
      }
    }
    
    // Check if we have enough operands
    if (constants_before < 2) {
      fprintf(stderr, "Error: Not enough operands for binary operation\n");
      return;
    }
    
    // Find the positions of the last two constants before the operator
    size_t const_positions[2];
    size_t const_sizes[2];
    rpnmath_item_const_t const_items[2];
    int const_found = 0;
    
    scan_pos = 0;
    while (scan_pos < operator_pos && const_found < 2) {
      if (scan_pos + sizeof(rpnmath_itemkind_t) > operator_pos) break;
      
      rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + scan_pos);
      
      if (kind == RPNMATH_ITEMKIND_CONST) {
        rpnmath_item_const_t *item = (rpnmath_item_const_t*)(stack->data + scan_pos);
        
        // Store the last two constants (shift array to keep most recent)
        if (const_found == 2) {
          const_positions[0] = const_positions[1];
          const_sizes[0] = const_sizes[1];
          const_items[0] = const_items[1];
        }
        
        int idx = const_found < 2 ? const_found : 1;
        const_positions[idx] = scan_pos;
        const_sizes[idx] = sizeof(rpnmath_item_const_t) + item->size;
        const_items[idx] = *item;
        
        // Copy the data
        const_items[idx].data = malloc(item->size);
        memcpy(const_items[idx].data, stack->data + scan_pos + sizeof(rpnmath_item_const_t), item->size);
        
        if (const_found < 2) const_found++;
        
        scan_pos += sizeof(rpnmath_item_const_t) + item->size;
      } else if (kind == RPNMATH_ITEMKIND_BINOP) {
        scan_pos += sizeof(rpnmath_item_binop_t);
      } else {
        scan_pos += sizeof(rpnmath_itemkind_t);
      }
    }
    
    // Extract values and perform operation
    long long left_val = rpnmath_get_int_value(&const_items[0]);
    long long right_val = rpnmath_get_int_value(&const_items[1]);
    long long result_val = 0;
    
    // Determine result bit width (promote to larger type)
    size_t result_bitwidth = const_items[0].type.size > const_items[1].type.size ? 
                            const_items[0].type.size : const_items[1].type.size;
    
    // Perform operation with overflow checking
    switch (operation) {
      case RPNMATH_BINOP_ADD:
        if (rpnmath_type_would_overflow_add(left_val, right_val)) {
          fprintf(stderr, "Warning: Addition overflow detected, promoting type\n");
          result_bitwidth = result_bitwidth < 64 ? result_bitwidth * 2 : 64;
          if (result_bitwidth > 64) {
            printf("TODO: Support for integers over 64 bits not implemented\n");
            free(const_items[0].data);
            free(const_items[1].data);
            abort();
          }
        }
        result_val = left_val + right_val;
        break;
        
      case RPNMATH_BINOP_SUB:
        if (rpnmath_type_would_overflow_sub(left_val, right_val)) {
          fprintf(stderr, "Warning: Subtraction overflow detected, promoting type\n");
          result_bitwidth = result_bitwidth < 64 ? result_bitwidth * 2 : 64;
          if (result_bitwidth > 64) {
            printf("TODO: Support for integers over 64 bits not implemented\n");
            free(const_items[0].data);
            free(const_items[1].data);
            abort();
          }
        }
        result_val = left_val - right_val;
        break;
        
      case RPNMATH_BINOP_MUL:
        if (rpnmath_type_would_overflow_mul(left_val, right_val)) {
          fprintf(stderr, "Warning: Multiplication overflow detected, promoting type\n");
          result_bitwidth = result_bitwidth < 64 ? result_bitwidth * 2 : 64;
          if (result_bitwidth > 64) {
            printf("TODO: Support for integers over 64 bits not implemented\n");
            free(const_items[0].data);
            free(const_items[1].data);
            abort();
          }
        }
        result_val = left_val * right_val;
        break;
        
      case RPNMATH_BINOP_DIV:
        if (right_val == 0) {
          fprintf(stderr, "Error: Division by zero\n");
          free(const_items[0].data);
          free(const_items[1].data);
          return;
        }
        result_val = left_val / right_val;
        break;
        
      default:
        fprintf(stderr, "Error: Unknown operation\n");
        free(const_items[0].data);
        free(const_items[1].data);
        return;
    }
    
    // Create result item
    rpnmath_item_const_t result_item = rpnmath_create_int_const(result_val, result_bitwidth);
    
    // Calculate total size to remove (2 constants + 1 operator)
    size_t total_remove_size = const_sizes[0] + const_sizes[1] + operator_size;
    size_t start_remove_pos = const_positions[0];
    
    // Replace the operands and operator with the result
    // First, copy the result to a temporary location
    size_t result_total_size = sizeof(rpnmath_item_const_t) + result_item.size;
    char *temp_result = malloc(result_total_size);
    memcpy(temp_result, &result_item, sizeof(rpnmath_item_const_t));
    memcpy(temp_result + sizeof(rpnmath_item_const_t), result_item.data, result_item.size);
    
    // Remove the old items by shifting everything after them
    size_t bytes_after = stack->size - (start_remove_pos + total_remove_size);
    if (bytes_after > 0) {
      memmove(stack->data + start_remove_pos,
              stack->data + start_remove_pos + total_remove_size,
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
    free(const_items[0].data);
    free(const_items[1].data);
    free(result_item.data);
  }
}
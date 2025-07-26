#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "type.h"
#include "item.h"
#include "stack.h"

void rpnmath_stack_init(rpnmath_stack_t *stack, size_t sizehint) {
  stack->data = malloc(sizehint);
  if (!stack->data) {
    fprintf(stderr, "Failed to allocate stack memory\n");
    exit(1);
  }
  stack->top = -1;
  stack->size = 0;
  stack->capacity = sizehint;
}


// Helper function to ensure stack has enough space
static void rpnmath_stack_ensure_space(rpnmath_stack_t *stack, size_t needed) {
  if (stack->size + needed > stack->capacity) {
    size_t new_size = stack->capacity * 2;
    if (new_size < required) new_size = required;
    
    stack->data = realloc(stack->data, new_size);
    if (!stack->data) {
      fprintf(stderr, "Failed to expand stack memory\n");
      exit(1);
    }
    stack->capacity = new_size;
  }
}

void rpnmath_stack_pushc(rpnmath_stack_t *stack, rpnmath_item_const_t *item) {
  size_t item_size = sizeof(rpnmath_item_const_t) + item->size;
  rpnmath_stack_ensure_space(stack, item_size);
  
  // Copy the struct header
  memcpy(stack->data + stack->size + 1, item, item_size);
  stack->size += item_size;
  
  // Copy the data
  memcpy(stack->data + stack->size + 1, item->data, item->size);
  stack->size += item->size;
}

void rpnmath_stack_pushbo(rpnmath_stack_t *stack, rpnmath_item_binop_t *item) {
  size_t item_size = sizeof(rpnmath_item_binop_t);
  rpnmath_stack_ensure_space(stack, item_size);
  
  memcpy(stack->data + stack->size + 1, item, item_size);
  stack->size += item_size;
}

rpnmath_itemkind_t rpnmath_stack_peekk(rpnmath_stack_t *stack) {
  size_t top = stack->top;
  top += sizeof(rpnmath_itemkind_t); 
  if (top > size) {
    return RPNMATH_ITEMKIND_VOID;
  }
  rpnmath_itemkind_t kind = *(rpnmath_itemkind_t*)(stack->data + top - sizeof(rpnmath_itemkind_t));
  return kind;
}

rpnmath_item_const_t rpnmath_stack_popc(rpnmath_stack_t *stack) {

}

rpnmath_item_const_t rpnmath_stack_popbo(rpnmath_stack_t *stack) {

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
  
  switch (native_size) {
    case 1: *(in

// Helper function to create const item from integer value
static rpnmath_item_const_t rpnmath_create_int_const(long long value, size_t bitwidth) {
  rpnmath_item_const_t item = {0};
  item.kind = RPNMATH_ITEMKIND_CONST;
  rpnmath_type_int(&item.type, bitwidth);
  
  size_t native_size = rpnmath_type_native_size(bitwidth);
  item.size = native_size;
  item.data = malloc(native_size);
  
  switch (native_size) {
    case 1: *(int8_t*)item.data = (int8_t)value; break;
    case 2: *(int16_t*)item.data = (int16_t)value; break;
    case 4: *(int32_t*)item.data = (int32_t)value; break;
    case 8: *(int64_t*)item.data = (int64_t)value; break;
    default:
      printf("TODO: Support for integers over 64 bits not implemented\n");
      abort();
  }
  
  return item;
}

void rpnmath_stack_execute(rpnmath_stack_t *stack) {
  // Check if we have at least one binary operation and two operands
  int const_count = 0;
  int binop_count = 0;
  
  // Count items on stack (simplified check)
  if (rpnmath_stack_isempty(stack)) {
    return;
  }
  
  // For RPN, we need at least 2 constants and 1 binary operation
  // This is a simplified implementation
  
  // Pop binary operation
  rpnmath_item_const_t binop_item = rpnmath_stack_popbo(stack);
  if (binop_item.kind != RPNMATH_ITEMKIND_BINOP) {
    if (binop_item.data) free(binop_item.data);
    return;
  }
  
  rpnmath_binop_t operation = *(rpnmath_binop_t*)binop_item.data;
  free(binop_item.data);
  
  // Pop two operands (right operand first in RPN)
  rpnmath_item_const_t right_item = rpnmath_stack_popc(stack);
  rpnmath_item_const_t left_item = rpnmath_stack_popc(stack);
  
  if (right_item.kind != RPNMATH_ITEMKIND_CONST || left_item.kind != RPNMATH_ITEMKIND_CONST) {
    fprintf(stderr, "Error: Invalid operands for operation\n");
    if (right_item.data) free(right_item.data);
    if (left_item.data) free(left_item.data);
    return;
  }
  
  long long left_val = rpnmath_get_int_value(&left_item);
  long long right_val = rpnmath_get_int_value(&right_item);
  long long result_val = 0;
  
  // Determine result bit width (promote to larger type)
  size_t result_bitwidth = left_item.type.size > right_item.type.size ? 
                          left_item.type.size : right_item.type.size;
  
  // Perform operation with overflow checking
  switch (operation) {
    case RPNMATH_BINOP_ADD:
      if (rpnmath_type_would_overflow_add(left_val, right_val)) {
        fprintf(stderr, "Error: Addition overflow\n");
        result_bitwidth = result_bitwidth < 64 ? result_bitwidth * 2 : 64;
        if (result_bitwidth > 64) {
          printf("TODO: Support for integers over 64 bits not implemented\n");
          abort();
        }
      }
      result_val = left_val + right_val;
      break;
      
    case RPNMATH_BINOP_SUB:
      if (rpnmath_type_would_overflow_sub(left_val, right_val)) {
        fprintf(stderr, "Error: Subtraction overflow\n");
        result_bitwidth = result_bitwidth < 64 ? result_bitwidth * 2 : 64;
        if (result_bitwidth > 64) {
          printf("TODO: Support for integers over 64 bits not implemented\n");
          abort();
        }
      }
      result_val = left_val - right_val;
      break;
      
    case RPNMATH_BINOP_MUL:
      if (rpnmath_type_would_overflow_mul(left_val, right_val)) {
        fprintf(stderr, "Error: Multiplication overflow\n");
        result_bitwidth = result_bitwidth < 64 ? result_bitwidth * 2 : 64;
        if (result_bitwidth > 64) {
          printf("TODO: Support for integers over 64 bits not implemented\n");
          abort();
        }
      }
      result_val = left_val * right_val;
      break;
      
    case RPNMATH_BINOP_DIV:
      if (right_val == 0) {
        fprintf(stderr, "Error: Division by zero\n");
        free(right_item.data);
        free(left_item.data);
        return;
      }
      result_val = left_val / right_val;
      break;
      
    default:
      fprintf(stderr, "Error: Unknown operation\n");
      free(right_item.data);
      free(left_item.data);
      return;
  }
  
  // Create result item and push back to stack
  rpnmath_item_const_t result_item = rpnmath_create_int_const(result_val, result_bitwidth);
  rpnmath_stack_pushc(stack, &result_item);
  
  // Cleanup
  free(right_item.data);
  free(left_item.data);
  free(result_item.data);
}
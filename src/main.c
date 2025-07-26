#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>
#include "type.h"
#include "item.h"
#include "stack.h"

// Helper function to determine if a string is a number
int is_number(const char *str) {
  if (!str || *str == '\0') return 0;
  
  char *endptr;
  strtoll(str, &endptr, 10);
  return *endptr == '\0';
}

// Helper function to determine if a string is an operator
int is_operator(const char *str) {
  return (strlen(str) == 1 && 
          (*str == '+' || *str == '-' || *str == '*' || *str == '/'));
}

// Helper function to get operation enum from string
rpnmath_binop_t get_operation(const char *str) {
  switch (*str) {
    case '+': return RPNMATH_BINOP_ADD;
    case '-': return RPNMATH_BINOP_SUB;
    case '*': return RPNMATH_BINOP_MUL;
    case '/': return RPNMATH_BINOP_DIV;
    default: return RPNMATH_BINOP_ADD; // fallback
  }
}

// Helper function to determine appropriate bit width for a number
size_t determine_bitwidth(long long value) {
  if (value >= SCHAR_MIN && value <= SCHAR_MAX) return 8;
  if (value >= SHRT_MIN && value <= SHRT_MAX) return 16;
  if (value >= INT_MIN && value <= INT_MAX) return 32;
  return 64;
}

// Helper function to create and push a constant to stack
void push_number(rpnmath_stack_t *stack, long long value) {
  rpnmath_item_const_t item = {0};
  item.kind = RPNMATH_ITEMKIND_CONST;
  
  size_t bitwidth = determine_bitwidth(value);
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
  
  rpnmath_stack_pushc(stack, &item);
  free(item.data);
}

// Helper function to create and push an operator to stack
void push_operator(rpnmath_stack_t *stack, rpnmath_binop_t operation) {
  rpnmath_item_binop_t item = {0};
  item.kind = RPNMATH_ITEMKIND_BINOP;
  item.operation = operation;
  
  rpnmath_stack_pushbo(stack, &item);
}

// Helper function to get the top value from stack for result
long long get_result(rpnmath_stack_t *stack) {
  if (rpnmath_stack_isempty(stack)) {
    return 0;
  }
  
  rpnmath_itemkind_t kind = rpnmath_stack_peekk(stack);
  if (kind != RPNMATH_ITEMKIND_CONST) {
    return 0;
  }
  
  rpnmath_item_const_t result_item = rpnmath_stack_popc(stack);
  if (result_item.kind != RPNMATH_ITEMKIND_CONST) {
    if (result_item.data) free(result_item.data);
    return 0;
  }
  
  size_t native_size = rpnmath_type_native_size(result_item.type.size);
  long long value = 0;
  
  switch (native_size) {
    case 1: value = *(int8_t*)result_item.data; break;
    case 2: value = *(int16_t*)result_item.data; break;
    case 4: value = *(int32_t*)result_item.data; break;
    case 8: value = *(int64_t*)result_item.data; break;
    default:
      printf("TODO: Support for integers over 64 bits not implemented\n");
      if (result_item.data) free(result_item.data);
      abort();
  }
  
  if (result_item.data) free(result_item.data);
  return value;
}

int main() {
  char expression[1000];
  
  printf("Reverse Polish Notation Calculator\n");
  printf("==================================\n");
  printf("Supported operators: +, -, *, /\n");
  printf("Example: \"3 4 +\" calculates 3 + 4 = 7\n");
  printf("Example: \"15 7 1 1 + - / 3 * 2 1 1 + + -\" calculates a complex expression\n");
  printf("Enter 'quit' to exit\n\n");
  
  while (1) {
    printf("RPN> ");
    
    if (!fgets(expression, sizeof(expression), stdin)) {
      break;
    }
    
    // Remove newline character
    expression[strcspn(expression, "\n")] = '\0';
    
    if (strcmp(expression, "quit") == 0) {
      break;
    }
    
    if (strlen(expression) == 0) {
      continue;
    }
    
    // Construct Stack
    rpnmath_stack_t stack;
    rpnmath_stack_init(&stack, 1024);
    
    // Parse expression and build stack
    char *expression_copy = malloc(strlen(expression) + 1);
    strcpy(expression_copy, expression);
    
    char *token = strtok(expression_copy, " \t");
    int error = 0;
    
    while (token != NULL && !error) {
      if (is_number(token)) {
        char *endptr;
        long long value = strtoll(token, &endptr, 10);
        
        if (*endptr != '\0') {
          printf("Error: Invalid number '%s'\n", token);
          error = 1;
          break;
        }
        
        push_number(&stack, value);
        printf("  Pushed number: %lld\n", value);
        
      } else if (is_operator(token)) {
        rpnmath_binop_t operation = get_operation(token);
        
        push_operator(&stack, operation);
        printf("  Pushed operator: %s\n", token);
        
      } else {
        printf("Error: Unknown token '%s'\n", token);
        error = 1;
        break;
      }
      
      token = strtok(NULL, " \t");
    }
    
    if (!error) {
      // Execute the entire RPN expression
      printf("  Executing RPN expression...\n");
      rpnmath_stack_execute(&stack);
      
      // Check final stack state - should have exactly one constant
      if (rpnmath_stack_isempty(&stack)) {
        printf("Error: No result on stack after execution\n\n");
      } else {
        rpnmath_itemkind_t kind = rpnmath_stack_peekk(&stack);
        if (kind == RPNMATH_ITEMKIND_CONST) {
          int remaining_constants = rpnmath_stack_count_constants(&stack);
          if (remaining_constants == 1) {
            long long result = get_result(&stack);
            printf("Result: %lld\n\n", result);
          } else {
            printf("Error: %d values remaining on stack\n", remaining_constants);
            printf("This suggests an incomplete or invalid RPN expression\n\n");
          }
        } else {
          printf("Error: Final result is not a constant value\n\n");
        }
      }
    } else {
      printf("\n");
    }
    
    // Clean up
    free(expression_copy);
    rpnmath_stack_cleanup(&stack);
  }
  
  printf("Goodbye!\n");
  return 0;
}
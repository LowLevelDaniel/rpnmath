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

// Helper function to determine if a string is an operation
int is_operation(const char *str) {
  if (strlen(str) != 1) return 0;
  return (*str == '+' || *str == '-' || *str == '*' || *str == '/' || 
          *str == '=' || *str == '.');
}

// Helper function to determine if a string is a variable reference ($0, $1, etc.)
int is_variable(const char *str) {
  if (!str || strlen(str) < 2 || str[0] != '$') return 0;
  
  // Check if everything after $ is digits
  for (size_t i = 1; i < strlen(str); i++) {
    if (!isdigit(str[i])) return 0;
  }
  
  return 1;
}

// Helper function to get operation enum from string
rpnmath_op_t get_operation(const char *str) {
  switch (*str) {
    case '+': return RPNMATH_OP_ADD;
    case '-': return RPNMATH_OP_SUB;
    case '*': return RPNMATH_OP_MUL;
    case '/': return RPNMATH_OP_DIV;
    case '=': return RPNMATH_OP_ASSIGN;
    case '.': return RPNMATH_OP_RETURN;
    default: return RPNMATH_OP_ADD; // fallback
  }
}

// Helper function to get variable ID from string ($0 -> 0, $1 -> 1, etc.)
size_t get_variable_id(const char *str) {
  return (size_t)strtoul(str + 1, NULL, 10); // skip the '$' character
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

// Helper function to create and push an operation to stack
void push_operation(rpnmath_stack_t *stack, rpnmath_op_t operation) {
  rpnmath_item_op_t item = {0};
  item.kind = RPNMATH_ITEMKIND_OP;
  item.operation = operation;
  
  rpnmath_stack_pushop(stack, &item);
}

// Helper function to create and push a variable reference to stack
void push_variable(rpnmath_stack_t *stack, size_t var_id) {
  rpnmath_item_varref_t item = {0};
  item.kind = RPNMATH_ITEMKIND_VARREF;
  item.variable_id = var_id;
  
  rpnmath_stack_pushvr(stack, &item);
}

// Helper function to get the result value from a const item
long long get_result_value(rpnmath_item_const_t *result_item) {
  if (result_item->kind != RPNMATH_ITEMKIND_CONST) {
    return 0;
  }
  
  size_t native_size = rpnmath_type_native_size(result_item->type.size);
  long long value = 0;
  
  switch (native_size) {
    case 1: value = *(int8_t*)result_item->data; break;
    case 2: value = *(int16_t*)result_item->data; break;
    case 4: value = *(int32_t*)result_item->data; break;
    case 8: value = *(int64_t*)result_item->data; break;
    default:
      printf("TODO: Support for integers over 64 bits not implemented\n");
      abort();
  }
  
  return value;
}

int main() {
  char expression[1000];
  
  printf("RPN Calculator with SSA Variables\n");
  printf("=================================\n");
  printf("Supported operators: +, -, *, /\n");
  printf("Variables: $0, $1, $2, ... (single static assignment)\n");
  printf("Assignment: = (assigns top stack value to variable)\n");
  printf("Return: . (returns top stack value and stops execution)\n");
  printf("Example: \"10 $0 = 20 $0 + .\" assigns 10 to $0, then returns $0 + 20\n");
  printf("Example: \"5 $0 = 3 $1 = $0 $1 * .\" assigns 5 to $0, 3 to $1, returns $0 * $1\n");
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
        
      } else if (is_variable(token)) {
        size_t var_id = get_variable_id(token);
        
        if (var_id >= RPNMATH_MAX_VARIABLES) {
          printf("Error: Variable ID %zu exceeds maximum %d\n", var_id, RPNMATH_MAX_VARIABLES - 1);
          error = 1;
          break;
        }
        
        push_variable(&stack, var_id);
        printf("  Pushed variable reference: $%zu\n", var_id);
        
      } else if (is_operation(token)) {
        rpnmath_op_t operation = get_operation(token);
        
        push_operation(&stack, operation);
        printf("  Pushed operation: %s (%s)\n", token, rpnmath_op_name(operation));
        
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
      rpnmath_item_const_t result;
      int exec_result = rpnmath_stack_execute(&stack, &result);
      
      if (exec_result == 0) {
        long long result_value = get_result_value(&result);
        printf("Result: %lld\n\n", result_value);
        
        // Clean up result data
        if (result.data) {
          free(result.data);
        }
      } else {
        printf("Error: Execution failed\n\n");
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
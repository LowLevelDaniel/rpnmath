#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

int main() {
  char expression[1000];
  
  printf("Reverse Polish Notation Calculator\n");
  printf("==================================\n");
  printf("Supported operators: +, -, *, /, ^\n");
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


    // Parse Stack

    
    // Print Result
    printf("Result: %.6g\n\n", result);
  }
  
  printf("Goodbye!\n");
  return 0;
}
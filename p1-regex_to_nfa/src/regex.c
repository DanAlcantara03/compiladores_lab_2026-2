#include <stddef.h>   // size_t
#include <stdio.h>    // FILE 
#include <stdbool.h> // bool


/* --- Local Helpers to do the implementation easier --- */



/* --- Helpers in regex.h (useful for organization) --- */


regex from_implicit_to_explicit_concat(const regex *infix_implicit){

}


regex regex_explicit_infix_to_postfix(const regex *infix_explicit){

}


void regex_print(regex r, FILE *out){
    
}

/* --- Principal Functions --- */

regex create_regex(const char *regex_expression){

}


void regex_free(regex *r){

}


regex parse_regex(const regex *infix_implicit){

}
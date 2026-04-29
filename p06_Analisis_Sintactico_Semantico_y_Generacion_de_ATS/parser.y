%{
#include "ast.h"
#include <iostream>
#include <string>

extern int yylex();
extern int yylineno;
void yyerror(const char *s);

Node* root; // Raíz del AST
%}

%union {
    int ival;
    char* sval;
    class Node* nptr;
}

%token <sval> ID INT_LIT FLOAT_LIT STRING_LIT
%token QUACKINT QUACKLE QUACKS POND QUACK_RETURN
%token IF ELSIF ELSE WHILE FOR
%token PATO_START PATO_END
%token PLUS MINUS MULT DIV MOD EQ NEQ GT LT
%token ASSIGN SEMI COMMA LPAREN RPAREN

/* Tipos de nodos */
%type <nptr> program mixed_list mixed_item function_definition 
%type <nptr> parameter_list parameter type declaration statement_list 
%type <nptr> statement expression if_statement block 
%type <nptr> elsif_list argument_list for_loop assignment

/* Precedencia */
%left PATO_START
%left EQ NEQ GT LT
%left PLUS MINUS
%left MULT DIV MOD
%right ASSIGN

%%

program:
    mixed_list { root = $1; }
    ;

mixed_list:
    mixed_item { 
        $$ = new Node("Program"); 
        $$->add_child($1); 
    }
    | mixed_list mixed_item { 
        $1->add_child($2); 
        $$ = $1; 
    }
    ;

mixed_item:
    function_definition { $$ = $1; }
    | statement { $$ = $1; }
    ;

function_definition:
    POND ID LPAREN parameter_list RPAREN block {
        $$ = new Node("Function", "pond " + std::string($2));
        if ($4) $$->add_child($4);
        $$->add_child($6);
    }
    ;

parameter_list:
    /* empty */ { $$ = nullptr; }
    | parameter { 
        $$ = new Node("Parameters");
        $$->add_child($1); 
    }
    | parameter_list COMMA parameter {
        if ($1) {
            $1->add_child($3);
            $$ = $1;
        } else {
            $$ = new Node("Parameters");
            $$->add_child($3);
        }
    }
    ;

parameter:
    type ID {
        $$ = new Node("Parameter", std::string($2));
        $$->add_child($1);
    }
    ;

type:
    QUACKINT { $$ = new Node("Type", "quackint"); }
    | QUACKLE { $$ = new Node("Type", "quackle"); }
    | QUACKS  { $$ = new Node("Type", "quacks"); }
    | POND    { $$ = new Node("Type", "pond"); }
    ;

statement_list:
    statement { 
        $$ = new Node("Statement List"); 
        $$->add_child($1); 
    }
    | statement_list statement { 
        $1->add_child($2); 
        $$ = $1; 
    }
    ;

statement:
    declaration SEMI { $$ = $1; }
    | assignment SEMI { $$ = $1; }
    | expression SEMI { $$ = $1; }
    | if_statement { $$ = $1; }
    | WHILE LPAREN expression RPAREN block {
        $$ = new Node("Loop", "quack!");
        $$->add_child($3);
        $$->add_child($5);
    }
    | for_loop { $$ = $1; }
    | QUACK_RETURN expression SEMI {
        $$ = new Node("Return", "quack_return");
        $$->add_child($2);
    }
    ;

declaration:
    type ID ASSIGN expression {
        $$ = new Node("Declaration", std::string($2));
        $$->add_child($1);
        $$->add_child($4);
    }
    | type ID {
        $$ = new Node("Declaration", std::string($2));
        $$->add_child($1);
    }
    ;

assignment:
    ID ASSIGN expression {
        $$ = new Node("Assignment", std::string($1));
        $$->add_child($3);
    }
    ;

if_statement:
    IF LPAREN expression RPAREN block elsif_list {
        $$ = new Node("Conditional", "quack?");
        $$->add_child($3);
        $$->add_child($5);
        if ($6) $$->add_child($6);
    }
    | IF LPAREN expression RPAREN block elsif_list ELSE block {
        $$ = new Node("Conditional", "quack?");
        $$->add_child($3);
        $$->add_child($5);
        if ($6) $$->add_child($6);
        Node* else_node = new Node("Else", "quack???");
        else_node->add_child($8);
        $$->add_child(else_node);
    }
    ;

elsif_list:
    /* empty */ { $$ = nullptr; }
    | elsif_list ELSIF LPAREN expression RPAREN block {
        Node* elsif_node = new Node("Else-If", "quack??");
        elsif_node->add_child($4);
        elsif_node->add_child($6);
        if ($1) {
            $1->add_child(elsif_node);
            $$ = $1;
        } else {
            $$ = new Node("Else-If Branches");
            $$->add_child(elsif_node);
        }
    }
    ;

for_loop:
    FOR LPAREN assignment SEMI expression SEMI assignment RPAREN block {
        $$ = new Node("Loop", "qu@ck");
        Node* header = new Node("FOR Header");
        header->add_child($3);
        header->add_child($5);
        header->add_child($7);
        $$->add_child(header);
        $$->add_child($9);
    }
    ;

block:
    PATO_START statement_list PATO_END { $$ = $2; }
    | PATO_START PATO_END { $$ = new Node("Empty Block"); }
    ;

expression:
    INT_LIT { $$ = new Node("Integer", $1); }
    | FLOAT_LIT { $$ = new Node("Float", $1); }
    | STRING_LIT { $$ = new Node("String", $1); }
    | ID { $$ = new Node("Identifier", $1); }
    | ID LPAREN argument_list RPAREN {
        $$ = new Node("Call", std::string($1));
        if ($3) $$->add_child($3);
    }
    | expression PLUS expression {
        $$ = new Node("Operation", "(+)>");
        $$->add_child($1);
        $$->add_child($3);
    }
    | expression MINUS expression {
        $$ = new Node("Operation", "(-)>");
        $$->add_child($1);
        $$->add_child($3);
    }
    | expression MULT expression {
        $$ = new Node("Operation", "(*)>");
        $$->add_child($1);
        $$->add_child($3);
    }
    | expression DIV expression {
        $$ = new Node("Operation", "(/)>");
        $$->add_child($1);
        $$->add_child($3);
    }
    | expression MOD expression {
        $$ = new Node("Operation", "(%)>");
        $$->add_child($1);
        $$->add_child($3);
    }
    | expression PATO_START expression {
        $$ = new Node("Operation", "(^)>");
        $$->add_child($1);
        $$->add_child($3);
    }
    | expression EQ expression {
        $$ = new Node("Comparison", "(==)>");
        $$->add_child($1);
        $$->add_child($3);
    }
    | expression NEQ expression {
        $$ = new Node("Comparison", "(!=)>");
        $$->add_child($1);
        $$->add_child($3);
    }
    | expression GT expression {
        $$ = new Node("Comparison", "(>)>");
        $$->add_child($1);
        $$->add_child($3);
    }
    | expression LT expression {
        $$ = new Node("Comparison", "(<)>");
        $$->add_child($1);
        $$->add_child($3);
    }
    | LPAREN expression RPAREN { $$ = $2; }
    ;

argument_list:
    /* empty */ { $$ = nullptr; }
    | expression { 
        $$ = new Node("Arguments");
        $$->add_child($1); 
    }
    | argument_list COMMA expression {
        if ($1) {
            $1->add_child($3);
            $$ = $1;
        } else {
            $$ = new Node("Arguments");
            $$->add_child($3);
        }
    }
    ;

%%

void yyerror(const char *s) {
    std::cerr << "Quack-astrophic Error: " << s << " at line " << yylineno << std::endl;
}

int main() {
    std::cout << "--- Starting Quackier++ Parser ---" << std::endl;
    if (yyparse() == 0) {
        std::cout << "\n--- Analysis Success. Pond Structure (AST): ---" << std::endl;
        if (root) {
            root->print();
            root->gen_dot("ast.dot");
        }
    }
    return 0;
}
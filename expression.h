//
// Created by Olivier L.-Applin on 2019-01-25.
//

#ifndef TESTSHELL_EXPRESSION_H
#define TESTSHELL_EXPRESSION_H

#define EXIT_SHELL "exit"
#define LS "ls"
#define ECHO "echo"

#define IF_TOKEN "if"
#define DO_TOKEN "do"
#define DONE_TOKEN "done"
#define OR_TOKEN "||"
#define AND_TOKEN "&&"
#define IF_SEP ";"

#define FAIL 'F'
#define SUCCESS 'S'

#define INVALID_INDEX -1

#define background_task_token "&"


typedef enum {COMMAND, IF_EXPRESSION, OR_EXPRESSION, AND_EXPRESSION} exp_type;
typedef struct {
    char*                   cmd_name;
    char**                  args;
    int                     arg_size;
    char**                  options;
    int                     options_size;
} command;

/**
 * Abstract syntax tree representation for parsing the command line.
 * Inspired by : https://lambda.uta.edu/cse5317/notes/node26.html
 */
typedef struct Expression {
    exp_type                                            id;
    union {
        command*                                        cmd_expr;
        struct {
            struct Expression*      condition;
            struct Expression*      statement;
        }                                               if_expr;
        struct {
            struct Expression*      left_statement;
            struct Expression*      right_statement;
        }                                               or_expr;
        struct {
            struct Expression*      left_statement;
            struct Expression*      right_statement;
        }                                               and_expr;
    } node;
} Expression;


Expression* create_exp      (exp_type type, Expression* first, Expression* second);
int eval                    (Expression* exp);
int or_eval                 (Expression* exp);
int and_eval                (Expression* exp);
Expression* create_cmd      (char** line, int start_index, int end_index);
int run_command             (command cmd);


/**
 * To allocate memory for an Expression
 * @param type
 * @param first
 * @param second
 * @return
 */
Expression * create_exp (exp_type type, Expression* first, Expression* second){

    Expression* e = malloc(sizeof(Expression));
    if (e == NULL){
        perror("malloc error create_exp");
        return NULL;
    }

    exp_type* type_pointer = malloc(sizeof(exp_type));
    if (type_pointer == NULL){
        perror("malloc error type_pointer");
        free(e);
        return NULL;
    }

    *type_pointer = type;
    e->id = *type_pointer;

    if (type == IF_EXPRESSION){
        e->node.if_expr.condition = first;
        e->node.if_expr.statement = second;
    }

    if (type == OR_EXPRESSION){
        e->node.or_expr.left_statement = first;
        e->node.or_expr.right_statement = second;
    }

    if (type == AND_EXPRESSION){
        e->node.and_expr.left_statement = first;
        e->node.and_expr.right_statement = second;
    }

    return e;
}


/**
 *
 * @param line
 * @return
 */
Expression * create_cmd (char** line, int start_index, int end_index){

    int size = end_index - start_index + 1;
    if (strncmp(line[start_index + size - 1], IF_SEP, 1) == 0){
        size--;
    }

    Expression* e = malloc(sizeof(Expression));
    if (e == NULL){
        perror("malloc error create_cmd");
        return NULL;
    }
    e->id = COMMAND;

    e->node.cmd_expr = malloc(sizeof(command));
    if (e->node.cmd_expr == NULL){
        free(e);
        perror("malloc error create_cmd");
        return NULL;
    }

    e->node.cmd_expr->cmd_name = line[start_index];

    // todo : split between arguments and options
    if (size == 1){
        e->node.cmd_expr->args = NULL;
        e->node.cmd_expr->options = NULL;
    } else {
        for (int i = start_index; i <= end_index; ++i) {
            printf("%s", line[i]);
        }
        printf("\n");
    }

    return e;
}


void destroy_expression (Expression* e){

    if (e==NULL){
        free(e);
        return;
    }

    if (e->id == COMMAND){
        free(e->node.cmd_expr);
    }

    if (e->id == IF_EXPRESSION){
        destroy_expression(e->node.if_expr.condition);
        destroy_expression(e->node.if_expr.statement);
    }

    if (e->id == OR_EXPRESSION){
        destroy_expression(e->node.or_expr.left_statement);
        destroy_expression(e->node.or_expr.right_statement);
    }

    if (e->id == AND_EXPRESSION){
        destroy_expression(e->node.and_expr.left_statement);
        destroy_expression(e->node.and_expr.right_statement);
    }

    free(&e->id);
    // free(&e->node);
    // free(e);

}

/**
 * evaluation of an expression
 *
 * @param exp the expression to evaluate
 * @return if the evaluation succeeded or not
 */
int eval (Expression* exp){
    switch (exp->id){
        case IF_EXPRESSION :
            if (eval(exp->node.if_expr.condition)){
                return eval(exp->node.if_expr.statement);
            } else {
                return 0; // failure
            }
        case COMMAND:            return run_command(*exp->node.cmd_expr);
        case OR_EXPRESSION:      return or_eval(exp);
        case AND_EXPRESSION:     return and_eval(exp);
        default:                 return -1;
    }
}


/**
 * if left_statement works, do not run right_statement. Run right_statement only of left statement works.
 * @param exp
 * @return
 */
int or_eval (Expression* exp){
    int first_result = eval(exp->node.or_expr.left_statement);
    if (first_result){
        return first_result;
    } else {
        return eval(exp->node.or_expr.right_statement);
    }
}


/**
 *
 * @param exp
 * @return
 */
int and_eval (Expression* exp){
    int first_result = eval(exp->node.and_expr.left_statement);
    if (first_result){
        return eval(exp->node.or_expr.right_statement);
    } else {
        return false;
    }

}



/**
 * Visualization tool to help debug the AST
 * @param e
 * @param level
 */
void __debug_print(Expression* e, int level){

    if (level == 0){
        printf("root : \n");
    }

    for (int i = 0; i < level; ++i) {
        printf(" ");
    }

    if (e->id == COMMAND){
        printf("cmd: {name=%s, arg=", e->node.cmd_expr->cmd_name);
        for (int i = 0; i < e->node.cmd_expr->arg_size; ++i) {
            printf("%s ", e->node.cmd_expr->args[i]);
        }
        printf(", options=");
        for (int i = 0; i < e->node.cmd_expr->options_size; ++i) {
            printf("%s ", e->node.cmd_expr->options[i]);
        }
        printf("}\n");
    }

    if (e->id == IF_EXPRESSION){
        printf("if: \ncond=");
        __debug_print(e->node.if_expr.condition, ++level);
        printf("statement=");
        __debug_print(e->node.if_expr.statement, ++level);
        printf("\n");
    }

    if (e->id == OR_EXPRESSION){
        printf("or: left=");
        __debug_print(e->node.or_expr.left_statement, ++level);
        printf("right=");
        __debug_print(e->node.or_expr.right_statement, ++level);
        printf("\n");
    }

    if (e->id == AND_EXPRESSION){
        printf("and: left=");
        __debug_print(e->node.and_expr.left_statement, ++level);
        printf("right=");
        __debug_print(e->node.and_expr.right_statement, ++level);
        printf("\n");
    }

}

#endif //TESTSHELL_EXPRESSION_H
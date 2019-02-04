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
static char *enumStrings[] = {"COMMAND", "IF_EXPRESSION", "OR_EXPRESSION", "AND_EXPRESSION"};

typedef struct command {
    char**  cmd;
    bool redirect_flag;
    char* output_file;
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
            struct Expression*      left;
            struct Expression*      right;
        }                                               cond_expr;
    } node;
} Expression;


Expression* create_exp      (exp_type type, Expression* first, Expression* second);
Expression* create_cmd      (char** line, int start_index, int end_index);

int eval                    (Expression* exp);
int or_eval                 (Expression* exp);
int and_eval                (Expression* exp);
void destroy_command        (command* cmd);
int run_command             (command* cmd);


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

    e->id = type;
    e->node.cond_expr.left = first;
    e->node.cond_expr.right = second;
    return e;

}


/**
 *
 * @param line
 * @return
 * 
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


    e->node.cmd_expr->cmd = malloc(sizeof(char) * (size + 1));
    if (e->node.cmd_expr->cmd == NULL){
        free(e);
        perror("malloc error create_cmd");
        return NULL;
    }

    for(int i = 0; i < size; i++){
        e->node.cmd_expr->cmd[i] = line[i + start_index];
    }

    e->node.cmd_expr->cmd[size] = NULL;
    
    return e;
}


void destroy_expression (Expression* e){

    printf("freeing %s\n", enumStrings[e->id]);

    if (e==NULL){
        free(e);
        return;
    }

    if (e->id == COMMAND){
        destroy_command(e->node.cmd_expr);
    }

    else {
        destroy_expression(e->node.cond_expr.left);
        destroy_expression(e->node.cond_expr.right);
    }

    free(e);

}


void destroy_command(command* cmd_expr){
    free(cmd_expr->cmd);
    free(cmd_expr);
}

/**
 * evaluation of an expression
 *
 * @param exp the expression to evaluate
 * @return if the evaluation succeeded or not
 */
int eval (Expression* exp){
    printf("evaluation : %s\n", enumStrings[exp->id]);
    switch (exp->id){
        case IF_EXPRESSION :
            if (eval(exp->node.cond_expr.left)){
                return eval(exp->node.cond_expr.right);
            } else {
                return 0; // failure
            }
        case COMMAND:            return run_command(exp->node.cmd_expr);
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
    int first_result = eval(exp->node.cond_expr.left);
    if (first_result){
        return first_result;
    } else {
        return eval(exp->node.cond_expr.right);
    }
}


/**
 *
 * @param exp
 * @return
 */
int and_eval (Expression* exp){
    int first_result = eval(exp->node.cond_expr.left);
    if (first_result){
        return eval(exp->node.cond_expr.right);
    } else {
        return false;
    }
}

#endif //TESTSHELL_EXPRESSION_H

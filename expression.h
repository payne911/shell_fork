#ifndef TESTSHELL_EXPRESSION_H
#define TESTSHELL_EXPRESSION_H








/* ********************************************************
 * GLOBAL DEFINITIONS
 * ****************************************************** */


#define EXIT_SHELL "exit"
#define LS "ls"
#define ECHO "echo"
#define IF_TOKEN "if"
#define DO_TOKEN "do"
#define DONE_TOKEN "done"
#define OR_TOKEN "||"
#define AND_TOKEN "&&"
#define BACK_TOKEN "&"
#define IF_SEP ";"
#define REDIRECT_SEPERATOR ">"
#define FAIL 'F'
#define SUCCESS 'S'
#define INVALID_INDEX -1

static const char * enumStrings[] = 
    {"COMMAND", "IF_EXPRESSION", "OR_EXPRESSION", "AND_EXPRESSION"};








/* ********************************************************
 * TYPE DEFINITIONS
 * ****************************************************** */


/**
 * Define different expression types that the 
 * Abstract Syntax Tree can have.
 * */
typedef enum {COMMAND, IF_EXPRESSION, OR_EXPRESSION, AND_EXPRESSION} exp_type;


/**
 * Wrapper around a single command to be executed. Contains an array
 * of word in the correct structure for a call to exec() and optional 
 * information about the redirect output file.
 * */
typedef struct Command {
    char**  cmd;            // a command
    bool    redirect_flag;  // if '>' exists
    char*   output_file;    // name of the file
} Command;


/**
 * Wrapper around a whole command line. Contains the number of words 
 * (split by spaces) and the conditional background flag to execute
 * the whole line as a background task.
 * */
typedef struct Split_line {
    int     size;           // size of the array
    char**  content;        // array of words
    bool    thread_flag;    // if '&' at the end
} Split_line;


/**
 * Abstract syntax tree representation for parsing the command line.
 * Inspired by : https://lambda.uta.edu/cse5317/notes/node26.html
 * */
typedef struct Expression {
    exp_type                                            id;
    union {
        Command*                                        cmd_expr;
        struct {
            struct Expression*      left;
            struct Expression*      right;
        }                                               cond_expr;
    } node;
} Expression;





/* ********************************************************
 * FUCNTION DECLARATIONS
 * ****************************************************** */


Expression* create_exp   (exp_type type, Expression* first, Expression* second);
Expression* create_cmd   (char** line, int start_index, int end_index);
Expression* parse_line   (Split_line* line, int start_index, int end_index);

int run_command          (Command* cmd);

int eval                 (Expression* exp);
int or_eval              (Expression* exp);
int and_eval             (Expression* exp);

void destroy_command     (Command* cmd);
void destroy_expression  (Expression* e);
void free_split_line     (Split_line* line);








/* ********************************************************
 * PARSING FUNCTIONS
 * ***************************************************** */


/**
 * Creates a command struct that represent a single system call to be 
 * executed. That struct contains the array that the exec() call needs and 
 * other useful information for executing the command.
 * @param   start_index     the index of the first word of the command to 
 *                          be created in the command line.
 * @param   end_index       the index of the first word of the command to 
 *                          be created in the command line.
 * @param   line            the array of words representing the command
 *                          to be created
 * @returns The Expression that represent the command to execute.
 * 
 * */ 
Expression * create_cmd (char** line, int start_index, int end_index){

    // correcting SIZE
    int size = end_index - start_index + 1;
    if (strncmp(line[start_index + size - 1], IF_SEP, 1) == 0){
        size--;
    }

    // ALLOCATING MEMORY
    Expression* e = malloc(sizeof(Expression));
    if (e == NULL){
        perror("malloc error create_cmd");
        return NULL;
    }

    e->id = COMMAND;

    e->node.cmd_expr = malloc(sizeof(Command));
    if (e->node.cmd_expr == NULL){
        free(e);
        perror("malloc error create_cmd");
        return NULL;
    }

    // +1 for final NULL
    e->node.cmd_expr->cmd = malloc(sizeof(char*) * (size + 1));
    e->node.cmd_expr->redirect_flag=false;

    if (e->node.cmd_expr->cmd == NULL){
        free(e->node.cmd_expr);
        free(e);
        perror("malloc error create_cmd");
        return NULL;
    }

    // 
    int i;
    bool redir = false;
    for(i = 0; i < size; i++){

        // gérer le cas des multiples redirects
        if (redir){
            char * current = line[i + start_index]; 
            char * next = line[i + start_index + 1];
            if (strncmp(current, REDIRECT_SEPERATOR, 1) == 0 && 
                (next == NULL 
                || strncmp (next, OR_TOKEN, 2)   == 0
                || strncmp (next, AND_TOKEN, 2)  == 0
                || strncmp (next, DO_TOKEN, 2)   == 0
                || strncmp (next, IF_TOKEN, 2)   == 0
                || strncmp (next, DONE_TOKEN, 4) == 0
                || next == IF_SEP
                || next == REDIRECT_SEPERATOR)){
                    printf("no output file. Redirect kept on %s", 
                                                e->node.cmd_expr->output_file);
                    return e;
            } else {
                if (strncmp(current, REDIRECT_SEPERATOR, 1)==0 && next != NULL){
                    printf("dest : %s\n", next);
                    e->node.cmd_expr->output_file=next;
                }
            }
        }       

        // si on trouve un redirect
        else if (strncmp(line[i+start_index], REDIRECT_SEPERATOR, 1) == 0){
            redir = true;
            char * next = line[i + start_index + 1];
            
            // if next word is NULL or a token => no redirect file
            if (next == NULL 
                || strncmp (next, OR_TOKEN, 2)  == 0
                || strncmp (next, AND_TOKEN, 2) == 0
                || strncmp (next, DO_TOKEN, 2)       == 0
                || strncmp (next, IF_TOKEN, 2)       == 0
                || strncmp (next, DONE_TOKEN, 4)     == 0
                || next == IF_SEP
                || next == REDIRECT_SEPERATOR){
                    printf("no output file. Redirect on %s ignored\n", 
                                                            line[start_index]);
                    // realloc a smaller memory 
                    char ** tmp = realloc(e->node.cmd_expr->cmd, 
                                                    (sizeof(char*) * (i + 1)));
                    if (tmp == NULL){
                        free(e->node.cmd_expr->cmd);
                        return NULL;
                    }
                    e->node.cmd_expr->cmd = tmp;
                    e->node.cmd_expr->cmd[i+1] = NULL;
                    e->node.cmd_expr->redirect_flag=false;
                    return e;
                }
               
                // looks like 'command > my_file.txt'
                e->node.cmd_expr->redirect_flag=true;
                e->node.cmd_expr->output_file=next;

                // realloc() free the old pointer, 
                // but if it fails the old pointer is still OK
                char ** tmp = realloc(e->node.cmd_expr->cmd, 
                                                    (sizeof(char*) * (i + 1)));
                    if (tmp == NULL){
                        free(e->node.cmd_expr->cmd);
                        return NULL;
                    }
                e->node.cmd_expr->cmd = tmp;
                e->node.cmd_expr->cmd[i] = NULL;                
            }

        else {
            e->node.cmd_expr->cmd[i] = line[i + start_index];
        }
    }

    if (!redir){
        e->node.cmd_expr->cmd[size] = NULL;
    }
    
    return e;
}



/**
 * Creates nodes of the syntax tree based on what words are located within
 * the start_index and end_index.
 * 
 * @param   start_index     the index of the first word of the command line 
 *                          to parse
 * @param   end_index       the index of the last word of the command line 
 *                          to parse
 * @param   split_line      an array of word containing each individual 
 *                          statement (word seperated by spaces) of
 *                          the command line
 * @return  an Expression representation of the command to execute. It is
 *          a single node in the Abstract Syntax Tree that is created. 
 * 
 * */
Expression * parse_line (Split_line* line, int start_index, int end_index) {

    //printf("parsing line from %d to %d\n", start_index, end_index);

    int if_count = 0;
    int do_count = 0;
    int done_count = 0;

    // look for the root of the AST : either the first if statement,
    // or the first logical operator outside an if statement
    for (int i = start_index; i < end_index; ++i) {

        char* current = line->content[i];

        // counting if
        if (strncmp(current, IF_TOKEN, 2) == 0){
            if (i == start_index){
                    if_count++;
            } else {
                char* previous = line->content[i - 1];
                if (strncmp(previous, IF_TOKEN, 2) == 0  || 
                    strncmp(previous, DO_TOKEN, 2) == 0  || 
                    strncmp(previous, AND_TOKEN, 2) == 0 || 
                    strncmp(previous, OR_TOKEN, 2) == 0){
                    if_count++;
                }
            }
        }

        // counting do
        if (strncmp(current, DO_TOKEN, 2) == 0 && 
            strncmp(current, DONE_TOKEN, 4) != 0){
            // todo : check if it is a true 'do'
            if (i != start_index 
                && strncmp(line->content[i-1], IF_SEP, 1) == 0){
                do_count++;
            }
        }


        // counting done
        if (strncmp(current, DONE_TOKEN, 4) == 0){
            if (i != start_index && 
                strncmp(line->content[i-1], IF_SEP, 1) == 0){
                done_count++;
            }
        }

        // outside of an if statement implies if_count == done_count

        // the 'and' token at 'i' is the root
        if (strncmp(current, AND_TOKEN, 3) == 0 && if_count == done_count){
            //printf("the \'AND\' token at %d is the root\n", i);
            Expression* left = parse_line(line, start_index, i - 1);
            if (left == NULL){
                return NULL;
            }
            Expression* right = parse_line(line, i+1, end_index);
            if (right == NULL){
                destroy_expression(left);
                return NULL;
            } else {
                return create_exp(AND_EXPRESSION, left, right);
            }
        }

        // the 'or' token at 'i' is the root
        if (strncmp(current, OR_TOKEN, 2) == 0 && if_count == done_count){
            //printf("the \'OR\' token at %d is the root\n", i);
            Expression* left = parse_line(line, start_index, i - 1);
            if (left == NULL){
                return NULL;
            }
            Expression* right = parse_line(line, i+1, end_index);
            if (right == NULL){
                destroy_expression(left);
                return NULL;
            } else {
                return create_exp(OR_EXPRESSION, left, right);
            }

        }

    }

    // error if statement
    if ((if_count != 0 || do_count != 0 || done_count != 0) && 
        ((if_count != do_count) && (if_count != done_count))){
        printf("error in parsing if statement (if:%d, do:%d, done:%d)\n", 
            if_count, do_count, done_count);
        return NULL;
    }

    // here, the root of the AST is the 'if' statement at the beginning of the
    if (if_count != 0 && if_count == done_count){
        //printf("the \'IF\' token at %d is the root\n", start_index);

        // inside an if statement.
        // Need to separate the 'if' and 'do' statements
        if_count = 0;
        do_count = 0;
        done_count = 0;

        int do_index = INVALID_INDEX;
        int done_index = INVALID_INDEX;
        bool do_found = false;

        for (int j = start_index; j < end_index; ++j){

            char* current = line->content[j];

            if (strncmp(current, IF_TOKEN, 2) == 0){
                if_count++;
            }

            if (strncmp(current, DO_TOKEN, 2) == 0){
                do_count++;
            }

            if (strncmp(current, DONE_TOKEN, 4) == 0){
                done_count++;
            }

            if (if_count == do_count && !do_found){
                do_index = j;
                do_found = true;
            }

            if (if_count == done_count){
                done_index = j;
            }

        }

        //printf("if : %d, do : %d\n", start_index, do_index);

        // if :
        Expression *condition = parse_line(line, start_index + 1, do_index - 1);
        if (condition==NULL){
            return NULL;
        }

        Expression *statement = parse_line(line, do_index +1, done_index - 1);
        if (statement == NULL){
            destroy_expression(condition);
            return NULL;
        }

        return create_exp(IF_EXPRESSION, condition, statement);
    }

    // there is no 'if' statement, nor logical operator. 
    // The commnad line is a single command.
    if (if_count == 0){
        return create_cmd(line->content, start_index, end_index);
    }

    // error in parsing
    printf("error in parsing\n");
    return NULL;

}








/* ********************************************************
 * EVALUATION FUNCTIONS
 * ***************************************************** */


/**
 * The entry point for evaluating a node of the Abstract Syntax Tree.
 * This function should be called with the root of the tree to evaluate
 * every node in the correct order.
 * 
 * Four different type of expression are used to evaluate the tree as defined
 * in the exp_type enum. Every enum item has it's own way to be evaluated
 * correctly. IF_EXPRESSION is directly evaluated here, wheras 
 * OR_EXPRESSION and AND_EXPRESSION have their own methods. COMMAND enum type
 * also has it's own command run_command(command *) to execute a single
 * command.
 * 
 * @param   exp     the node of the AST to evaluate.
 * @return  int     a boolean representing if the evaluation 
 *                  has succeeded or not.
 * 
 * */
int eval (Expression* exp){
    switch (exp->id){
        case IF_EXPRESSION :
            if (eval(exp->node.cond_expr.left)){
                return eval(exp->node.cond_expr.right);
            } else {
                // failure of the conditional statement is considered a 
                // successful if statement
                return true; // success
            }
        case COMMAND:            return run_command(exp->node.cmd_expr);
        case OR_EXPRESSION:      return or_eval(exp);
        case AND_EXPRESSION:     return and_eval(exp);
        default:                 return -1;
    }
}


/**
 * Evaluation procedure for an IF_EXPRESSION exp_type enum instance.
 * 
 * */
int or_eval (Expression* exp){
    int first_result = eval(exp->node.cond_expr.left);
    if (first_result){
        return first_result;
    } else {
        return eval(exp->node.cond_expr.right);
    }
}


/**
 * Evaluation procedure for an AND_EXPRESSION exp_type enum instance.
 * 
 * */
int and_eval (Expression* exp){
    int first_result = eval(exp->node.cond_expr.left);
    if (first_result){
        return eval(exp->node.cond_expr.right);
    } else {
        return false;
    }
}








/* ***********************************************************
 * MEMORY MANAGEMENT 
 * *********************************************************** */


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


void destroy_expression (Expression* e){

    // printf("freeing %s\n\n", enumStrings[e->id]);

    if (e==NULL){
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


void destroy_command(Command* cmd_expr){
    free(cmd_expr->cmd);
    free(cmd_expr);
}


void free_split_line(Split_line* line) {
    for(int i = 0; i < line->size; i++) {
        if(line->content[i] != NULL)
            free(line->content[i]);
    }
    free(line->content);
    free(line);
}

#endif //TESTSHELL_EXPRESSION_H

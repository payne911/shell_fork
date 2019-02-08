/* ch.c.
auteurs:    Jérémi Grenier-Berthiaume
            Olivier Lepage-Applin
date:       8 Février 2019
problèmes connus:
    - `optimizer_cnt` is lazy: a solution that wouldn't use strtok is possible
    - `debug_free` is lazy: a better solution could have been used
    - there could be even MORE error management
    - maybe we should have written it all in French..?
    - two lines dared to adventure beyond the almighty 80-chars-boundary
  */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>


/*
 * Abstract Syntax Tree.
 * Used to be in a "expression.h" file, but we are supposed to provide 1 file.
 */

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
                    printf("No output file. Redirect kept on %s.\n",
                                                e->node.cmd_expr->output_file);
                    return e;
            } else {
                if (strncmp(current, REDIRECT_SEPERATOR, 1)==0 && next != NULL){
                    //printf("dest : %s\n", next);
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
                    printf("No output file. Redirect on %s ignored\n",
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
 * Creates nodes of the syntax tree based on what words are located whithin
 * the start_index and end_index.
 * 
 * @param   start_index     the index of the first word of the command line 
 *                          to parse
 * @param   end_index       the index of the last word of the command line 
 *                          to parse
 * @param   split_line      an array of word containing each individual 
 *                          statement (word seoerated by spaces) of 
 *                          the command line
 * @return  an Expression representation of the commad to execute. It is
 *          a single node in the Abstract Syntax Tree that is created. 
 * 
 * */
Expression * parse_line (Split_line* line, int start_index, int end_index) {

    // printf("parsing line from %d to %d\n", start_index, end_index);

    if (start_index<0||end_index<0){
        return NULL;
    }

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
                    strncmp(previous, OR_TOKEN, 2) == 0) {
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
        if (strncmp(current, DO_TOKEN, 2) == 0 && 
            strlen(current) == 2 &&
            strncmp(line->content[i-1], IF_SEP, 1) == 0) {
            done_count++;
        }


        // outside of an if statement implies if_count == done_count

        // the 'and' token at 'i' is the root
        if (strncmp(current, AND_TOKEN, 3) == 0 && if_count == done_count){
            // printf("the \'AND\' token at %d is the root\n", i);
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
            // printf("the \'OR\' token at %d is the root\n", i);
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
        ((if_count != do_count) || (if_count != done_count))){
        printf("error in parsing if statement\n");
        return NULL;
    }

    // here, the root of the AST is the 'if' statement at the beginning of the
    if (if_count != 0 && if_count == done_count){

        // inside an if statement.
        // Need to separate the 'if' and 'do' statements
        if_count = 0;
        do_count = 0;
        done_count = 0;

        int do_index = INVALID_INDEX;
        int done_index = INVALID_INDEX;
        bool do_found = false;

        for (int j = start_index; j <= end_index; ++j){

            char* current = line->content[j];
            // printf("checking for ifs [%s]\n", current);
            if (strncmp(current, IF_TOKEN, 2) == 0 &&
                strlen(current) == 2){
                if_count++;
            }

            if (strncmp(current, DO_TOKEN, 2) == 0 && 
                strlen(current) == 2 &&
                strncmp(line->content[j-1], IF_SEP, 1) == 0
                ){
                do_count++;
            }

            if (strncmp(current, DONE_TOKEN, 4) == 0 &&
                strlen(current) == 4 &&
                strncmp(line->content[j-1], IF_SEP, 1) == 0){
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
    printf("error in parsing \n");
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
  if (exp == NULL){
    return false;
  }
  
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


/*
 * ###########################################################
 * #        END OF THE "ABSTRACT SYNTAX TREE" code.          #
 * ###########################################################
 */



/*
 * The "FAILED_EXECVP" value is returned by a child when `execvp`
 * did not successfully run the command.
 */
#define FAILED_EXECVP 42


/*
 * Updated as the PID of the last child process created by a
 * sequential command (no trailing '&').
 */
volatile pid_t CURR_CHILD = 0;


/*
 * Function declarations.
 */

Split_line* form_split_line(char**, int);
char** query_and_split_input();
size_t optimizer_cnt(const char*);
int count_words(char**);
char** split_str (const char*, const char[]);
void debug_free(char**);
void set_up_handlers();
void z_handler();
void ctrl_Z_handler(int);
void remove_Z_handler();
void set_up_reaper();
void zombie_handler(int);
void run_bg_cmd(Split_line*);
bool writeOutputInFile(const char*);
int run_command(Command*);
void run_fg_cmd(Split_line*);


/*
 * Utils.
 */

Split_line* form_split_line(char** args, int wordc) {
    /// Properly sets up a `Split_line` with the char** obtained from query.

    Split_line* sl = malloc(sizeof(Split_line));
    if(sl == NULL) return NULL;  // OOM
    sl->content = args;
    sl->size = wordc;
    sl->thread_flag = ((strcmp(args[wordc-1], "&")) == 0); // find trailing '&'
    return sl;
}

char** query_and_split_input() {
    /// Asks for a command. Ignores anything beyond first '\n'.
    /// Returns the array resulting from splitting the input.

    /* Prompting for command. */
    printf ("\nshell> ");  // `\n` to ensure the printf-buffer is emptied

    char* input_str = NULL;
    size_t buffer_size;
    if (getline(&input_str, &buffer_size, stdin) == -1) {
        free(input_str);
        return NULL;
    }
    input_str[strcspn(input_str, "\n")] = 0;  // crop to first new-line

    /* Splitting the input string. */
    char** args;
    if ((args = split_str (input_str, " ")) == 0) {
        free(input_str);
        return query_and_split_input();
    } else {
        free(input_str);
        return args;
    }
}

size_t optimizer_cnt(const char *str) {
    /// Returns min(1, #words).

    char copied_str[strlen(str)+1];
    strcpy(copied_str, str);
    size_t count = 1;

    strtok(copied_str, " ");
    while(strtok(NULL, " ") != 0) {
        count++;
    }

    return count;
}

int count_words(char** tab) {
    /// Counts the amount of words in the split line (parsed from input by user).
    int tmp = -1;

    while(true) {
        tmp++;
        if(tab[tmp] == NULL)
            break;
    }

    return tmp;
}

char** split_str (const char* str, const char delim[]) {
    /// Returns a list of pointers to strings that came from splitting the input.
    /// Returns NULL in case of error.

    char* copied_input = strdup(str);  // because 'strtok()' alters the string
    if (copied_input == 0) return NULL;  // OOM

    /* Counting words to optimize allocated's memory size. */
    size_t nwords = optimizer_cnt(copied_input);
    char** result;
    if ((result = malloc((nwords+1) * sizeof(char *))) == 0) return NULL; // OOM

    /* Obtaining first word. */
    char* token = strtok (copied_input, delim);
    if (token == NULL) {
        free(result);
        free(copied_input);
        return NULL;
    } else if ((result[0] = strdup (token)) == 0) {  // OOM
        free(result);
        free(copied_input);
        return NULL;
    }

    /* To populate the whole array with the rest of the words. */
    int tmp = 0;
    while ((token = strtok (NULL, delim)) != NULL) {
        if ((result[++tmp] = strdup (token)) == 0) {
            /* In case of OOM, each past array must also be freed. */
            while (tmp-- != 0) {
                free(result[tmp]);
            }
            free(result);
            free(copied_input);
            return NULL;
        }
    }

    result[++tmp] = NULL;  /* Must end with NULL */

    free(copied_input);
    return result;
}

void debug_free(char** args) {
    /// Lazy function to ensure `args` is freed properly.

    int count = count_words(args);
    Split_line* line = form_split_line(args, count);
    free_split_line(line);
}


/*
 * Bonus 2
 */

void set_up_handlers() {
    /// To initialize the signal handlers.

    set_up_reaper();
    z_handler();
}

void z_handler() {
    /// To treat Ctrl-Z (Bonus #2).

    struct sigaction bonus2_signal;
    bonus2_signal.sa_handler = &ctrl_Z_handler;
    sigemptyset(&bonus2_signal.sa_mask);
    bonus2_signal.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &bonus2_signal, NULL);
}

void ctrl_Z_handler(int sigNo) {
    /// Stops the process identified by the CURR_CHILD pid.

    if(CURR_CHILD != 0)  // prevents crash if there wasn't any child created yet
        kill(CURR_CHILD, SIGSTOP);  // triggers the waitpid in the parent
}

void remove_Z_handler() {
    /// Used to ensure a child-process will not detect the Ctrl-Z.

    struct sigaction signal_ignore;
    signal_ignore.sa_handler = SIG_IGN;
    sigemptyset(&signal_ignore.sa_mask);
    signal_ignore.sa_flags = 0;
    sigaction(SIGTSTP, &signal_ignore, NULL);
}


/*
 * Q2.4
 */

void set_up_reaper() {
    /// To reap zombie-child processes.

    struct sigaction zombie_reaper;
    zombie_reaper.sa_handler = &zombie_handler;
    sigemptyset(&zombie_reaper.sa_mask);
    zombie_reaper.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &zombie_reaper, NULL);
}

void zombie_handler(int sigNo) {
    /// Reaps the processes it receives signals from.

    int status;
    while ((waitpid(-1, &status, WNOHANG)) > 0)
        ;  // always reap children
}

void run_bg_cmd(Split_line *line) {
    /// To execute a chain of command(s) in the background.

    /* To replace the trailing '&' from the command. */
    free(line->content[line->size-1]);
    line->content[line->size-1] = NULL;
    Expression* ast = parse_line(line, 0, line->size - 2);

    /* Forking. */
    pid_t    pid;
    pid = fork();

    if (pid < 0) {  // error while forking
        free_split_line(line);

    } else if (pid == 0) {  // child
        remove_Z_handler();  // prevent Cltr-Z triggers in child

        // todo: if "cat" or "vi", stop process ? (bonus 1)

        /* Executing the command(s). */
        eval(ast);

        /* Child process failed. */
        free_split_line(line);
        destroy_expression(ast);
        exit(-1);


    } else {  // back in parent

        /* Freeing memory. */
        free_split_line(line);
        destroy_expression(ast);
    }
}

bool writeOutputInFile(const char* output) {
    /// Redirecting `stdout` to a file-descriptor.

    int fileDescriptor = open(output,O_WRONLY|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
    if (fileDescriptor < 0)         return false;    // in case of error
    if (dup2(fileDescriptor,1) < 0) return false;    // in case of error
    if (close(fileDescriptor) < 0)  return false;    // in case of error
    return true;
}


/*
 * Q2.1
 */

int run_command(Command* cmd) {
    /// Called throughout the recursion that evaluates every single command.

    /* Forking. */
    int         status;
    CURR_CHILD = fork();

    if (CURR_CHILD < 0) {
        return false;

    } else if (CURR_CHILD == 0) {  // child
        remove_Z_handler();  // prevent Cltr-Z triggers in child

        /* Redirecting output. */
        if(cmd->redirect_flag) {
            const char* filePath = cmd->output_file;
            bool successfulWrite = writeOutputInFile(filePath);
            if(!successfulWrite) {
                // todo: in case of error
            }
        }

        /* Executing the command(s). */
        execvp(cmd->cmd[0], cmd->cmd);

        /* Child process failed. */
        exit(FAILED_EXECVP);


    } else {  // back in parent
        waitpid(CURR_CHILD, &status, WUNTRACED);  // wait for child to finish

        /* Managing the 'return status' of an evaluation. */
        if(WEXITSTATUS(status) == FAILED_EXECVP) {
            return false;  // execvp couldn't execute the command properly
        } else if (status == 0) {
            return true;
        } else {
            return false;
        }
    }
}

void run_fg_cmd(Split_line *line) {
    /// To execute a chain of command(s) in the foreground.

    /* Create and execute Syntax Tree. */
    Expression* ast_root = parse_line(line, 0, line->size - 1);
    eval(ast_root);

    /* Freeing the memory. */
    destroy_expression(ast_root);
    free_split_line(line);
}

int main (void) {
    /// Instanciates the main shell and queries the command(s).

    fprintf (stdout, "%% ");
    /* ¡REMPLIR-ICI! : Lire les commandes de l'utilisateur et les exécuter. */

    /* Initial set up. */
    bool running = true;
    set_up_handlers();

    /* Our shell. */
    while(running) {

        /* Ask for an instruction. */
        char** args = query_and_split_input();

        /* Error handling  |  exit  |  set up. */
        if (args == NULL) {  // error while reading input
            running = false;
        } else if (strcmp(args[0], "exit") == 0) {  // home-made "exit" command
            running = false;
            debug_free(args);
        } else {
            int count = count_words(args);
            Split_line* line = form_split_line(args, count);
            if(line == NULL) {  // if an error occured: skip and ask new query
                free_split_line(line);
                continue;
            }

            /* Executing the command(s). */
            if(line->thread_flag) {
                run_bg_cmd(line);
            } else {
                run_fg_cmd(line);
            }
        }
    }

    /* We're all done here. See you! */
    printf("Bye!\n");
    exit (0);
}
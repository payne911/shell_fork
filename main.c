/* ch.c.

auteurs:    Jérémi Grenier-Berthiaume
            Olivier Lepage-Applin

date:       Février 2019

problèmes connus:
    - `optimizer_cnt` could be faster (doesn't require using `strtok`)
  */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>


/*
 * Abstract Syntax Tree.
 */
#include "expression.h"

/*
 * The "DEBUG" value is used to print trace of the flow of execution.
 * Any non-zero value will result in traces being printed.
 * Setting it to 0 will silence the debugging mode.
 */
#define DEBUG 1


typedef struct split_line {
    int size;           // size of the array
    char** content;     // array of words
} split_line;

int count_words(char**);



size_t optimizer_cnt(const char *str) {
    /// Used to get an upper limit on the amount of words separated by a space in the input string.

    char copied_str[strlen(str)+1];
    strcpy(copied_str, str);  // todo: OOM ??
    size_t count = 1;

    strtok(copied_str, " ");
    while(strtok(NULL, " ") != 0) {
        count++;
    }

    return count;
}

char** split_str (const char* str, const char delim[]) {
    /// Returns a list of pointers to strings that came from splitting the input.
    /// Returns NULL in case of error.

    char* copied_input = strdup(str);  // because 'strtok()' alters the string it passes through
    if (copied_input == 0) return NULL;  // OOM

    /* Counting words to optimize allocated's memory size. */
    size_t nwords = optimizer_cnt(copied_input);
    char** result;
    if ((result = malloc((nwords+1) * sizeof(char *))) == 0) return NULL;  // OOM

    /* Obtaining first word. */
    char* token = strtok (copied_input, delim);
    if (token == NULL) {
        if(DEBUG != 0) printf("Bug extracting first word of split\n");
        free(result);
        free(copied_input);
        return NULL;
    } else if ((result[0] = strdup (token)) == 0) {  // OOM
        if(DEBUG != 0) printf("Bug allocating first word of split\n");
        free(result);
        free(copied_input);
        return NULL;
    }

    /* To populate the whole array with the rest of the words. */
    int tmp = 0;
    while ((token = strtok (NULL, delim)) != NULL) {
        if ((result[++tmp] = strdup (token)) == 0) {
            /* In case of OOM, each past array must also be freed. */
            while (tmp-- != 0) {  // todo: inequality is right? (will free down to result[1] inclusively?)
                if(DEBUG != 0) printf("Bug allocating words of split. Currently freeing index %d\n", tmp);
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


/**
 * run a command line
 * @param command
 * @return
 */
int run_command(command cmd){
//    bool success = cmd.cmd_name[0] == SUCCESS;
//    printf("running : %s (%s)\n", cmd.cmd_name, success?"SUCCESS":"FAIL");
//    printf("args : %s");
//    return success;

    if(DEBUG != 0) {
        printf("in debug");
        int tmp = count_words(cmd.cmd);
        printf("count is %d\n", tmp);
        while(tmp-- != 0) {
            printf("run_cmd %d : %s\n", tmp, cmd.cmd[tmp]);
        }
    }

    /* Forking. */
    int status;  // todo: see https://www.gnu.org/software/libc/manual/html_node/Process-Creation-Example.html   ??
    pid_t    pid;
    pid = fork();


    if (pid < 0) {
        fprintf(stderr, "fork failed");

    } else if (pid == 0) {  // child
        /* Executing the commands. */
        if(DEBUG != 0) printf("Child executing the command.\n");
        execvp(cmd.cmd[0], cmd.cmd);

        /* Child process failed. */
        if(DEBUG != 0) printf("execvp didn't finish properly: running exit on child process\n");
        exit(-1);  // todo: free variables?


    } else {  // back in parent
        wait(NULL);  // wait for child to finish        todo: waitpid() ???

        //free(args);  todo: now necessary?
        if(DEBUG != 0) printf("Terminating parent of the child.\n");
    }

    return true;
}


void run_shell(Expression* ast, split_line* sl, char** args) {
    /// Does the forking and executes the given command.

    /* Forking. */
    int status;  // todo: see https://www.gnu.org/software/libc/manual/html_node/Process-Creation-Example.html   ??
    pid_t    pid;
    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "fork failed");

    } else if (pid == 0) {  // child
        /* Executing the commands. */
        if(DEBUG != 0) printf("Child executing the command.\n");
        execvp(args[0], args);

        /* Child process failed. */
        if(DEBUG != 0) printf("execvp didn't finish properly: running exit on child process\n");
        exit(-1);  // todo: free variables?


    } else {  // back in parent
        wait(NULL);  // wait for child to finish        todo: waitpid() ???

        free(args);
        if(DEBUG != 0) printf("Terminating parent of the child.\n");
    }
}


int count_words(char** tab) {
    int tmp = -1;

    while(true) {
        tmp++;
        if(tab[tmp] == NULL)
            break;
    }

    if(DEBUG != 0) printf("number of words in input string: %d\n", tmp);

    return tmp;
}


split_line* form_split_line(char** args, int wordc) {
    split_line* sl = malloc(sizeof(split_line));  // todo: OOM
    sl->content = args;
    sl->size = wordc;
    return sl;
}


char** query_and_split_input() {
    /// Asks for a command until a valid one is given. Ignores anything beyond first '\n'.
    /// Calls the function that splits the command and returns the resulting array.

    /* Prompting for command. */
    printf ("shell> ");

    char* input_str = NULL;
    size_t buffer_size;
    if (getline(&input_str, &buffer_size, stdin) == -1) {
        if(DEBUG != 0) printf("error while trying to read line\n");
        //free(input_str);  // todo: how to know if malloc'd ?
        return NULL;
    }
    input_str[strcspn(input_str, "\n")] = 0;  // crop to first new-line    todo: \r ??  https://stackoverflow.com/a/28462221/9768291

    /* Splitting the input string. */
    char** args;
    if ((args = split_str (input_str, " ")) == 0) {
        if(DEBUG != 0) printf("error in split_str function: querying new command\n");
        free(input_str);
        return query_and_split_input();
    } else {
        if(DEBUG != 0) printf("split_str function finished\n");
        free(input_str);
        return args;
    }
}

/**
 * from a list of words, parse it to create a abstract syntax tree
 *
 * @param line
 * @param start_index
 * @param end_index
 * @return
 */
Expression* parse_line (split_line* line, int start_index, int end_index){

    //printf("parsing line from %d to %d\n", start_index, end_index);

    int if_count = 0;
    int do_count = 0;
    int done_count = 0;

    // look for the root of the AST : either the first if statement,
    // or the first logical operator outside an if statement
    for (int i = start_index; i < end_index; ++i) {

        char* current = line->content[i];

        if (strncmp(current, IF_TOKEN, 2) == 0){
            if_count++;
        }

        if (strncmp(current, DO_TOKEN, 2) == 0 && strncmp(current, DONE_TOKEN, 4) != 0){
            do_count++;
        }

        if (strncmp(current, DONE_TOKEN, 4) == 0){
            done_count++;
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
    if ((if_count != 0 || do_count != 0 || done_count != 0) && ((if_count != do_count) && (if_count != done_count))){
        printf("error in parsing if statement (if:%d, do:%d, done:%d)\n", if_count, do_count, done_count);
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
        Expression * condition = parse_line(line, start_index + 1, do_index - 1);
        if (condition==NULL){
            return NULL;
        }

        Expression * statement = parse_line(line, do_index +1, done_index - 1);
        if (statement == NULL){
            destroy_expression(condition);
            return NULL;
        }

        return create_exp(IF_EXPRESSION, condition, statement);
    }

    // there is no 'if' statement, nor logical operator. The commnad line is a single command.
    if (if_count == 0){
        return create_cmd(line->content, start_index, end_index);
    }

    // error in parsing
    printf("error in parsing\n");
    return NULL;

}

int main (void) {
    /// Instanciates the main shell and queries the commands. Type "eof" to exit.

    fprintf (stdout, "%% ");
    /* ¡REMPLIR-ICI! : Lire les commandes de l'utilisateur et les exécuter. */

    bool running = true;

    while(running) {

        /* Ask for an instruction. */
        char** args = query_and_split_input();

        /* Executing the commands. */
        if (args == NULL) {  // error while reading input
            running = false;
            if(DEBUG != 0) printf("error while reading line: aborting shell\n");
        } else if (strcmp(args[0], "eof") == 0) {  // home-made "exit" command
            running = false;
            // todo: free the sub-arrays (args[x])
            free(args);
            if(DEBUG != 0) printf("aborting shell\n");
        } else {
            if(DEBUG != 0) printf("shell processing new command\n");

            int count = count_words(args);
            split_line* line = form_split_line(args, count);  // todo: NULL returned
            Expression* ast = parse_line(line, 0, line->size - 1);

//            if(DEBUG != 0) printf("ast expression is formed\n");
//            eval(ast);
//            if(DEBUG != 0) printf("exited eval\n");

            run_shell(ast, line, args);

            destroy_expression(ast);
            free(line);

            if(DEBUG != 0) printf("done running shell on one command\n");
        }
    }

    /* We're all done here. See you! */
    printf("Bye!\n");
    exit (0);  // todo: why ???
}

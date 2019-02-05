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
#include <fcntl.h>
#include <signal.h>


/*
 * Abstract Syntax Tree.
 */
#include "expression.h"

/*
 * The "DEBUG" value is used to print trace of the flow of execution.
 * Any non-zero value will result in traces being printed.
 * Setting it to 0 will silence the debugging mode.
 */
#define DEBUG 0

/*
 * The "FAILED_EXECVP" value is returned by a child when `execvp` did not successfully run the command.
 */
#define FAILED_EXECVP 42


typedef struct split_line {
    int size;           // size of the array
    char** content;     // array of words
    bool thread_flag;   // if '&' at the end
} split_line;




size_t optimizer_cnt(const char *str) {
    /// Returns min(1, #words).

    char copied_str[strlen(str)+1];
    strcpy(copied_str, str);  // todo: OOM ??
    size_t count = 1;

    strtok(copied_str, " ");
    while(strtok(NULL, " ") != 0) {
        count++;
    }

    return count;
}

int count_words(char** tab) {
    /// Counts the amount of words in the split line (parsed from input by the user).
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

bool writeOutputInFile(const char* output) {
    int fileDescriptor = open(output, O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    if(fileDescriptor < 0) return false;            // in case of error
    if(dup2(fileDescriptor,1) < 0) return false;    // in case of error
    if(close(fileDescriptor) < 0) return false;     // in case of error
    return true;
}

/**
 * run a command line
 * @param command
 * @return
 */
int run_command(command* cmd){

    if(DEBUG != 0) {
        int tmp = count_words(cmd->cmd);
        printf("count is %d\n", tmp);
        while(tmp-- != 0) {
            printf("run_cmd %d : %s\n", tmp, cmd->cmd[tmp]);
        }
    }

    /* Forking. */
    int         status;
    pid_t       pid;
    pid = fork();


    if (pid < 0) {
        fprintf(stderr, "fork failed");
        return false;  // todo: not necessarily what we want

    } else if (pid == 0) {  // child
        if(DEBUG != 0) printf("Child executing the command.\n");

        /* Redirecting output. */
        if(cmd->redirect_flag) {
            const char* filePath = cmd->output_file;  // ex: "./test_output_file.txt";
            bool successfulWrite = writeOutputInFile(filePath);
            if(DEBUG != 0) printf("filepath for output: %s | successfulWrite: %s\n", filePath, successfulWrite==0?"false":"true");
            if (!successfulWrite) {
                printf("unsuccessful write coming from 'redirect output' flag\n");  // todo: in case of error
            }
        }

        /* Executing the commands. */
        execvp(cmd->cmd[0], cmd->cmd);

        /* Child process failed. */
        if(DEBUG != 0) printf("execvp didn't finish properly: running exit on child process\n");
        exit(FAILED_EXECVP);


    } else {  // back in parent
        waitpid(pid, &status, 0);  // wait for child to finish

        if(WIFEXITED(status)) {
            if(DEBUG != 0) printf("OK: Child exited with exit status %d.\n", WEXITSTATUS(status));

            if(WEXITSTATUS(status) == FAILED_EXECVP) {
                if(DEBUG != 0) printf("run_command returning FALSE\n");
                return false;  // execvp couldn't execute the command properly
            } else {
                if(DEBUG != 0) printf("run_command returning TRUE\n");
                return true;  // execution went normally
            }

        } else {
            if(DEBUG != 0) printf("ERROR: Child has not terminated correctly. Status is: %d\n", status);
            return false;  // todo: not necessarily what we want
        }
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
Expression * parse_line (split_line* line, int start_index, int end_index) {

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
                if (strncmp(previous, IF_TOKEN, 2) == 0
                    || strncmp(previous, DO_TOKEN, 2) == 0
                    || strncmp(previous, AND_TOKEN, 2) == 0
                    || strncmp(previous, OR_TOKEN, 2) == 0){
                    if_count++;
                }
            }
        }

        // counting do
        if (strncmp(current, DO_TOKEN, 2) == 0 && strncmp(current, DONE_TOKEN, 4) != 0){
            // todo : check if it is a true 'do'
            if (i != start_index && strncmp(line->content[i-1], IF_SEP, 1) == 0){
                do_count++;
            }
        }


        // counting done
        if (strncmp(current, DONE_TOKEN, 4) == 0){
            if (i != start_index && strncmp(line->content[i-1], IF_SEP, 1) == 0){
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


void free_split_line(split_line* line) {
    /// To `free()` the char** and all its sub-arrays
    if(DEBUG != 0) printf("free_split_line   |   line sub arrays size: %d\n", line->size);
    for(int i = 0; i < line->size; i++) {
        if(DEBUG != 0) printf("free_split_line   |   content[%d]: %s\n", i, line->content[i]);
        free(line->content[i]);
    }
    free(line->content);
    free(line);
}

void run_background_cmd(split_line* line) {

    /* To replace the trailing '&' from the command. */
    line->content[line->size-1] = NULL;
    Expression* ast = parse_line(line, 0, line->size - 2);

    /* Forking. */
    pid_t    pid;
    if(DEBUG != 0) printf("SUB___shell: just before fork.\n");
    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "SUB___fork failed");

    } else if (pid == 0) {  // child
        if(DEBUG != 0) printf("SUB___Child executing the command.\n\n");
        sleep(3);  // todo: remove eventually (after tests)

        /* Executing the commands. */
//      execvp(args[0], args);
        if(DEBUG != 0) printf("SUB___ast expression is formed\n");
        eval(ast);
        if(DEBUG != 0) printf("SUB___exited eval\n");

        /* Child process failed. */
        if(DEBUG != 0) printf("SUB___execvp didn't finish properly: running exit on child process\n");
        free_split_line(line);
        destroy_expression(ast);  // todo: ensure eval has it inside (remove from here)
        exit(-1);  // todo: remove?


    } else {  // back in parent
        free_split_line(line);
        destroy_expression(ast);  // todo: ensure eval has it inside (remove from here)

        if(DEBUG != 0) printf("SUB___Terminating parent of the child.\n");
    }

    if(DEBUG != 0) printf("SUB___run_bg_cmd is over.\n");
}

split_line* form_split_line(char** args, int wordc) {
    /// Properly sets up a "SplitLine" struct with the char** obtained from the query.
    split_line* sl = malloc(sizeof(split_line));
    if(sl == NULL) return NULL;  // OOM
    sl->content = args;
    sl->size = wordc;
    sl->thread_flag = ((strcmp(args[wordc-1], "&")) == 0); // find if there is a trailing '&'
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
        if(DEBUG != 0) printf("QUERY____error while trying to read line\n");
        //free(input_str);  // todo: how to know if malloc'd ?
        return NULL;
    }
    input_str[strcspn(input_str, "\n")] = 0;  // crop to first new-line    todo: \r ??  https://stackoverflow.com/a/28462221/9768291

    /* Splitting the input string. */
    char** args;
    if ((args = split_str (input_str, " ")) == 0) {
        if(DEBUG != 0) printf("QUERY____error in split_str function: querying new command\n");
        free(input_str);
        return query_and_split_input();
    } else {
        if(DEBUG != 0) printf("QUERY____split_str function finished\n");
        free(input_str);
        return args;
    }
}


void zombie_handler(int sigNo) {
    int status;
    pid_t child_pid;
    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        /*...do something with exited child's status */
        printf("Reaped child pid %d | exit status: %d\n", child_pid, status);
    }
}

void ctrl_Z_handler(int sigNo) {
    printf("Caught Ctrl-Z\n");
}


int main (void) {
    /// Instanciates the main shell and queries the commands. Type "eof" to exit.

    fprintf (stdout, "%% ");
    /* ¡REMPLIR-ICI! : Lire les commandes de l'utilisateur et les exécuter. */

    bool running = true;


    /* To reap zombie child processes created by using '&'. */
    struct sigaction zombie_reaper;  // todo, read: http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
    zombie_reaper.sa_handler = &zombie_handler;
    sigemptyset(&zombie_reaper.sa_mask);
    zombie_reaper.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &zombie_reaper, NULL) == -1) {  // todo: error handling
        perror(0);
        printf("error within SIGCHLD signal thingy\n");
        exit(1);
    }

    /* To treat Ctrl-Z (Bonus #2). */
    struct sigaction bonus2_signal;  // todo, read: https://indradhanush.github.io/blog/writing-a-unix-shell-part-3/
    bonus2_signal.sa_handler = &ctrl_Z_handler;
    sigemptyset(&bonus2_signal.sa_mask);
    bonus2_signal.sa_flags = SA_RESTART;
    if (sigaction(SIGTSTP, &bonus2_signal, NULL) == -1) {  // todo: error handling
        perror(0);
        printf("error within SIGTSTP signal thingy\n");
        exit(1);
    }

    while(running) {

        /* Ask for an instruction. */
        char** args = query_and_split_input();

        /* Executing the commands. */
        if (args == NULL) {  // error while reading input
            running = false;
            if(DEBUG != 0) printf("error while reading line: aborting shell\n");
        } else if (strcmp(args[0], "eof") == 0) {  // home-made "exit" command
            running = false;
            int count = count_words(args);
            split_line* line = form_split_line(args, count);  // to facilitate the freeing of memory
            free_split_line(line);
            if(DEBUG != 0) printf("aborting shell\n");
        } else {
            if(DEBUG != 0) printf("shell processing new command\n");

            /* Normal execution of a shell command. */
            int count = count_words(args);
            split_line* line = form_split_line(args, count);
            if(line == NULL) {  // in case an error occured: skip and ask new query
                free_split_line(line);
                continue;
            }

            if(line->thread_flag) {
                run_background_cmd(line);

            } else {
                if(DEBUG != 0) printf("ast expression is formed\n");
                Expression* ast = parse_line(line, 0, line->size - 1);
                eval(ast);

                destroy_expression(ast);  // todo: ensure eval has it inside (remove from here)
                free_split_line(line);
                if(DEBUG != 0) printf("exited eval\n");

//                run_shell(args);
            }

            if(DEBUG != 0) printf("done running shell on one command\n");
        }
    }

    /* We're all done here. See you! */
    printf("Bye!\n");
    exit (0);
}
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
            while (tmp-- != 0) {
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

    /* Forking. */
    int         status;
    pid_t       pid;
    pid = fork();


    if (pid < 0) {
        return false;

    } else if (pid == 0) {  // child
        if(DEBUG != 0) printf("Child executing the command.\n");

        /* Redirecting output. */
        if(cmd->redirect_flag) {
            const char* filePath = cmd->output_file;  // ex: "./test_output_file.txt";
            bool successfulWrite = writeOutputInFile(filePath);
            if(!successfulWrite) {
                // todo: in case of error
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
            return false;
        }
    }
}


void run_sequential_cmd(split_line* line) {
    Expression* ast = parse_line(line, 0, line->size - 1);
    eval(ast);

    destroy_expression(ast);
    free_split_line(line);
}

void run_background_cmd(split_line* line) {

    /* To replace the trailing '&' from the command. */
    free(line->content[line->size-1]);
    line->content[line->size-1] = NULL;
    Expression* ast = parse_line(line, 0, line->size - 2);

    /* Forking. */
    pid_t    pid;
    if(DEBUG != 0) printf("SUB___shell: just before fork.\n");
    pid = fork();

    if (pid < 0) {
        free_split_line(line);

    } else if (pid == 0) {  // child
        if(DEBUG != 0) printf("SUB___Child executing the command.\n\n");

        /* Executing the commands. */
        if(DEBUG != 0) printf("SUB___ast expression is formed\n");
        eval(ast);
        if(DEBUG != 0) printf("SUB___exited eval\n");

        /* Child process failed. */
        if(DEBUG != 0) printf("SUB___execvp didn't finish properly: running exit on child process\n");
        free_split_line(line);
        destroy_expression(ast);
        exit(-1);


    } else {  // back in parent
        free_split_line(line);
        destroy_expression(ast);

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
        free(input_str);
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

//    /* Forking. */
//    pid_t    pid;
//    if(DEBUG != 0) printf("CZ___shell: just before fork.\n");
//    pid = fork();
//
//    if (pid < 0) {
//        if(DEBUG != 0) printf("CZ___Error in the creation of the child.\n");
//
//    } else if (pid == 0) {  // child
//        if(DEBUG != 0) printf("CZ___Child executing the command.\n\n");
//
//    } else {  // back in parent
//        if(DEBUG != 0) printf("CZ___Terminating parent of the child.\n");
//    }
//
//    if(DEBUG != 0) printf("CZ___run_bg_cmd is over.\n");
}


int main (void) {
    /// Instanciates the main shell and queries the commands. Type "eof" to exit.

    fprintf (stdout, "%% ");
    /* ¡REMPLIR-ICI! : Lire les commandes de l'utilisateur et les exécuter. */

    bool running = true;


    /* To reap zombie-child processes created by using '&'. */
    struct sigaction zombie_reaper;
    zombie_reaper.sa_handler = &zombie_handler;
    sigemptyset(&zombie_reaper.sa_mask);
    zombie_reaper.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &zombie_reaper, NULL);

    /* To treat Ctrl-Z (Bonus #2). */
    struct sigaction bonus2_signal;
    bonus2_signal.sa_handler = &ctrl_Z_handler;
    sigemptyset(&bonus2_signal.sa_mask);
    bonus2_signal.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &bonus2_signal, NULL);

    /* Our shell. */
    while(running) {

        /* Ask for an instruction. */
        char** args = query_and_split_input();

        /* Error handling  |  exit  |  set up. */
        if (args == NULL) {  // error while reading input
            running = false;
            if(DEBUG != 0) printf("error while reading line: aborting shell\n");
        } else if (strcmp(args[0], "eof") == 0) {  // home-made "exit" command
            running = false;
            int count = count_words(args);
            split_line* line = form_split_line(args, count);  // to facilitate the freeing of memory
            free_split_line(line);
            if(DEBUG != 0) printf("exit command detected\n");
        } else {
            if(DEBUG != 0) printf("shell processing new command\n");
            int count = count_words(args);
            split_line* line = form_split_line(args, count);
            if(line == NULL) {  // in case an error occured: skip and ask new query
                free_split_line(line);
                continue;
            }

            /* Executing the commands. */
            if(line->thread_flag) {
                if(DEBUG != 0) printf("before bg cmd\n");
                run_background_cmd(line);
                if(DEBUG != 0) printf("after bg cmd\n");
            } else {
                if(DEBUG != 0) printf("before seq cmd\n");
                run_sequential_cmd(line);
                if(DEBUG != 0) printf("after seq cmd\n");
            }

            if(DEBUG != 0) printf("done running shell on one command\n");
        }
    }

    /* We're all done here. See you! */
    printf("Bye!\n");
    exit (0);
}
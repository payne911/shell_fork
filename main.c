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
 */
#include "expression.h"


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


/*
 * Memory management.
 */

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

        /* Error handling  |  exit  |  cat  |  set up. */
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
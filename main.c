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


/*
 * The "DEBUG" value is used to print trace of the flow of execution.
 * Any non-zero value will result in traces being printed.
 * Setting it to 0 will silence the debugging mode.
 */
#define DEBUG 0



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

void run_shell(char** args) {
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

int main (void) {
    /// Instanciates the main shell and queries the commands. Type "eof" to exit.

    fprintf (stdout, "%% ");
    /* ¡REMPLIR-ICI! : Lire les commandes de l'utilisateur et les exécuter. */

    int running = 0;

    while(running == 0) {

        /* Ask for a command. */
        char** args = query_and_split_input();

        /* Executing the commands. */
        if (args == NULL) {  // error while reading input
            running = 1;
            if(DEBUG != 0) printf("error while reading line: aborting shell\n");
        } else if (strcmp(args[0], "eof") == 0) {  // home-made "exit" command
            running = 1;
            // todo: free the sub-arrays (args[x])
            free(args);
            if(DEBUG != 0) printf("aborting shell\n");
        } else {
            if(DEBUG != 0) printf("shell processing new command\n");
            run_shell(args);
            if(DEBUG != 0) printf("done running shell on one command\n");
        }
    }

    /* We're all done here. See you! */
    printf("Bye!\n");
    exit (0);  // todo: why ???
}

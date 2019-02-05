#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "expression.h"

#define DEFAULT_BUFFER_SIZE 64


typedef struct split_line {
    int size;           // size of the array
    char** content;     // array of words
} split_line;



// function declaration

split_line* split_str       (char* str, const char delim[]);


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

size_t optimizer_cnt (const char *str) {
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


/**
 * Splits a string into array of of strings based on all delimiters specified
 * @param str
 * @param delim
 * @return
 */
split_line * split_str (char* str, const char delim[]) {
    /// Returns a list of pointers to strings that came from splitting the input.
    /// Returns NULL in case of error.

    if (strlen(str) == 1){
        split_line* sl = malloc(sizeof(split_line));
        sl->size = 0;
        sl->content = NULL;
        return sl;
    }

    char* copied_input = strdup(str);  // because 'strtok()' alters the string it passes through
    if (copied_input == 0) return NULL;  // OOM

    /* Counting content to optimize allocated's memory size. */
    size_t nwords = optimizer_cnt(copied_input);
    char** result;
    if ((result = malloc((nwords+1) * sizeof(char *))) == 0) return NULL;  // OOM

    /* Obtaining first word. */
    char* token = strtok (copied_input, delim);
    if (token == NULL) {
        //if(DEBUG != 0) printf("Bug extracting first word of split\n");
        free(result);
        free(copied_input);
        return NULL;
    } else if ((result[0] = strdup (token)) == 0) {  // OOM
        //if(DEBUG != 0) printf("Bug allocating first word of split\n");
        free(result);
        free(copied_input);
        return NULL;
    }

    /* To populate the whole array with the rest of the content. */
    int tmp = 0;
    while ((token = strtok (NULL, delim)) != NULL) {
        if ((result[++tmp] = strdup (token)) == 0) {
            /* In case of OOM, each past array must also be freed. */
            while (tmp-- != 0) {  // todo: inequality is right? (will free down to result[1] inclusively?)
                //if(DEBUG != 0) printf("Bug allocating content of split. Currently freeing index %d\n", tmp);
                free(result[tmp]);
            }
            free(result);
            free(copied_input);
            return NULL;
        }
    }

    result[++tmp] = NULL;  /* Must end with NULL */

    free(copied_input);

    if (result[0] == NULL){
        tmp = 0;
    }else if (str[strlen(str) - 2] == ' '){
        tmp--;
    }

    // printf("Size = %d\n", tmp);

    split_line* sl = malloc(sizeof(split_line));
    sl->content = result;
    sl->size = tmp;
    return sl;
}


/**
 * run a command line
 * @param command
 * @return
 */
int run_command(command * cmd){
    
    printf("\t%s\n", cmd->cmd[0]);
    if (cmd->redirect_flag) printf("\nREDIRECT TO %s", cmd->output_file);
    // if (cmd->background_flag) printf("DO IN BACKGROUND");
    return 1;
}


int main() {

    // system("clear");

    // read line continuously
    do {

        char* read_buffer = malloc(sizeof(char) * DEFAULT_BUFFER_SIZE);
        int buffer_size = DEFAULT_BUFFER_SIZE;

        int index = 0;
        char ch;

        printf("votre shell> ");
        do {
            scanf("%c", &ch);
            read_buffer[index++] = ch;
            if (index == buffer_size){
                read_buffer = realloc(read_buffer, (size_t)index+DEFAULT_BUFFER_SIZE);
                buffer_size = index+DEFAULT_BUFFER_SIZE;
            }

        } while(ch != '\n');

        // end string
        read_buffer[index] = '\0';

        if (strncmp(read_buffer, EXIT_SHELL, 4) == 0){
            free(read_buffer);
            break;
        }

        // here, read_buffer is the whole line
        split_line* line = split_str(read_buffer, " ");
        free(read_buffer);

        // no input
        if (line->size == 0){
            // printf("no input\n");
            continue;
        }
        Expression* ast = parse_line(line, 0, line->size - 1);
        //__debug_print(ast, 0);

        eval(ast);

        // once the line has been used, free memory
        destroy_expression(ast);
        free(line->content);
        free(line);

    } while (true);

    return 0;
}

#include <stdlib.h>
#include <string.h>

#include "tokenise.h"

size_t tokenise_string(char* input_string, char delim, char ***tokens)
{
    char *string = malloc(strlen(input_string) + 1);
    strcpy(string, input_string);
    size_t string_len = strlen(string);
    size_t previous_delim_location = 0;

    size_t num_tokens = 0;

    size_t i;
    for (i = 0; i < string_len; i++)
    {
        if (string[i] == delim || string[i] == '\n')
        {
            string[i] = 0;
            (*tokens) = realloc((*tokens), sizeof(*(*tokens))*(num_tokens + 1));
            (*tokens)[num_tokens] = malloc(strlen(string + previous_delim_location) + 1);
            strcpy((*tokens)[num_tokens], string + previous_delim_location);
            num_tokens++;
            previous_delim_location = i + 1;
        }
    }
    (*tokens) = realloc((*tokens), sizeof(*(*tokens))*(num_tokens + 1));
    (*tokens)[num_tokens] = malloc(strlen(string + previous_delim_location) + 1);
    strcpy((*tokens)[num_tokens], string + previous_delim_location);
    num_tokens++;

    return num_tokens;
}

#include <stdlib.h>
#include <string.h>

#include "tokenise.h"

size_t tokenise_string(char* input_string, char delim, char ***tokens)
{
    char *string = strdup(input_string);
    size_t string_len = strlen(string);
    //If the string ends in a \n we want to remove it.
    if (string[string_len-1] == '\n')
    {
        string[string_len-1] = '\0';
        string_len--;
    }

    size_t previous_delim_location = 0;

    int prev_state = 0;
    int curr_state;

    size_t num_tokens = 0;

    size_t i;
    for (i = 0; i < string_len; i++)
    {
        //check if current character matches the delimiter
        if (string[i] == delim)
            curr_state = 0;
        else
            curr_state = 1;

        if (prev_state == 0 && curr_state == 1) //word starts
        {
            previous_delim_location = i;
        }
        else if (prev_state == 1 && curr_state == 0) //word ends, add token
        {
            string[i] = 0;
            (*tokens) = realloc((*tokens), sizeof(*(*tokens))*(num_tokens + 1));
            (*tokens)[num_tokens] = malloc(strlen(string + previous_delim_location) + 1);
            strcpy((*tokens)[num_tokens], string + previous_delim_location);
            num_tokens++;
        }
        prev_state = curr_state;
    }
    //at the end of the string
    if (curr_state == 1) //there was a word right at the end of the string.
    {
        string[i] = 0;
        (*tokens) = realloc((*tokens), sizeof(*(*tokens))*(num_tokens + 1));
        (*tokens)[num_tokens] = malloc(strlen(string + previous_delim_location) + 1);
        strcpy((*tokens)[num_tokens], string + previous_delim_location);
        num_tokens++;
    }

    free(string);
    string = NULL;

    return num_tokens;
}

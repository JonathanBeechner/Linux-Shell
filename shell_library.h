#ifndef SHELL_LIBRARY_H
#define SHELL_LIBRARY_H

#include <stdarg.h>

// Functions for processing user input.
int shell_init(int, char*[]);
int shell_prompt(char[], const int);
char **shell_tokenize(char[]);
int shell_exec(char**);
void shell_free_tokens(char**);

// System functions for the shell.
int shell_exit(int, char**);
int shell_cd(int, char**);

// Other useful functions.
int shell_error(const char*, ...);
int shell_verror(const char*, va_list);



#endif // SHELL_LIBRARY_H
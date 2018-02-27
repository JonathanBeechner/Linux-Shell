#include "shell_library.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <wordexp.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>



// Variables for use in functions below.
bool utilshell_prompt_visible;
bool utilshell_colors;



// Utility functions: Only used within this source file.

int utilshell_print_prompt();
int utilshell_get_input(char[], const int);
char **utilshell_append_token(char*, char**, int, int*, int*);
int utilshell_exec(char**, bool);

/* Ok, you may be wondering why I have two functions called exec (shell_exec and utilshell_exec).
 * Basically, shell_exec is called from main(), and in turn, it will do some magic stuff and then
 *  call utilshell_exec. This basically allows the main() to look much cleaner, which I believe is
 * important.
 */



// --------------------------------------------------------------
// Functions for processing user input.
// --------------------------------------------------------------

// Initializes shell. Returns EXIT_SUCCESS on success, EXIT_FAILURE on error.
int shell_init(int argc, char *argv[]) {

    utilshell_prompt_visible = true;
    utilshell_colors = true;

    // Parse arguments.
        int c;
        while((c = getopt(argc, argv, "tc")) != -1) {
            switch(c) {
                case 't':
                    utilshell_prompt_visible = false;
                    break;

                case 'c':
                    utilshell_colors = false;
                    break;

                case '?':
                    break;

                default:
                    shell_error("An unknown error occured while parsing input arguments.\n");
                    return EXIT_FAILURE;
                    break;
            }

        }
    
    return EXIT_SUCCESS;

}

// Displays prompt and retrieves user input. Returns EXIT_SUCCESS on success, EXIT_FAILURE on error.
int shell_prompt(char buffer[], const int BUFFER_LEN) {

    if(utilshell_print_prompt() == EXIT_SUCCESS &&
        utilshell_get_input(buffer, BUFFER_LEN) == EXIT_SUCCESS)
        return EXIT_SUCCESS;
    else
        return EXIT_FAILURE;

    return EXIT_SUCCESS;

}

/* Reads buffer and returns an array of tokens. A token can be:
 *  a) Command string (eg. cat *.c)
 *  b) A pipe (|)
 *  c) A redirect (< or >)
 *  d) An ampersand (&)
 *  (to be added later)
 *  e) More redirects (2>, >>)
 * 
 * Returns a pointer to the list of tokens on success. Returns NULL otherwise.
 */
char **shell_tokenize(char buffer[]) {

    // This is probably unnecessary now, but it might be useful in the future.
    if(buffer == NULL)
        return NULL;

    int num_tokens = 0; // This is the current number of tokens in the list.
    int max_tokens = 4; // This is the number of tokens that can be used before reallocating the list.
    char **result = (char**)malloc((max_tokens+1)*sizeof(char*));

    // Tokenizing will be achived using a simple state machine. These are the states.
    const int NORMAL = 0;
    const int READING_QUOTE = 1;
    const int READING_ESCAPE = 2;
    const int READING_ESCAPE_IN_QUOTE = 3;

    // The state should begin and end at NORMAL. If it is not NORMAL after tokenizing, return NULL.
    int state = NORMAL;

    /* This is the index of the current token being read. Once the beginning of a new token is found,
     * everything from this point up to the current point is added to the token list.
     */
    int current_token_index = 0;

    // Iterate through the whole string using a simple state machine.
    int i;
    for(i = 0; buffer[i] != '\0'; ++i) {

        switch(state) {

            case NORMAL:

            char **new_tokens;
            int n;
            char *start;
            bool redir_err;
            bool redir_app;
            switch(buffer[i]) {

                case '|':
                case '>':
                case '<':
                case '&':

                redir_err = false;
                redir_app = false;

                if(buffer[i] == '>') {
                    if(buffer[i+1] == '>')
                        redir_app = true;
                    else if(i > 1 && buffer[i-1] == '2' && isspace(buffer[i-2]))
                        redir_err = true;
                }

                // We have found a special symbol!
                // If a previous token was being read, then add it.
                if(i - current_token_index > 0) {

                    // Add string to token list.
                    n = i - current_token_index;
                    if(redir_err)
                        n = n - 1;
                    new_tokens = utilshell_append_token(buffer+current_token_index, result, n, &num_tokens, &max_tokens);

                    if(new_tokens == NULL) {
                        shell_free_tokens(result);
                        return NULL;
                    } else {
                        result = new_tokens;
                    }

                }

                // Add the symbol to the token list.
                start = buffer + i;
                n = 1;
                if(redir_err) {
                    n = n + 1;
                    start = start - 1;
                } else if(redir_app) {
                    n = n + 1;
                }
                new_tokens = utilshell_append_token(start, result, n, &num_tokens, &max_tokens);

                if(new_tokens == NULL) {
                    shell_free_tokens(result);
                    return NULL;
                } else {
                    result = new_tokens;
                }
                
                // The next token starts right after this one.
                current_token_index = i + n;
                if(redir_app)
                    ++i;
                break;

                case '\\':
                state = READING_ESCAPE;
                break;

                case '\"':
                state = READING_QUOTE;
                break;

                default:
                break;
            }
            break;

            case READING_QUOTE:
            switch(buffer[i]) {
                case '\\':
                state = READING_ESCAPE_IN_QUOTE;
                break;

                case '\"':
                state = NORMAL;
                break;

                default:
                break;
            }
            break;

            case READING_ESCAPE:
            state = NORMAL;
            break;

            case READING_ESCAPE_IN_QUOTE:
            state = READING_QUOTE;
            break;

            default:
            shell_error("An unexpected error has occured in tokenizing the input string.");
            shell_free_tokens(result);
            return NULL;

        }
    
    }

    // If state is not NORMAL, then there was an unfinished escape sequence or non-terminated quotes.
    if(state != NORMAL) {
        shell_error("Could not tokenize input. state:%d\n", state);
        shell_free_tokens(result);
        return NULL;
    }

    // If we finished iterating through the buffer, then add last token to the list.
    if(i - current_token_index > 0) {
        char **new_tokens = utilshell_append_token(buffer+current_token_index, result, i - current_token_index, &num_tokens, &max_tokens);

        if(new_tokens == NULL) {
            shell_free_tokens(result);
            return NULL;
        } else {
            result = new_tokens;
        }
    }

    return result;

}

// Execute a list of tokens.
int shell_exec(char **tokens) {

    int argc = 0;
    for(int i = 0; tokens[i] != NULL; ++i)
        ++argc;

    if(strcmp(tokens[0], "exit") == 0) {

        return shell_exit(argc, tokens);

    } else if(strcmp(tokens[0], "cd") == 0) {

        return shell_cd(argc, tokens);

    } else {

        bool background = false;
        for(int i = 0; tokens[i] != NULL; ++i)
            if(strcmp(tokens[i], "&") == 0)
                background = true;

        if(fork() == 0) {
            utilshell_exec(tokens, background);
            exit(EXIT_SUCCESS);
        } else {
            if(!background)
                wait(NULL);
        }

    }

    return EXIT_SUCCESS;

}

// Free the memory pointed to by tokens.
void shell_free_tokens(char **tokens) {

    if(tokens == NULL)
        return;

    int i = 0;
    while(tokens[i] != NULL) {
        free(tokens[i]);
        ++i;
    }
    free(tokens);

}



// --------------------------------------------------------------
// Built in shell functions.
// --------------------------------------------------------------

int shell_exit(int argc, char **argv) {

    exit(EXIT_SUCCESS);
    return EXIT_SUCCESS; // This is probably really excessive, but whatever.

}

int shell_cd(int argc, char **argv) {

    if(argc > 1) {
        if(chdir(argv[1])) {
            shell_error("Cannot change to directory %s\n", argv[1]);
        }
    }
    
    return EXIT_SUCCESS;

}



// --------------------------------------------------------------
// Other useful functions.
// --------------------------------------------------------------

int shell_error(const char *fmt, ...) {
	va_list argp;
	va_start(argp, fmt);
	int result = shell_verror(fmt, argp);
	va_end(argp);
    return result;
}

int shell_verror(const char *fmt, va_list argp) {
    if(utilshell_colors)
        fprintf(stderr, "\033[4;31mERROR: ");
    else
        fprintf(stderr, "ERROR: ");

    vfprintf(stderr, fmt, argp);

    if(utilshell_colors)
        fprintf(stderr, "\033[0m");
}



// --------------------------------------------------------------
// Utility functions.
// --------------------------------------------------------------

// Prints the prompt (unless utilshell_prompt_visible is false).
int utilshell_print_prompt() {
    bool found_error = false;
    if(utilshell_prompt_visible) {
        // Get current working directory.
        char *cwd;
        if((cwd = get_current_dir_name()) == NULL) {
            int errsv = errno;
            shell_error("Could not retrieve current working directory. errno:%d\n", errsv);
            found_error = true;
        }

        // Get home directory.
        char *home_dir;
        if((getenv("HOME")) != NULL) {
            int len = strlen(getenv("HOME"));
            home_dir = (char*)malloc(len + 1);
            strncpy(home_dir, getenv("HOME"), len + 1);
        } else {
            shell_error("Could not retrieve home directory.\n");
            found_error = true;
        }
        
        // Use tilde '~' in place of home in cwd.
        char *dir_to_print;
        if((dir_to_print = strstr(cwd, home_dir)) == NULL)
            dir_to_print = cwd;
        else {
            dir_to_print += strlen(home_dir) - 1;
            dir_to_print[0] = '~';
        }

        // Get username.
        char *username;
        if((getenv("USER")) != NULL) {
            int len = strlen(getenv("USER"));
            username = (char*)malloc(len + 1);
            strncpy(username, getenv("USER"), len + 1);
        } else {
            shell_error("Could not retrieve user name.\n");
            found_error = true;
        }

        // Get host name.
        char host_name[HOST_NAME_MAX + 1];
        if(gethostname(host_name, HOST_NAME_MAX+1)) {
            int errsv = errno;
            shell_error("Could not retrieve host name. errno:%d\n", errsv);
            found_error = true;
        }

        // Print prompt.
        if(!found_error) {
            if(utilshell_colors)
                printf("\033[1;32m%s@%s \033[1;34m%s$ \033[0m", username, host_name, dir_to_print);
            else
                printf("%s@%s %s$ ", username, host_name, dir_to_print);
        } else
            printf("$ ");

        free(cwd);
        free(home_dir);
        free(username);
        // No need to free host_name.
    }

    return found_error ? EXIT_FAILURE : EXIT_SUCCESS;
}

// Retrieves user input.
int utilshell_get_input(char buffer[], const int BUFFER_LEN) {
    size_t line_len = 0;
    if(fgets(buffer, BUFFER_LEN, stdin) == NULL) {
        buffer[0] = '\0';
        if(ferror(stdin)) {
            shell_error("Could not read input.\n");
            return EXIT_FAILURE;
        }
    }

    if(buffer[strlen(buffer) - 1] == '\n')
        buffer[strlen(buffer) - 1] = '\0';

    return EXIT_SUCCESS;
}

/* Appends a token to the token list.
 *    token is a pointer to the beginning of the token.
 *    tokens is the actual token list.
 *    n is the length of the token.
 *    num_tokens is a pointer to the number of tokens currently in the list (it will be auto-incremented if needed).
 *    max_tokens is a pointer to the max number of tokens that can be put in the list before reallocation (it will be increased if needed).
 * Returns a pointer to the token list on success. This may not be the same as the original address passed to the function if reallocation
 * was necessary. On failure, the function returns NULL.
 */
char **utilshell_append_token(char *token, char **tokens, int n, int *num_tokens, int *max_tokens) {

    // Try to use word expansion on the token.
    
    int new_num_tokens; // This will be the new number of tokens in the list.

    // Copy the token string and try to expand it.
    char *just_token = (char*)malloc((n+1)*sizeof(char));
    strncpy(just_token, token, n);
    just_token[n] = '\0';

    wordexp_t p;
    char **w;

    int wp = wordexp(just_token, &p, 0);

    // Was the expansion successful?
    // If so, we are going to prepare to add all the tokens returned from wordexp.
    // If not, we are going to prepare to add only the token passed to the function.
    if(wp == 0) {

        // If word expansion was successful ...
        w = p.we_wordv;

        // Get number of tokens to add to list.
        new_num_tokens = *num_tokens;
        for(int i = 0; w[i] != NULL; ++i)
            ++new_num_tokens;

    } else {
        new_num_tokens = *num_tokens + 1;
    }

    // Resize token list if needed.
    if(new_num_tokens > *max_tokens) {

        // Find a new size for the list that can hold all the tokens.
        int new_max_tokens = *max_tokens * 2;
        while(new_num_tokens > new_max_tokens)
            new_max_tokens *= 2; 
        
        // Resize the list. Add one for the NULL terminator.
        char **new_tokens = (char**)realloc(tokens, (new_max_tokens+1)*sizeof(char*));
        if(new_tokens == NULL) {
            shell_error("Error in reallocating token list from size %d to new size %d.\n", max_tokens, new_max_tokens);
            return NULL;
        }

        *max_tokens = new_max_tokens;
        tokens = new_tokens;
    }

    if(wp == 0) {

        // If word expansion was successful, then copy those tokens.
        for(int i = 0; i < new_num_tokens - *num_tokens; ++i) {
            tokens[i + *num_tokens] = (char*)malloc((strlen(w[i]) + 1) * sizeof(char));
            strncpy(tokens[i + *num_tokens], w[i], strlen(w[i]) + 1);
        }
        wordfree(&p);

    } else {

        // If word expansion was not successful, then copy the original token.
        tokens[*num_tokens] = (char*)malloc((n+1)*sizeof(char));
        strncpy(tokens[*num_tokens], just_token, n+1);
    }

    tokens[new_num_tokens] = NULL;
    *num_tokens = new_num_tokens;

    free(just_token);

    return tokens;

}

int utilshell_exec(char **tokens, bool background) {

    if(tokens == NULL)
        return EXIT_SUCCESS;
    if(tokens[0] == NULL)
        return EXIT_SUCCESS;

    bool found_error = false;
    int errsv;

    int num_tokens;
    // Allocate space for args.
    num_tokens = 0;
    for(int i = 0; tokens[i] != NULL; ++i)
        ++num_tokens;

    char **args = NULL;
    args = (char**)malloc((num_tokens + 1)*sizeof(char**));
    args[0] = NULL;

    // Copy over args from tokens. Do not copy special symbols (eg. Do not copy '<' and its corresponding file arg).
    char *redir_in = NULL;
    char *redir_out = NULL;
    char *redir_app = NULL;
    char *redir_err = NULL;
    char **after_pipe = NULL;

    for(int i = 0, j = 0; tokens[i] != NULL; ++i) {

        if(strcmp(tokens[i], "|") == 0) {

            if(tokens[i+1] != NULL)
                after_pipe = tokens + i + 1;

            break; // We don't want to copy anything after the pipe to the args list.

        } else if(strcmp(tokens[i], "<") == 0) {

            // If there is not an argument, we will not throw an error. It's not a big deal.
            if(tokens[i+1] != NULL)
                redir_in = tokens[i+1];
            
            // Skip next token.
            ++i;

        } else if(strcmp(tokens[i], ">") == 0) {

            // If there is not an argument, we will not throw an error. It's not a big deal.
            if(tokens[i+1] != NULL)
                redir_out = tokens[i+1];
            
            // Skip next token.
            ++i;

        } else if(strcmp(tokens[i], ">>") == 0) {

            // If there is not an argument, we will not throw an error. It's not a big deal.
            if(tokens[i+1] != NULL)
                redir_app = tokens[i+1];
            
            // Skip next token.
            ++i;

        } else if(strcmp(tokens[i], "2>") == 0) {

            // If there is not an argument, we will not throw an error. It's not a big deal.
            if(tokens[i+1] != NULL)
                redir_err = tokens[i+1];
            
            // Skip next token.
            ++i;

        } else if(strcmp(tokens[i], "&") == 0) {

            // Do nothing. This case is taken care of in shell_exec().

        } else {
            args[j] = tokens[i];
            args[++j] = NULL;
        }

    }

    // Execute.
    if(args[0] != NULL) {
        if(fork() == 0) {

            int fd_in = -1;
            int fd_out = -1;
            int fd_app = -1;
            int fd_err = -1;

            // Handle redirects.
            if(redir_in != NULL) {
                fd_in = open(redir_in, O_RDONLY);

                if(fd_in == -1) {
                    shell_error("Could not open \"%s\" for reading.\n", redir_in);
                } else {
                    dup2(fd_in, fileno(stdin));
                }
            }

            if(redir_out != NULL) {
                fd_out = open(redir_out, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);

                if(fd_out == -1) {
                    shell_error("Could not open \"%s\" for writing.\n", redir_out);
                } else {
                    dup2(fd_out, fileno(stdout));
                }
            }

            if(redir_app != NULL) {
                fd_app = open(redir_app, O_WRONLY|O_CREAT|O_APPEND, S_IRWXU);

                if(fd_app == -1) {
                    shell_error("Could not open \"%s\" for writing.\n", redir_app);
                } else {
                    dup2(fd_app, fileno(stdout));
                }
            }

            if(redir_err != NULL) {
                fd_err = open(redir_err, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);

                if(fd_err == -1) {
                    shell_error("Could not open \"%s\" for writing.\n", redir_err);
                } else {
                    dup2(fd_err, fileno(stderr));
                }
            }

            // Execute (pipe if needed).
            if(after_pipe != NULL) {

                int fd[2];
                pipe(fd);

                if(fork() == 0) {
                    
                    close(fd[0]);
                    dup2(fd[1], fileno(stdout));

                    if(execvp(args[0], args) == -1) {
                        errsv = errno;
                        shell_error("Could not execute \"%s\". errno:%d\n", args[0], errsv);
                        found_error = true;
                    }

                    close(fd[1]);

                } else {

                    close(fd[1]);
                    dup2(fd[0], fileno(stdin));
                    wait(NULL);

                    utilshell_exec(after_pipe, background);

                    close(fd[0]);

                }

            } else {

                if(execvp(args[0], args) == -1) {
                    errsv = errno;
                    shell_error("Could not execute \"%s\". errno:%d\n", args[0], errsv);
                    found_error = true;
                }
            
            }

            close(fd_in);
            close(fd_out);

        } else {
            if(!background)
                wait(NULL);
        }
    }

    free(args);

    return EXIT_SUCCESS;

}
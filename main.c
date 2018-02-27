#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/utsname.h>

#include "shell_library.h"

int main(int argc, char *argv[]) {
        
    // Variables for user input.
    const int BUFFER_LEN = 4096;
    char buffer[BUFFER_LEN];

    // Initialize prompt.
    if(shell_init(argc, argv) != EXIT_SUCCESS) {
        shell_error("Could not initialize prompt.\n");
        return EXIT_FAILURE;
    }

    bool is_running = true;
    while(is_running) {

        // Print prompt and retrieve user input.
        if(shell_prompt(buffer, BUFFER_LEN) != EXIT_SUCCESS) {
            shell_error("Could not retrieve input.\n");
        }

        // Tokenize input (separate by command/|/</>/&).
        char **tokens;
        
        if((tokens = shell_tokenize(buffer)) == NULL) {
            shell_error("Could not tokenize input.\n");
        }

        // Execute command.
        if(shell_exec(tokens) != EXIT_SUCCESS) {
            shell_error("Could not execute input.\n");
        }

        // Free memory.
        shell_free_tokens(tokens);

    }

    return EXIT_SUCCESS;

}
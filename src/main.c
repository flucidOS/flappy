#include "flappy.h"
#include "env.h"

/**
 * main - Entry point for the flappy application
 * @argc: Argument count
 * @argv: Argument vector
 *
 * Description: Initializes logging and dispatches command-line arguments
 * to the appropriate handler.
 *
 * Return: Exit status code from cli_dispatch()
 */
int main(int argc, char **argv) {

    flappy_env_init();

    /* Step 2: Initialize logging */
    log_init();
    log_info("flappy invoked");

    /* Step 3: Dispatch CLI */
    return cli_dispatch(argc, argv);

}

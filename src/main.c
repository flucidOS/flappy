#include "flappy.h"

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
    log_init();
    log_info("flappy invoked");

    return cli_dispatch(argc, argv);
}

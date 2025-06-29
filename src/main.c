/* main.c
 * @file main.c
 * @brief Entry point and command-line handling for BasicCodeCompiler (bcc).
 *
 * Responsibilities:
 *  - Define compiler metadata and error/option enums
 *  - Parse and validate command-line options
 *  - Call into compile_file() for the actual compilation work
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>
#include <libgen.h>

#include "../include/compile.h"
#include "../include/shell_command_runner.h"

#define _POSIX_C_SOURCE 200809L
#define PATH_MAX 4096

/* Compiler identity */
#define COMPILER_NAME "BasicCodeCompiler (bcc)"
#define VERSION_MAJOR 0
#define VERSION_MINOR 3
#define VERSION_PATCH 1

/**
 * @brief Prints the version of the compiler.
 */
static void print_version(void) {
    printf("%s v%d.%d.%d\n",
           COMPILER_NAME,
           VERSION_MAJOR,
           VERSION_MINOR,
           VERSION_PATCH);
}

/**
 * @brief Prints usage information to stderr.
 */
static void print_usage(const char *program_name) {
    fprintf(stderr,
            "Usage: %s [options] <input-file>\n"
            "Options:\n"
            "  -h, --help            Show this help message\n"
            "  -v, --version         Show version information\n"
            "  -t, --tokens          Display token stream\n"
            "  -a, --ast             Display abstract syntax tree\n"
            "  -g, --show-registers  Show register allocation details\n"
            "  -r, --arch=<arch>     Specify target architecture (ARM)\n"
            "  -s, --save-assembly   Save the generated assembly file\n"
            "  -o <output>           Specify output executable name\n",
            program_name);
}

/**
 * @brief Removes the file extension from a filename string in-place.
 */
static void strip_extension(const char *filename) {
    char *dot = strrchr(filename, '.');
    if (dot) *dot = '\0';
}

/**
 * @brief Parses command-line options into a CompilerOptions struct.
 */
static CompilerOptions parse_options(int argc, char *argv[], ErrorCode *err) {
    CompilerOptions opts = {0};
    opts.target_arch = ARCH_ARM;
    *err = ERR_OK;

    static struct option long_opts[] = {
        {"help",            no_argument,       0, 'h'},
        {"version",         no_argument,       0, 'v'},
        {"tokens",          no_argument,       0, 't'},
        {"ast",             no_argument,       0, 'a'},
        {"show-registers",  no_argument,       0, 'g'},
        {"arch",            required_argument, 0, 'r'},
        {"save-assembly",   no_argument,       0, 's'},
        {0,0,0,0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hvtagr:so:", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'h': print_usage(argv[0]);  exit(EXIT_SUCCESS);
            case 'v': print_version();       exit(EXIT_SUCCESS);
            case 't': opts.show_tokens = true;      break;
            case 'a': opts.show_ast = true;         break;
            case 'g': opts.show_registers = true;   break;
            case 's': opts.save_asm = true;         break;
            case 'r':
                if (strcasecmp(optarg, "ARM") == 0) {
                    opts.target_arch = ARCH_ARM;
                } else {
                    fprintf(stderr, "Unsupported architecture: %s\n", optarg);
                    *err = ERR_INVALID_ARCH;
                    return opts;
                }
                break;
            case 'o':
                strncpy(opts.output_name, optarg,
                        sizeof(opts.output_name)-1);
                break;
            default:
                *err = ERR_UNKNOWN_OPTION;
                return opts;
        }
    }

    if (optind < argc) {
        // Store the original input path for directory extraction
        const char *input_path = argv[optind];

        // Set opts.filename to just the base filename
        char tmp_filename[PATH_MAX];
        static char base_filename[PATH_MAX];
        strncpy(tmp_filename, input_path, sizeof(tmp_filename) - 1);
        tmp_filename[sizeof(tmp_filename) - 1] = '\0';
        strncpy(base_filename, basename(tmp_filename), sizeof(base_filename) - 1);
        base_filename[sizeof(base_filename) - 1] = '\0';
        opts.filename = base_filename;

        // Set output name if not provided
        if (opts.output_name[0] == '\0') {
            strncpy(tmp_filename, opts.filename, sizeof(tmp_filename) - 1);
            tmp_filename[sizeof(tmp_filename) - 1] = '\0';
            strncpy(opts.output_name, tmp_filename, sizeof(opts.output_name) - 1);
            opts.output_name[sizeof(opts.output_name) - 1] = '\0';
        }
        strip_extension(opts.output_name);

        // Get absolute directory path of the input file
        static char abs_path[PATH_MAX];
        static char dir_path[PATH_MAX];
        if (realpath(input_path, abs_path)) {
            strncpy(dir_path, abs_path, sizeof(dir_path) - 1);
            dir_path[sizeof(dir_path) - 1] = '\0';
            opts.file_directory_path = dirname(dir_path);
        } else {
            opts.file_directory_path = NULL;
        }
    } else {
        *err = ERR_NO_INPUT_FILE;
    }

    opts.is_executable = true; // Default to generating an executable
    return opts;
}

/**
 * @brief Program entry point.
 */
int main(const int argc, char *argv[]) {
    ErrorCode err;
    const CompilerOptions opts = parse_options(argc, argv, &err);

    if (err != ERR_OK || !opts.filename) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    run_command("rm -rf tmp"); // Clean up old tmp directory

    return compile_file(&opts) == 0
           ? EXIT_SUCCESS
           : EXIT_FAILURE;
}

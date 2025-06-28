/**
 * @file main.c
 * @brief Entry point for the BasicCodeCompiler (bcc).
 *
 * This file implements the main driver program for the compiler,
 * handling command-line arguments, reading source files,
 * lexing, parsing, register allocation, code generation, and final linking.
 */

#define _POSIX_C_SOURCE 200809L
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/register_allocator.h"
#include "../include/codegen_arm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define COMPILER_NAME "BasicCodeCompiler (bcc)"
#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 0
#define MAX_FILE_SIZE (1024 * 1024) // 1MB

/**
 * @enum ErrorCode
 * @brief Enumeration of possible compiler error codes.
 */
typedef enum {
    ERR_OK = 0,
    ERR_FILE_OPEN,
    ERR_FILE_SEEK,
    ERR_FILE_TELL,
    ERR_FILE_SIZE,
    ERR_MEM_ALLOC,
    ERR_FILE_READ,
    ERR_LEXICAL,
    ERR_SYNTAX,
    ERR_UNKNOWN_OPTION,
    ERR_MULTIPLE_FILES,
    ERR_NO_INPUT_FILE,
    ERR_INVALID_ARCH
} ErrorCode;

/**
 * @enum Architecture
 * @brief Supported target architectures.
 */
typedef enum {
    ARCH_ARM
} Architecture;

/**
 * @struct CompilerOptions
 * @brief Stores command-line options passed to the compiler.
 */
typedef struct {
    bool show_tokens;
    bool show_ast;
    bool show_registers;
    bool save_asm;
    Architecture target_arch;
    const char *filename;
    char output_name[256];
} CompilerOptions;

/**
 * @struct CompilationContext
 * @brief Holds compilation state during compilation phases.
 */
typedef struct {
    TokenStream *token_stream;
    ASTNode *ast_root;
    Architecture target_arch;
} CompilationContext;

/**
 * @brief Prints the version of the compiler.
 * @description The version is defined by VERSION_MAJOR, VERSION_MINOR, and VERSION_PATCH.
 * Major is incremented for the release of a stable version.
 * Minor is incremented for new features or improvements.
 * Patch is incremented for bug fixes or minor changes.
 */
static void print_version(void) {
    printf("%s v%d.%d.%d\n", COMPILER_NAME, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
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
            "  -o <output>           Specify name of the output executable\n",
            program_name);
}

/**
 * @brief Removes the file extension from a filename string in-place.
 * @param filename The filename string to modify.
 */
static void strip_extension(const char *filename) {
    char *dot = strrchr(filename, '.');
    if (dot) *dot = '\0';
}

/**
 * @brief Parses command-line options.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @param err Pointer to ErrorCode to store any error.
 * @return CompilerOptions struct with parsed options.
 */
static CompilerOptions parse_options(const int argc, char *argv[], ErrorCode *err) {
    CompilerOptions options = {0};
    options.target_arch = ARCH_ARM;
    *err = ERR_OK;

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"tokens", no_argument, 0, 't'},
        {"ast", no_argument, 0, 'a'},
        {"output", required_argument, 0, 'o'},
        {"show-registers", no_argument, 0, 'g'},
        {"arch", required_argument, 0, 'r'},
        {"save-assembly", no_argument, 0, 's'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hvtagr:so:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h': print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            case 'v': print_version();
                exit(EXIT_SUCCESS);
            case 't': options.show_tokens = true;
                break;
            case 'a': options.show_ast = true;
                break;
            case 'r':
                if (strcasecmp(optarg, "ARM") == 0) {
                    options.target_arch = ARCH_ARM;
                } else {
                    fprintf(stderr, "Unsupported architecture: %s\n", optarg);
                    *err = ERR_INVALID_ARCH;
                    return options;
                }
                break;
            case 's': options.save_asm = true;
                break;
            case 'o': strncpy(options.output_name, optarg, sizeof(options.output_name) - 1);
                break;
            case 'g': options.show_registers = true;
                break;
            default: *err = ERR_UNKNOWN_OPTION;
                return options;
        }
    }

    if (optind < argc) {
        options.filename = argv[optind];
        if (options.output_name[0] == '\0') {
            char temp[256];
            strncpy(temp, options.filename, sizeof(temp) - 1);
            temp[sizeof(temp) - 1] = '\0';
            const char *base = basename(temp);
            strncpy(options.output_name, base, sizeof(options.output_name) - 1);
            options.output_name[sizeof(options.output_name) - 1] = '\0';
        }
    } else {
        *err = ERR_NO_INPUT_FILE;
    }

    strip_extension(options.output_name);
    return options;
}

/**
 * @brief Reads entire file contents into memory.
 * @param filename Path to file to read.
 * @param[out] buffer Pointer to allocated buffer with file contents.
 *                    Must be freed by caller.
 * @param[out] size Size of the file contents read.
 * @return ErrorCode indicating success or failure.
 */
static int read_file(const char *filename, char **buffer, size_t *size) {
    FILE *file = fopen(filename, "rb");
    if (!file) return ERR_FILE_OPEN;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ERR_FILE_SEEK;
    }

    const long file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return ERR_FILE_TELL;
    }
    if (file_size > MAX_FILE_SIZE) {
        fclose(file);
        return ERR_FILE_SIZE;
    }
    rewind(file);

    *buffer = malloc(file_size + 1);
    if (!*buffer) {
        fclose(file);
        return ERR_MEM_ALLOC;
    }

    const size_t read_bytes = fread(*buffer, 1, file_size, file);
    fclose(file);

    if (read_bytes != (size_t) file_size) {
        free(*buffer);
        return ERR_FILE_READ;
    }

    (*buffer)[read_bytes] = '\0';
    *size = read_bytes;
    return ERR_OK;
}

/**
 * @brief Executes a shell command.
 * @param cmd Command string to execute.
 * @return Status code from system() call.
 */
static int run_command(const char *cmd) {
    const int status = system(cmd);
    if (status != 0) {
        fprintf(stderr, "Command failed: %s\n", cmd);
    }
    return status;
}

/**
 * @brief Frees tokens in a token stream and resets the stream.
 * @param stream Pointer to the TokenStream to cleanup.
 */
static void token_stream_cleanup(TokenStream *stream) {
    for (size_t i = 0; i < stream->count; i++) {
        token_cleanup(&stream->tokens[i]);
    }
    free(stream->tokens);
    stream->tokens = NULL;
    stream->count = 0;
    stream->capacity = 0;
}

/**
 * @brief Prints the tokens in the token stream.
 * @param stream Pointer to the TokenStream to print.
 */
static void print_token_stream(const TokenStream *stream) {
    printf("\nToken Stream:\n========================================\n");
    for (size_t i = 0; i < stream->count; i++) {
        const Token token = stream->tokens[i];
        printf("%-12s Line %-3d", token_type_to_string(token.type), token.line);
        if (token.lexeme) printf(" '%s'", token.lexeme);
        printf("\n");
    }
    printf("========================================\n");
}

/**
 * @brief Frees AST and token stream resources in the compilation context.
 * @param ctx Pointer to CompilationContext to cleanup.
 */
static void cleanup_context(CompilationContext *ctx) {
    if (ctx->ast_root) {
        free_ast(ctx->ast_root);
        ctx->ast_root = NULL;
    }
    if (ctx->token_stream) {
        token_stream_cleanup(ctx->token_stream);
        ctx->token_stream = NULL;
    }
}

/**
 * @brief Performs the lexing phase.
 * @param source Source code string.
 * @param stream Pointer to TokenStream to populate.
 * @return Number of lexical errors found.
 */
static int lex_phase(const char *source, TokenStream *stream) {
    Lexer lexer = lexer_create(source);
    int error_count = 0;

    while (true) {
        const Token token = lexer_next_token(&lexer);
        token_stream_add(stream, token);
        if (token.type == TOKEN_ERROR) error_count++;
        if (token.type == TOKEN_EOF) break;
    }

    return error_count;
}

/**
 * @brief Performs the parsing phase.
 * @param ctx Compilation context holding tokens.
 * @param show_ast Whether to print the AST.
 * @return Number of parsing errors found.
 */
static int parse_phase(CompilationContext *ctx, bool show_ast) {
    Parser parser = parser_create(ctx->token_stream);
    const int error_count = parse(&parser);

    if (error_count == 0) {
        ctx->ast_root = parser.ast_root;
        parser.ast_root = NULL;

        if (show_ast) {
            printf("\nAST:\n========================================\n");
            print_ast(ctx->ast_root, 0);
            printf("========================================\n");
        }
    } else if (parser.ast_root) {
        free_ast(parser.ast_root);
        parser.ast_root = NULL;
    }

    parser_cleanup(&parser);
    return error_count;
}

/**
 * @brief Main compilation function.
 * Reads source, lexes, parses, allocates registers, generates assembly, and links.
 * @param opts Compiler options.
 * @return ErrorCode indicating success or failure.
 */
static int compile_file(const CompilerOptions *opts) {
    char *source = NULL;
    size_t size = 0;

    if (read_file(opts->filename, &source, &size) != ERR_OK) {
        fprintf(stderr, "Failed to open or read file: %s\n", opts->filename);
        return ERR_FILE_OPEN;
    }

    CompilationContext ctx = {0};
    TokenStream tokens = {0};

    const int lex_errors = lex_phase(source, &tokens);
    free(source);

    if (lex_errors > 0) {
        fprintf(stderr, "Lexical errors: %d\n", lex_errors);
        token_stream_cleanup(&tokens);
        return ERR_LEXICAL;
    }

    if (opts->show_tokens) {
        print_token_stream(&tokens);
    }

    ctx.token_stream = &tokens;
    ctx.target_arch = opts->target_arch;

    if (parse_phase(&ctx, opts->show_ast) > 0) {
        fprintf(stderr, "Syntax error occurred.\n");
        cleanup_context(&ctx);
        return ERR_SYNTAX;
    }

    if (ctx.target_arch == ARCH_ARM) {
        register_allocate_ast(ctx.ast_root, opts->show_registers);

        // Generate assembly output file path
        char asm_file[300];
        snprintf(asm_file, sizeof(asm_file), "%s.s", opts->output_name);

        FILE *asm_out = fopen(asm_file, "w");
        if (!asm_out) {
            cleanup_context(&ctx);
            return ERR_FILE_OPEN;
        }

        // Redirect stdout to assembly output file to capture codegen output
        const int saved_stdout = dup(fileno(stdout));
        fflush(stdout);
        dup2(fileno(asm_out), fileno(stdout));

        codegen_arm(ctx.ast_root);

        fflush(stdout);
        dup2(saved_stdout, fileno(stdout));
        close(saved_stdout);
        fclose(asm_out);

        // Make generate_executable.sh executable
        if (run_command("chmod +x ./scripts/generate_executable.sh") != 0) {
            cleanup_context(&ctx);
            return ERR_FILE_OPEN;
        }

        // Run the generate_executable.sh script to assemble and link
        char run_script_cmd[512];
        snprintf(run_script_cmd, sizeof(run_script_cmd), "./scripts/generate_executable.sh %s", asm_file);
        if (run_command(run_script_cmd) != 0) {
            cleanup_context(&ctx);
            return ERR_FILE_OPEN;
        }

        if (!opts->save_asm) {
            remove(asm_file);
        }
    }

    printf("Compilation and linking successful for target: ARM\n");
    cleanup_context(&ctx);
    return ERR_OK;
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

    return (compile_file(&opts) == ERR_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

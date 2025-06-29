/* compile.c
 * @file compile.c
 * @brief Implementation of compile_file() for BasicCodeCompiler (bcc).
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../include/compile.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/register_allocator.h"
#include "../include/codegen_arm.h"

/** Maximum input file size (1 MiB) */
static const size_t MAX_FILE_SIZE = 1024 * 1024;

/**
 * @struct CompilationContext
 * @brief Holds intermediate state during compilation.
 */
typedef struct {
    TokenStream   *token_stream;  /**< Pointer to token stream */
    ASTNode       *ast_root;      /**< Root of the AST */
    Architecture   target_arch;   /**< Target architecture */
} CompilationContext;

/**
 * @brief Read an entire file into a malloc’d buffer.
 *
 * @param filename  Path to input file.
 * @param out_buf   Receives pointer to malloc’d buffer (must be free()’d).
 * @param out_len   Receives length of buffer (excluding NUL).
 * @return          ERR_OK on success or an appropriate ErrorCode on failure.
 */
static ErrorCode read_file(const char *filename, char **out_buf, size_t *out_len) {
    FILE *f = fopen(filename, "rb");
    if (!f) return ERR_FILE_OPEN;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return ERR_FILE_SEEK; }
    long size = ftell(f);
    if (size < 0)            { fclose(f); return ERR_FILE_TELL; }
    if ((size_t)size > MAX_FILE_SIZE) {
        fclose(f);
        return ERR_FILE_SIZE;
    }
    rewind(f);

    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return ERR_MEM_ALLOC; }

    size_t read = fread(buf, 1, size, f);
    if (read != (size_t)size) {
        free(buf);
        fclose(f);
        return ERR_FILE_READ;
    }
    buf[size] = '\0';
    fclose(f);

    *out_buf = buf;
    *out_len = (size_t)size;
    return ERR_OK;
}

/**
 * @brief Execute a shell command, reporting failures.
 *
 * @param cmd  Null-terminated command string.
 * @return     system() return code.
 */
static int run_command(const char *cmd) {
    int status = system(cmd);
    if (status != 0) {
        fprintf(stderr, "Command failed: %s\n", cmd);
    }
    return status;
}

/**
 * @brief Free all tokens in a TokenStream.
 *
 * @param ts  TokenStream to clean up.
 */
static void cleanup_token_stream(TokenStream *ts) {
    for (size_t i = 0; i < ts->count; ++i) {
        token_cleanup(&ts->tokens[i]);
    }
    free(ts->tokens);
    ts->tokens = NULL;
    ts->count = ts->capacity = 0;
}

/**
 * @brief Print the token stream to stdout.
 *
 * @param ts  TokenStream to print.
 */
static void print_tokens(const TokenStream *ts) {
    printf("\nToken Stream:\n-------------------------------\n");
    for (size_t i = 0; i < ts->count; ++i) {
        const Token *t = &ts->tokens[i];
        printf("%-12s Line %-3d '%s'\n",
               token_type_to_string(t->type),
               t->line,
               t->lexeme ? t->lexeme : "");
    }
    printf("-------------------------------\n");
}

/**
 * @brief Free AST and token resources in the context.
 *
 * @param ctx  CompilationContext to clean up.
 */
static void cleanup_context(CompilationContext *ctx) {
    if (ctx->ast_root) {
        free_ast(ctx->ast_root);
        ctx->ast_root = NULL;
    }
    if (ctx->token_stream) {
        cleanup_token_stream(ctx->token_stream);
        ctx->token_stream = NULL;
    }
}

/**
 * @brief Perform lexing: read tokens into the TokenStream.
 *
 * @param source  Source code string.
 * @param ts      Pointer to TokenStream to populate.
 * @return        Number of lexical errors found.
 */
static int lex_phase(const char *source, TokenStream *ts) {
    Lexer lex = lexer_create(source);
    int errors = 0;
    while (true) {
        Token t = lexer_next_token(&lex);
        token_stream_add(ts, t);
        if (t.type == TOKEN_ERROR) ++errors;
        if (t.type == TOKEN_EOF) break;
    }
    return errors;
}

/**
 * @brief Perform parsing: build AST from tokens.
 *
 * @param ctx       CompilationContext holding tokens.
 * @param show_ast  If true, print the AST to stdout.
 * @return          Number of syntax errors found.
 */
static int parse_phase(CompilationContext *ctx, bool show_ast) {
    Parser p = parser_create(ctx->token_stream);
    int errors = parse(&p);
    if (errors == 0) {
        ctx->ast_root = p.ast_root;
        p.ast_root = NULL;
        if (show_ast) {
            printf("\nAST:\n-------------------------------\n");
            print_ast(ctx->ast_root, 0);
            printf("-------------------------------\n");
        }
    } else if (p.ast_root) {
        free_ast(p.ast_root);
    }
    parser_cleanup(&p);
    return errors;
}

/**
 * @brief Top-level compilation function.
 *
 * Reads source from disk, lexes, parses, allocates registers,
 * emits assembly, and invokes the linker script.
 *
 * @param opts  CompilerOptions describing flags and file names.
 * @return      ERR_OK on success or an ErrorCode on failure.
 */
ErrorCode compile_file(const CompilerOptions *opts) {
    char *source = NULL;
    size_t src_len = 0;
    ErrorCode er = read_file(opts->filename, &source, &src_len);
    if (er != ERR_OK) {
        fprintf(stderr, "Error reading '%s'\n", opts->filename);
        return er;
    }

    CompilationContext ctx = {0};
    TokenStream ts = {0};

    int lex_errs = lex_phase(source, &ts);
    free(source);
    if (lex_errs > 0) {
        fprintf(stderr, "Lexical errors: %d\n", lex_errs);
        cleanup_token_stream(&ts);
        return ERR_LEXICAL;
    }
    if (opts->show_tokens) {
        print_tokens(&ts);
    }

    ctx.token_stream = &ts;
    ctx.target_arch  = opts->target_arch;

    if (parse_phase(&ctx, opts->show_ast) > 0) {
        fprintf(stderr, "Syntax errors detected.\n");
        cleanup_context(&ctx);
        return ERR_SYNTAX;
    }

    /* Register allocation and backend codegen */
    register_allocate_ast(ctx.ast_root, opts->show_registers);

    /* Emit ARM assembly */
    char asm_path[300];
    snprintf(asm_path, sizeof(asm_path), "%s.s", opts->output_name);
    FILE *asm_out = fopen(asm_path, "w");
    if (!asm_out) {
        cleanup_context(&ctx);
        return ERR_FILE_OPEN;
    }

    /* Temporarily redirect stdout to assembly file */
    int saved_stdout = dup(fileno(stdout));
    fflush(stdout);
    dup2(fileno(asm_out), fileno(stdout));
    codegen_arm(ctx.ast_root);
    fflush(stdout);
    dup2(saved_stdout, fileno(stdout));
    close(saved_stdout);
    fclose(asm_out);

    /* Link using provided script */
    run_command("chmod +x ./scripts/generate_executable.sh");
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "./scripts/generate_executable.sh %s", asm_path);
    run_command(cmd);

    /* Clean up .s if not requested */
    if (!opts->save_asm) {
        remove(asm_path);
    }

    printf("Compilation and linking succeeded for target ARM\n");

    cleanup_context(&ctx);
    return ERR_OK;
}

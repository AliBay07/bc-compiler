/* compile.c
 * @file compile.c
 * @brief Implementation of compile_file() for BasicCodeCompiler (bcc).
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include "../include/compile.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/register_allocator.h"
#include "../include/codegen_arm.h"

/** Maximum input file size (1 MiB) */
static const size_t MAX_FILE_SIZE = 1024 * 1024;

/** Maximum path length for temporary files */
static const size_t PATH_MAX = 1024;

/**
 * @struct CompilationContext
 * @brief Holds intermediate state during compilation.
 */
typedef struct {
    TokenStream *token_stream; /**< Pointer to token stream */
    ASTNode *ast_root; /**< Root of the AST */
    Architecture target_arch; /**< Target architecture */
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

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return ERR_FILE_SEEK;
    }
    const long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return ERR_FILE_TELL;
    }
    if ((size_t) size > MAX_FILE_SIZE) {
        fclose(f);
        return ERR_FILE_SIZE;
    }
    rewind(f);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return ERR_MEM_ALLOC;
    }

    const size_t read = fread(buf, 1, size, f);
    if (read != (size_t) size) {
        free(buf);
        fclose(f);
        return ERR_FILE_READ;
    }
    buf[size] = '\0';
    fclose(f);

    *out_buf = buf;
    *out_len = (size_t) size;
    return ERR_OK;
}

/**
 * @brief Execute a shell command, reporting failures.
 *
 * @param cmd  Null-terminated command string.
 * @return     system() return code.
 */
static int run_command(const char *cmd) {
    const int status = system(cmd);
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
        const Token t = lexer_next_token(&lex);
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
    const int errors = parse(&p);
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
 * @brief Collect all import paths from the AST.
 *
 * Recursively traverses the AST and collects import paths into a dynamically
 * allocated array of strings. The caller is responsible for freeing the array
 * and its contents.
 *
 * @param node     Current AST node to process.
 * @param imports  Pointer to array of import strings (to be filled).
 * @param count    Pointer to current count of imports.
 * @param cap      Pointer to current capacity of the imports array.
 */
static void collect_imports(const ASTNode *node, char ***imports, size_t *count, size_t *cap) {
    if (!node) return;
    if (node->type == NODE_IMPORT && node->child_count > 0) {
        const ASTNode *id = node->children[0];
        if (id && id->token.lexeme) {
            if (*count >= *cap) {
                *cap = *cap ? *cap * 2 : 8;
                *imports = realloc(*imports, *cap * sizeof(char *));
                assert(*imports);
            }
            (*imports)[(*count)++] = strdup(id->token.lexeme);
        }
    }
    for (size_t i = 0; i < node->child_count; ++i) {
        collect_imports(node->children[i], imports, count, cap);
    }
}

/**
 * @brief Top-level compilation function.
 *
 * Reads source from disk, lexes, parses, allocates registers,
 * emits assembly, and invokes the linker script.
 * The generated .s file is placed in the tmp directory, using the full
 * absolute path of the input file (with '/' replaced by '_', and without .bc extension).
 * The generated executable is named after the input file (without path or .bc).
 * If the .s file already exists, compilation is skipped.
 *
 * @param opts  CompilerOptions describing flags and file names.
 * @return      ERR_OK on success or an ErrorCode on failure.
 */
ErrorCode compile_file(const CompilerOptions *opts) {
    // Get absolute path of input file
    char abs_path[PATH_MAX];
    if (!realpath(opts->filename, abs_path)) {
        fprintf(stderr, "Failed to resolve absolute path for '%s'\n", opts->filename);
        return ERR_FILE_OPEN;
    }

    // Convert absolute path to a safe filename for tmp/
    char safe_path[PATH_MAX];
    snprintf(safe_path, sizeof(safe_path), "%s", abs_path);
    for (char *p = safe_path; *p; ++p) {
        if (*p == '/') *p = '_';
    }
    // Remove .bc extension if present (for .s file)
    size_t len = strlen(safe_path);
    if (len > 3 && strcmp(safe_path + len - 3, ".bc") == 0) {
        safe_path[len - 3] = '\0';
    }

    // Write .s file in tmp directory with full path-based name (no .bc)
    char asm_path[PATH_MAX + 50];
    snprintf(asm_path, sizeof(asm_path), "tmp/%s.s", safe_path);

    // Ensure tmp directory exists
    struct stat st = {0};
    if (stat("tmp", &st) == -1) {
        if (mkdir("tmp", 0700) != 0) {
            fprintf(stderr, "Failed to create tmp directory\n");
            return ERR_FILE_OPEN;
        }
    }

    // If .s file already exists, skip compilation
    struct stat asm_stat = {0};
    if (stat(asm_path, &asm_stat) == 0) {
        printf("Assembly file '%s' already exists, skipping compilation.\n", asm_path);
        return ERR_OK;
    }

    char *source = NULL;
    size_t src_len = 0;
    const ErrorCode er = read_file(opts->filename, &source, &src_len);
    if (er != ERR_OK) {
        fprintf(stderr, "Error reading '%s'\n", opts->filename);
        return er;
    }

    CompilationContext ctx = {0};
    TokenStream ts = {0};

    const int lex_errs = lex_phase(source, &ts);
    free(source);
    if (lex_errs > 0) {
        for (size_t i = 0; i < ts.count; i++) {
            const Token *t = &ts.tokens[i];
            if (t->type == TOKEN_ERROR) {
                fprintf(stderr, "Lexical error at line %d: %s\n", t->line, t->literal.error_message);
            }
        }
        fprintf(stderr, "Lexical errors: %d\n", lex_errs);
        cleanup_token_stream(&ts);
        return ERR_LEXICAL;
    }

    if (opts->show_tokens) {
        print_tokens(&ts);
    }

    ctx.token_stream = &ts;
    ctx.target_arch = opts->target_arch;

    if (parse_phase(&ctx, opts->show_ast) > 0) {
        fprintf(stderr, "Syntax errors detected.\n");
        cleanup_context(&ctx);
        return ERR_SYNTAX;
    }

    // --- Collect imports after parsing ---
    char **import_files = NULL;
    size_t import_count = 0, import_cap = 0;
    collect_imports(ctx.ast_root, &import_files, &import_count, &import_cap);

    /* Register allocation and backend codegen */
    register_allocate_ast(ctx.ast_root, opts->show_registers);

    FILE *asm_out = fopen(asm_path, "w");
    if (!asm_out) {
        cleanup_context(&ctx);
        for (size_t i = 0; i < import_count; ++i) free(import_files[i]);
        free(import_files);
        return ERR_FILE_OPEN;
    }

    /* Temporarily redirect stdout to assembly file */
    const int saved_stdout = dup(fileno(stdout));
    fflush(stdout);
    dup2(fileno(asm_out), fileno(stdout));
    codegen_arm(ctx.ast_root);
    fflush(stdout);
    dup2(saved_stdout, fileno(stdout));
    close(saved_stdout);
    fclose(asm_out);

    printf("Compilation succeeded for file : %s\n", opts->filename);

    // --- Recursively compile all imports ---
    for (size_t i = 0; i < import_count; ++i) {
        const char *import_file = import_files[i];
        size_t import_len = strlen(import_file);
        if (import_len > 2 && strcmp(import_file + import_len - 2, ".s") == 0) {
            // Copy .s file to tmp/ with safe name, avoiding double .s extension
            char import_abs[PATH_MAX];
            if (!realpath(import_file, import_abs)) {
                fprintf(stderr, "Failed to resolve absolute path for import '%s'\n", import_file);
                continue;
            }
            char import_safe[PATH_MAX];
            snprintf(import_safe, sizeof(import_safe), "%s", import_abs);
            for (char *p = import_safe; *p; ++p) {
                if (*p == '/') *p = '_';
            }
            // Remove .s extension if present
            size_t safe_len = strlen(import_safe);
            if (safe_len > 2 && strcmp(import_safe + safe_len - 2, ".s") == 0) {
                import_safe[safe_len - 2] = '\0';
            }
            char import_tmp[PATH_MAX + 50];
            snprintf(import_tmp, sizeof(import_tmp), "tmp/%s.s", import_safe);
            // Only copy if not already present
            struct stat st = {0};
            if (stat(import_tmp, &st) != 0) {
                char copy_cmd[PATH_MAX * 2 + 16];
                snprintf(copy_cmd, sizeof(copy_cmd), "cp '%s' '%s'", import_abs, import_tmp);
                run_command(copy_cmd);
            }
        } else {
            // Recursively compile non .s imports
            CompilerOptions import_opts = {0};
            import_opts.filename = import_files[i];
            import_opts.is_executable = false;
            compile_file(&import_opts);
        }
        free(import_files[i]);
    }
    free(import_files);

    // Get base filename (no path, no .bc)
    const char *base = strrchr(opts->filename, '/');
    base = base ? base + 1 : opts->filename;
    char exe_name[PATH_MAX];
    strncpy(exe_name, base, sizeof(exe_name));
    exe_name[sizeof(exe_name) - 1] = '\0';
    len = strlen(exe_name);
    if (len > 3 && strcmp(exe_name + len - 3, ".bc") == 0) {
        exe_name[len - 3] = '\0';
    }

    // Build command for generate_executable.sh
    if (opts->is_executable) {
        char cmd[PATH_MAX * 2 + 32];
        if (opts->save_asm) {
            snprintf(cmd, sizeof(cmd),
                     "./scripts/generate_executable.sh %s -s", exe_name);
        } else {
            snprintf(cmd, sizeof(cmd),
                     "./scripts/generate_executable.sh %s", exe_name);
        }
        run_command("chmod +x ./scripts/generate_executable.sh");
        run_command(cmd);

        printf("Executable generated for file : %s\n", opts->filename);
    }

    cleanup_context(&ctx);
    return ERR_OK;
}

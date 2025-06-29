#ifndef COMPILE_H
#define COMPILE_H

#include <stdbool.h>

/**
 * @enum ErrorCode
 * @brief Possible error codes returned by compile_file().
 */
typedef enum {
    ERR_OK = 0, /**< Compilation succeeded */
    ERR_FILE_OPEN, /**< Failed to open input or output file */
    ERR_FILE_SEEK, /**< fseek() failed */
    ERR_FILE_TELL, /**< ftell() failed */
    ERR_FILE_SIZE, /**< File exceeds maximum allowed size */
    ERR_MEM_ALLOC, /**< Memory allocation failed */
    ERR_FILE_READ, /**< fread() failed */
    ERR_LEXICAL, /**< Lexical errors encountered */
    ERR_SYNTAX, /**< Syntax errors encountered */
    ERR_UNKNOWN_OPTION,
    ERR_NO_INPUT_FILE,
    ERR_INVALID_ARCH
} ErrorCode;

/**
 * @enum Architecture
 * @brief Supported target architectures.
 */
typedef enum {
    ARCH_ARM /**< ARM code generation backend */
} Architecture;

/**
 * @struct CompilerOptions
 * @brief Command‑line options and settings for the compiler.
 */
typedef struct {
    bool show_tokens; /**< If true, dump token stream */
    bool show_ast; /**< If true, dump AST */
    bool show_registers; /**< If true, print register allocation details */
    bool save_asm; /**< If true, keep the .s file after linking */
    Architecture target_arch; /**< Target architecture (e.g. ARCH_ARM) */
    const char *filename; /**< Path to the input source file */
    char output_name[256]; /**< Base name for output (.s and executable) */
} CompilerOptions;

/**
 * @brief Perform full compilation on a single source file.
 *
 * This function will:
 *  - Read the file into memory
 *  - Run the lexer and parser
 *  - Perform register allocation
 *  - Generate target-specific assembly
 *  - Invoke the linker script to produce the final binary
 *
 * @param opts  Pointer to a CompilerOptions struct describing inputs and flags.
 * @return      ErrorCode (ERR_OK on success, non-zero on failure).
 */
ErrorCode compile_file(const CompilerOptions *opts);

#endif /* COMPILE_H */

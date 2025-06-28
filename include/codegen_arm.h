/**
* @file codegen_arm.h
 * @brief ARM code generation interface for BasicCodeCompiler
 */

#ifndef CODEGEN_ARM_H
#define CODEGEN_ARM_H

#include "parser.h"

/**
 * @brief Generate ARM assembly code from the given AST.
 * @param root Root node of the AST.
 */
void codegen_arm(const ASTNode *root);

#endif // CODEGEN_ARM_H

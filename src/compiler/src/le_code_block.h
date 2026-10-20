/**
 * @file le_code_block.h
 * @brief Expansion pass lowering high-level code block elements into primitive circuit schematics.
 *
 * This module implements a compiler frontend pass that synthesizes textual algorithm blocks
 * (`"CODE_BLOCK"` elements) into deterministic, cycle-accurate hardware element schematics
 * (arithmetic units, multiplexers, and state registers).
 *
 * ## Language Specification & Grammar (EBNF)
 * The embedded mini-language conforms to a deterministic, statically-typed subset of C99:
 *
 * ```ebnf
 * Program       ::= { FunctionDef | Stmt } ;
 * FunctionDef   ::= Type Identifier '(' [ ParamList ] ')' BlockStmt ;
 * ParamList     ::= Param { ',' Param } ;
 * Param         ::= Type Identifier ;
 *
 * Stmt          ::= VarDeclStmt | AssignStmt | IfStmt | ForStmt | ReturnStmt | ExprStmt | BlockStmt ;
 * VarDeclStmt   ::= [ 'static' ] Type Identifier [ '[' IntLiteral ']' ] [ '=' Expr ] ';' ;
 * AssignStmt    ::= LValue ( '=' | '+=' | '-=' | '*=' | '/=' ) Expr ';' ;
 * IfStmt        ::= 'if' '(' Expr ')' Stmt [ 'else' Stmt ] ;
 * ForStmt       ::= 'for' '(' [ Stmt ] ';' [ Expr ] ';' [ Stmt ] ')' Stmt ;
 * ReturnStmt    ::= 'return' [ Expr ] ';' ;
 * ExprStmt      ::= [ Expr ] ';' ;
 * BlockStmt     ::= '{' { Stmt } '}' ;
 *
 * Type          ::= 'bool' | 'int' | 'float' | 'complex' | 'void' ;
 * LValue        ::= Identifier [ '[' Expr ']' ] [ '.' Identifier ] ;
 *
 * Expr          ::= TernaryExpr ;
 * TernaryExpr   ::= LogicalOrExpr [ '?' Expr ':' TernaryExpr ] ;
 * LogicalOrExpr ::= LogicalAndExpr { '||' LogicalAndExpr } ;
 * LogicalAndExpr::= BitOrExpr { '&&' BitOrExpr } ;
 * BitOrExpr     ::= BitXorExpr { '|' BitXorExpr } ;
 * BitXorExpr    ::= BitAndExpr { '^' BitAndExpr } ;
 * BitAndExpr    ::= EqualityExpr { '&' EqualityExpr } ;
 * EqualityExpr  ::= RelationalExpr { ( '==' | '!=' ) RelationalExpr } ;
 * RelationalExpr::= AdditiveExpr { ( '<' | '<=' | '>' | '>=' ) AdditiveExpr } ;
 * AdditiveExpr  ::= MultiplicativeExpr { ( '+' | '-' ) MultiplicativeExpr } ;
 * Multiplicative::= UnaryExpr { ( '*' | '/' | '%' ) UnaryExpr } ;
 * UnaryExpr     ::= ( '+' | '-' | '!' | '~' | '++' | '--' ) UnaryExpr | PostfixExpr ;
 * PostfixExpr   ::= PrimaryExpr { '[' Expr ']' | '.' Identifier | '(' [ ArgList ] ')' | '++' | '--' } ;
 * PrimaryExpr   ::= Identifier | IntLiteral | FloatLiteral | 'true' | 'false' | '(' Expr ')' ;
 * ```
 *
 * ## Hardware Synthesis Semantics
 * - **Combinatorial Logic**: Pure expressions lower directly to hardware logic gates and math blocks.
 * - **Conditionals (`if` / `else` / `? :`)**: Lowered to conditional dataflow using `MUX` elements.
 * - **State (`static` variables)**: Synthesized into persistent hardware registers (`BOOLREGISTER`,
 *   `INTREGISTER`, `FLOATREGISTER`, `COMPLEXREGISTER`), retaining values across scan cycles.
 * - **Loops (`for`)**: Unrolled statically at compile-time to maintain strictly bounded scan execution.
 *
 * ## Thread Safety
 * All functions in this module are stateless and re-entrant. Separate circuit documents can be
 * expanded concurrently across multiple threads.
 */

#ifndef LE_CODE_BLOCK_H
#define LE_CODE_BLOCK_H

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "le_json.h"

namespace LogicElements {

/**
 * @brief Structured outcome container for code block expansion.
 */
struct CodeBlockResult {
    bool success = true;                  /**< True if all code blocks were successfully expanded; false on error. */
    std::string error_message;           /**< Descriptive diagnostic message if @ref success is false. */
    std::vector<std::string> warnings;   /**< Non-fatal compiler diagnostics (e.g. inferred latches or unroll warnings). */
};

/**
 * @brief Frontend transformation engine lowering code blocks into primitive circuit nets.
 */
class CodeBlockExpander {
public:
    /**
     * @brief Synthesizes all "CODE_BLOCK" elements in a circuit schematic into primitive hardware elements.
     *
     * Scans @p circuit_doc for elements with `"type": "CODE_BLOCK"`. For each found block,
     * parses its algorithm text, generates primitive logic gates, registers, and multiplexers,
     * appends them to the `"elements"` array, and synthesizes connecting `"nets"`.
     *
     * @param[in,out] circuit_doc In/Out JSON document containing `"elements"` and `"nets"`.
     * @param[out] out_error Receives error description if expansion fails.
     * @param[out] out_warnings Receives non-fatal compiler warnings.
     * @return `true` if all code blocks lowered successfully; `false` on semantic or syntax error.
     */
    static bool expand_code_blocks(
        JsonValue& circuit_doc,
        std::string& out_error,
        std::vector<std::string>& out_warnings
    );

    /**
     * @brief Overload returning structured @ref CodeBlockResult.
     *
     * @param[in,out] circuit_doc In/Out JSON document containing `"elements"` and `"nets"`.
     * @return @ref CodeBlockResult containing success status, error message, and warning list.
     */
    static CodeBlockResult expand(JsonValue& circuit_doc) {
        CodeBlockResult res;
        res.success = expand_code_blocks(circuit_doc, res.error_message, res.warnings);
        return res;
    }
};

} // namespace LogicElements

#endif /* LE_CODE_BLOCK_H */

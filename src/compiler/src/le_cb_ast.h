/**
 * @file le_cb_ast.h
 * @brief Abstract Syntax Tree (AST) node hierarchy and token definitions for the Code Block compiler.
 *
 * Defines the intermediate syntax representation produced by @ref Lexer and @ref Parser
 * during structured text synthesis into LogicElements netlists.
 */

#ifndef LE_CB_AST_H
#define LE_CB_AST_H

#include <memory>
#include <string>
#include <vector>

namespace LogicElements {

/**
 * @brief Lexical token classification categories.
 */
enum class TokenType {
    Eof,
    Identifier,
    IntLiteral,
    FloatLiteral,
    KwBool,
    KwInt,
    KwFloat,
    KwComplex,
    KwIf,
    KwElse,
    KwFor,
    KwReturn,
    KwStatic,
    KwTrue,
    KwFalse,
    KwVoid,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Equal,
    EqualEqual,
    Excl,
    ExclEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Amp,
    AmpAmp,
    Pipe,
    PipePipe,
    Caret,
    Tilde,
    PlusEqual,
    MinusEqual,
    StarEqual,
    SlashEqual,
    PlusPlus,
    MinusMinus,
    Question,
    Colon,
    Semicolon,
    Comma,
    Dot,
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket
};

/**
 * @brief Represents a single lexical token with source coordinates.
 */
struct Token {
    TokenType type{TokenType::Eof};
    std::string text;
    int int_val{0};
    double float_val{0.0};
    int line{1};
    int col{1};
};

/**
 * @brief Primary data types supported in the mini-C code block language.
 */
enum class TypeKind {
    Void,
    Bool,
    Int,
    Float,
    Complex
};

/**
 * @brief Fully qualified type specification including optional array sizing.
 */
struct VarType {
    TypeKind kind{TypeKind::Void};
    int array_size{0}; /**< 0 = scalar, >0 = fixed-length array dimension. */
};

/**
 * @brief Base class for all expression nodes in the AST.
 */
struct Expr {
    virtual ~Expr() = default;
};

/**
 * @brief Literal value expression (boolean, integer, float, or complex constant).
 */
struct LiteralExpr : public Expr {
    VarType type;
    bool b_val{false};
    int i_val{0};
    float f_val{0.0f};
    float c_real{0.0f};
    float c_imag{0.0f};
};

/**
 * @brief Variable reference or array index subscript expression.
 */
struct VarRefExpr : public Expr {
    std::string name;
    std::shared_ptr<Expr> index_expr;
    std::string member; /**< "real" or "imag" for complex component access. */
};

/**
 * @brief Unary operator expression (e.g. `!a`, `-b`, `~c`).
 */
struct UnaryExpr : public Expr {
    TokenType op{TokenType::Eof};
    std::shared_ptr<Expr> expr;
};

/**
 * @brief Binary operator expression (arithmetic, bitwise, comparison, or logical).
 */
struct BinaryExpr : public Expr {
    TokenType op{TokenType::Eof};
    std::shared_ptr<Expr> left;
    std::shared_ptr<Expr> right;
};

/**
 * @brief Conditional ternary expression (`cond ? then_expr : else_expr`).
 */
struct TernaryExpr : public Expr {
    std::shared_ptr<Expr> cond;
    std::shared_ptr<Expr> then_expr;
    std::shared_ptr<Expr> else_expr;
};

/**
 * @brief Function invocation expression with argument list.
 */
struct CallExpr : public Expr {
    std::string name;
    std::vector<std::shared_ptr<Expr>> args;
};

/**
 * @brief Base class for all statement nodes in the AST.
 */
struct Stmt {
    virtual ~Stmt() = default;
};

/**
 * @brief Expression statement (e.g. function call or standalone expression).
 */
struct ExprStmt : public Stmt {
    std::shared_ptr<Expr> expr;
};

/**
 * @brief Variable declaration statement with optional static persistence and initializer.
 */
struct VarDeclStmt : public Stmt {
    VarType type;
    std::string name;
    bool is_static{false};
    std::shared_ptr<Expr> init_expr;
};

/**
 * @brief Variable assignment statement (e.g. `a = b;`, `x += 2;`, `arr[i] = v;`).
 */
struct AssignStmt : public Stmt {
    std::string name;
    std::shared_ptr<Expr> index_expr;
    std::string member;
    TokenType op{TokenType::Equal}; /**< Equal, PlusEqual, MinusEqual, etc. */
    std::shared_ptr<Expr> rhs;
};

/**
 * @brief Scoped compound block statement (`{ stmt1; stmt2; }`).
 */
struct BlockStmt : public Stmt {
    std::vector<std::shared_ptr<Stmt>> stmts;
};

/**
 * @brief Conditional branching statement with optional else branch.
 */
struct IfStmt : public Stmt {
    std::shared_ptr<Expr> cond;
    std::shared_ptr<Stmt> then_branch;
    std::shared_ptr<Stmt> else_branch;
};

/**
 * @brief Bounded loop statement synthesizable via compile-time unrolling.
 */
struct ForStmt : public Stmt {
    std::shared_ptr<Stmt> init_stmt;
    std::shared_ptr<Expr> cond_expr;
    std::shared_ptr<Stmt> inc_stmt;
    std::shared_ptr<Stmt> body;
};

/**
 * @brief Return statement for function definitions.
 */
struct ReturnStmt : public Stmt {
    std::shared_ptr<Expr> expr;
};

/**
 * @brief Top-level function definition with parameters and body block.
 */
struct FunctionDef {
    VarType return_type;
    std::string name;
    std::vector<VarDeclStmt> params;
    std::shared_ptr<BlockStmt> body;
};

} // namespace LogicElements

#endif /* LE_CB_AST_H */

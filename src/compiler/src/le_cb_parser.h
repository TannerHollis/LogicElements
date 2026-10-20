/**
 * @file le_cb_parser.h
 * @brief Recursive-descent parser for the LogicElements Code Block frontend.
 *
 * Parses discrete token streams produced by @ref Lexer into structured Abstract
 * Syntax Trees (@ref FunctionDef, @ref Stmt, @ref Expr). Implements standard
 * C-style operator precedence and associativity rules.
 */

#ifndef LE_CB_PARSER_H
#define LE_CB_PARSER_H

#include "le_cb_ast.h"
#include "le_cb_lexer.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace LogicElements {

/**
 * @brief Recursive-descent parser translating Code Block text into AST representation.
 */
class Parser {
public:
    /**
     * @brief Constructs a parser instance bound to the provided source code string.
     *
     * @param[in] src Source code string to parse.
     */
    explicit Parser(const std::string& src) : m_lexer(src) {
        advance();
    }

    /**
     * @brief Parses the entire compilation unit into functions and top-level statements.
     *
     * @param[out] out_functions Receives user-defined function definitions.
     * @param[out] out_top_stmts Receives top-level executable statements.
     * @throws std::runtime_error On syntax errors or premature end-of-file.
     */
    void parse_program(std::vector<FunctionDef>& out_functions,
                       std::vector<std::shared_ptr<Stmt>>& out_top_stmts) {
        while (m_cur.type != TokenType::Eof) {
            if (is_type_token(m_cur.type) && peek_is_function()) {
                out_functions.push_back(parse_function());
            } else {
                out_top_stmts.push_back(parse_statement());
            }
        }
    }

private:
    void advance() {
        m_cur = m_lexer.next_token();
    }

    bool match(TokenType tt) {
        if (m_cur.type == tt) {
            advance();
            return true;
        }
        return false;
    }

    void expect(TokenType tt, const std::string& msg) {
        if (m_cur.type != tt) {
            throw std::runtime_error("Parser error at line " + std::to_string(m_cur.line) + ":" +
                                     std::to_string(m_cur.col) + ": expected " + msg + " but found '" + m_cur.text + "'");
        }
        advance();
    }

    static bool is_type_token(TokenType tt) {
        return tt == TokenType::KwBool || tt == TokenType::KwInt || tt == TokenType::KwFloat ||
               tt == TokenType::KwComplex || tt == TokenType::KwVoid;
    }

    bool peek_is_function() {
        Lexer lookahead = m_lexer;
        Token t1 = lookahead.next_token();
        if (t1.type != TokenType::Identifier) return false;
        Token t2 = lookahead.next_token();
        return t2.type == TokenType::LParen;
    }

    VarType parse_type() {
        VarType vt;
        if (m_cur.type == TokenType::KwBool) vt.kind = TypeKind::Bool;
        else if (m_cur.type == TokenType::KwInt) vt.kind = TypeKind::Int;
        else if (m_cur.type == TokenType::KwFloat) vt.kind = TypeKind::Float;
        else if (m_cur.type == TokenType::KwComplex) vt.kind = TypeKind::Complex;
        else if (m_cur.type == TokenType::KwVoid) vt.kind = TypeKind::Void;
        else throw std::runtime_error("Expected type name at line " + std::to_string(m_cur.line));
        advance();
        if (match(TokenType::LBracket)) {
            if (m_cur.type != TokenType::IntLiteral) {
                throw std::runtime_error("Array dimension must be integer constant at line " + std::to_string(m_cur.line));
            }
            vt.array_size = m_cur.int_val;
            advance();
            expect(TokenType::RBracket, "']'");
        }
        return vt;
    }

    FunctionDef parse_function() {
        FunctionDef fn;
        fn.return_type = parse_type();
        if (m_cur.type != TokenType::Identifier) {
            throw std::runtime_error("Expected function name at line " + std::to_string(m_cur.line));
        }
        fn.name = m_cur.text;
        advance();
        expect(TokenType::LParen, "'('");
        if (m_cur.type != TokenType::RParen) {
            while (true) {
                VarDeclStmt p;
                p.type = parse_type();
                if (m_cur.type != TokenType::Identifier) {
                    throw std::runtime_error("Expected parameter name at line " + std::to_string(m_cur.line));
                }
                p.name = m_cur.text;
                advance();
                fn.params.push_back(p);
                if (match(TokenType::Comma)) continue;
                break;
            }
        }
        expect(TokenType::RParen, "')'");
        fn.body = parse_block();
        return fn;
    }

    std::shared_ptr<BlockStmt> parse_block() {
        expect(TokenType::LBrace, "'{'");
        auto blk = std::make_shared<BlockStmt>();
        while (m_cur.type != TokenType::RBrace && m_cur.type != TokenType::Eof) {
            blk->stmts.push_back(parse_statement());
        }
        expect(TokenType::RBrace, "'}'");
        return blk;
    }

    std::shared_ptr<Stmt> parse_statement() {
        if (m_cur.type == TokenType::LBrace) {
            return parse_block();
        }
        if (m_cur.type == TokenType::KwIf) {
            advance();
            expect(TokenType::LParen, "'('");
            auto cond = parse_expression();
            expect(TokenType::RParen, "')'");
            auto then_b = parse_statement();
            std::shared_ptr<Stmt> else_b = nullptr;
            if (match(TokenType::KwElse)) {
                else_b = parse_statement();
            }
            auto if_s = std::make_shared<IfStmt>();
            if_s->cond = cond;
            if_s->then_branch = then_b;
            if_s->else_branch = else_b;
            return if_s;
        }
        if (m_cur.type == TokenType::KwFor) {
            advance();
            expect(TokenType::LParen, "'('");
            std::shared_ptr<Stmt> init_s = nullptr;
            if (m_cur.type != TokenType::Semicolon) {
                init_s = parse_statement_no_semi();
            }
            expect(TokenType::Semicolon, "';'");
            std::shared_ptr<Expr> cond_e = nullptr;
            if (m_cur.type != TokenType::Semicolon) {
                cond_e = parse_expression();
            }
            expect(TokenType::Semicolon, "';'");
            std::shared_ptr<Stmt> inc_s = nullptr;
            if (m_cur.type != TokenType::RParen) {
                inc_s = parse_statement_no_semi();
            }
            expect(TokenType::RParen, "')'");
            auto body = parse_statement();
            auto for_s = std::make_shared<ForStmt>();
            for_s->init_stmt = init_s;
            for_s->cond_expr = cond_e;
            for_s->inc_stmt = inc_s;
            for_s->body = body;
            return for_s;
        }
        if (m_cur.type == TokenType::KwReturn) {
            advance();
            auto ret = std::make_shared<ReturnStmt>();
            if (m_cur.type != TokenType::Semicolon) {
                ret->expr = parse_expression();
            }
            expect(TokenType::Semicolon, "';'");
            return ret;
        }

        bool is_static = match(TokenType::KwStatic);
        if (is_static || is_type_token(m_cur.type)) {
            auto decl = std::make_shared<VarDeclStmt>();
            decl->is_static = is_static;
            decl->type = parse_type();
            if (m_cur.type != TokenType::Identifier) {
                throw std::runtime_error("Expected variable name at line " + std::to_string(m_cur.line));
            }
            decl->name = m_cur.text;
            advance();
            if (match(TokenType::LBracket)) {
                if (m_cur.type != TokenType::IntLiteral) {
                    throw std::runtime_error("Array dimension must be integer constant at line " + std::to_string(m_cur.line));
                }
                decl->type.array_size = m_cur.int_val;
                advance();
                expect(TokenType::RBracket, "']'");
            }
            if (match(TokenType::Equal)) {
                decl->init_expr = parse_expression();
            }
            expect(TokenType::Semicolon, "';'");
            return decl;
        }

        auto s = parse_statement_no_semi();
        expect(TokenType::Semicolon, "';'");
        return s;
    }

    std::shared_ptr<Stmt> parse_statement_no_semi() {
        if (is_type_token(m_cur.type)) {
            auto decl = std::make_shared<VarDeclStmt>();
            decl->type = parse_type();
            if (m_cur.type != TokenType::Identifier) {
                throw std::runtime_error("Expected variable name at line " + std::to_string(m_cur.line));
            }
            decl->name = m_cur.text;
            advance();
            if (match(TokenType::LBracket)) {
                if (m_cur.type != TokenType::IntLiteral) {
                    throw std::runtime_error("Array dimension must be integer constant at line " + std::to_string(m_cur.line));
                }
                decl->type.array_size = m_cur.int_val;
                advance();
                expect(TokenType::RBracket, "']'");
            }
            if (match(TokenType::Equal)) {
                decl->init_expr = parse_expression();
            }
            return decl;
        }

        if (m_cur.type == TokenType::Identifier) {
            std::string name = m_cur.text;
            advance();
            std::shared_ptr<Expr> idx_expr = nullptr;
            std::string member;
            if (match(TokenType::LBracket)) {
                idx_expr = parse_expression();
                expect(TokenType::RBracket, "']'");
            }
            if (match(TokenType::Dot)) {
                if (m_cur.type != TokenType::Identifier) {
                    throw std::runtime_error("Expected member name after '.' at line " + std::to_string(m_cur.line));
                }
                member = m_cur.text;
                advance();
            }

            if (match(TokenType::PlusPlus)) {
                auto as = std::make_shared<AssignStmt>();
                as->name = name;
                as->index_expr = idx_expr;
                as->member = member;
                as->op = TokenType::PlusEqual;
                auto one = std::make_shared<LiteralExpr>();
                one->type = {TypeKind::Int, 0};
                one->i_val = 1;
                as->rhs = one;
                return as;
            }
            if (match(TokenType::MinusMinus)) {
                auto as = std::make_shared<AssignStmt>();
                as->name = name;
                as->index_expr = idx_expr;
                as->member = member;
                as->op = TokenType::MinusEqual;
                auto one = std::make_shared<LiteralExpr>();
                one->type = {TypeKind::Int, 0};
                one->i_val = 1;
                as->rhs = one;
                return as;
            }

            if (m_cur.type == TokenType::Equal || m_cur.type == TokenType::PlusEqual ||
                m_cur.type == TokenType::MinusEqual || m_cur.type == TokenType::StarEqual ||
                m_cur.type == TokenType::SlashEqual) {
                TokenType op = m_cur.type;
                advance();
                auto as = std::make_shared<AssignStmt>();
                as->name = name;
                as->index_expr = idx_expr;
                as->member = member;
                as->op = op;
                as->rhs = parse_expression();
                return as;
            }

            auto vref = std::make_shared<VarRefExpr>();
            vref->name = name;
            vref->index_expr = idx_expr;
            vref->member = member;
            if (match(TokenType::LParen)) {
                auto call = std::make_shared<CallExpr>();
                call->name = name;
                if (m_cur.type != TokenType::RParen) {
                    while (true) {
                        call->args.push_back(parse_expression());
                        if (match(TokenType::Comma)) continue;
                        break;
                    }
                }
                expect(TokenType::RParen, "')'");
                auto es = std::make_shared<ExprStmt>();
                es->expr = call;
                return es;
            }
        }

        auto es = std::make_shared<ExprStmt>();
        es->expr = parse_expression();
        return es;
    }

    std::shared_ptr<Expr> parse_expression() {
        return parse_ternary();
    }

    std::shared_ptr<Expr> parse_ternary() {
        auto cond = parse_logical_or();
        if (match(TokenType::Question)) {
            auto then_e = parse_expression();
            expect(TokenType::Colon, "':'");
            auto else_e = parse_ternary();
            auto t = std::make_shared<TernaryExpr>();
            t->cond = cond;
            t->then_expr = then_e;
            t->else_expr = else_e;
            return t;
        }
        return cond;
    }

    std::shared_ptr<Expr> parse_logical_or() {
        auto left = parse_logical_and();
        while (match(TokenType::PipePipe)) {
            auto bin = std::make_shared<BinaryExpr>();
            bin->op = TokenType::PipePipe;
            bin->left = left;
            bin->right = parse_logical_and();
            left = bin;
        }
        return left;
    }

    std::shared_ptr<Expr> parse_logical_and() {
        auto left = parse_bitwise_or();
        while (match(TokenType::AmpAmp)) {
            auto bin = std::make_shared<BinaryExpr>();
            bin->op = TokenType::AmpAmp;
            bin->left = left;
            bin->right = parse_bitwise_or();
            left = bin;
        }
        return left;
    }

    std::shared_ptr<Expr> parse_bitwise_or() {
        auto left = parse_bitwise_xor();
        while (match(TokenType::Pipe)) {
            auto bin = std::make_shared<BinaryExpr>();
            bin->op = TokenType::Pipe;
            bin->left = left;
            bin->right = parse_bitwise_xor();
            left = bin;
        }
        return left;
    }

    std::shared_ptr<Expr> parse_bitwise_xor() {
        auto left = parse_bitwise_and();
        while (match(TokenType::Caret)) {
            auto bin = std::make_shared<BinaryExpr>();
            bin->op = TokenType::Caret;
            bin->left = left;
            bin->right = parse_bitwise_and();
            left = bin;
        }
        return left;
    }

    std::shared_ptr<Expr> parse_bitwise_and() {
        auto left = parse_equality();
        while (match(TokenType::Amp)) {
            auto bin = std::make_shared<BinaryExpr>();
            bin->op = TokenType::Amp;
            bin->left = left;
            bin->right = parse_equality();
            left = bin;
        }
        return left;
    }

    std::shared_ptr<Expr> parse_equality() {
        auto left = parse_relational();
        while (m_cur.type == TokenType::EqualEqual || m_cur.type == TokenType::ExclEqual) {
            TokenType op = m_cur.type;
            advance();
            auto bin = std::make_shared<BinaryExpr>();
            bin->op = op;
            bin->left = left;
            bin->right = parse_relational();
            left = bin;
        }
        return left;
    }

    std::shared_ptr<Expr> parse_relational() {
        auto left = parse_additive();
        while (m_cur.type == TokenType::Less || m_cur.type == TokenType::LessEqual ||
               m_cur.type == TokenType::Greater || m_cur.type == TokenType::GreaterEqual) {
            TokenType op = m_cur.type;
            advance();
            auto bin = std::make_shared<BinaryExpr>();
            bin->op = op;
            bin->left = left;
            bin->right = parse_additive();
            left = bin;
        }
        return left;
    }

    std::shared_ptr<Expr> parse_additive() {
        auto left = parse_multiplicative();
        while (m_cur.type == TokenType::Plus || m_cur.type == TokenType::Minus) {
            TokenType op = m_cur.type;
            advance();
            auto bin = std::make_shared<BinaryExpr>();
            bin->op = op;
            bin->left = left;
            bin->right = parse_multiplicative();
            left = bin;
        }
        return left;
    }

    std::shared_ptr<Expr> parse_multiplicative() {
        auto left = parse_unary();
        while (m_cur.type == TokenType::Star || m_cur.type == TokenType::Slash || m_cur.type == TokenType::Percent) {
            TokenType op = m_cur.type;
            advance();
            auto bin = std::make_shared<BinaryExpr>();
            bin->op = op;
            bin->left = left;
            bin->right = parse_unary();
            left = bin;
        }
        return left;
    }

    std::shared_ptr<Expr> parse_unary() {
        if (m_cur.type == TokenType::Minus || m_cur.type == TokenType::Excl || m_cur.type == TokenType::Tilde) {
            TokenType op = m_cur.type;
            advance();
            auto u = std::make_shared<UnaryExpr>();
            u->op = op;
            u->expr = parse_unary();
            return u;
        }
        return parse_postfix();
    }

    std::shared_ptr<Expr> parse_postfix() {
        auto expr = parse_primary();
        while (true) {
            if (match(TokenType::LBracket)) {
                auto idx = parse_expression();
                expect(TokenType::RBracket, "']'");
                if (auto vr = std::dynamic_pointer_cast<VarRefExpr>(expr)) {
                    vr->index_expr = idx;
                } else {
                    throw std::runtime_error("Array subscript only supported on variables");
                }
            } else if (match(TokenType::Dot)) {
                if (m_cur.type != TokenType::Identifier) {
                    throw std::runtime_error("Expected member name after '.'");
                }
                std::string mem = m_cur.text;
                advance();
                if (auto vr = std::dynamic_pointer_cast<VarRefExpr>(expr)) {
                    vr->member = mem;
                } else {
                    throw std::runtime_error("Member access only supported on variables");
                }
            } else {
                break;
            }
        }
        return expr;
    }

    std::shared_ptr<Expr> parse_primary() {
        if (m_cur.type == TokenType::IntLiteral) {
            auto lit = std::make_shared<LiteralExpr>();
            lit->type = {TypeKind::Int, 0};
            lit->i_val = m_cur.int_val;
            advance();
            return lit;
        }
        if (m_cur.type == TokenType::FloatLiteral) {
            auto lit = std::make_shared<LiteralExpr>();
            lit->type = {TypeKind::Float, 0};
            lit->f_val = static_cast<float>(m_cur.float_val);
            advance();
            return lit;
        }
        if (m_cur.type == TokenType::KwTrue || m_cur.type == TokenType::KwFalse) {
            auto lit = std::make_shared<LiteralExpr>();
            lit->type = {TypeKind::Bool, 0};
            lit->b_val = (m_cur.type == TokenType::KwTrue);
            advance();
            return lit;
        }
        if (m_cur.type == TokenType::Identifier) {
            std::string name = m_cur.text;
            advance();
            if (match(TokenType::LParen)) {
                auto call = std::make_shared<CallExpr>();
                call->name = name;
                if (m_cur.type != TokenType::RParen) {
                    while (true) {
                        call->args.push_back(parse_expression());
                        if (match(TokenType::Comma)) continue;
                        break;
                    }
                }
                expect(TokenType::RParen, "')'");
                return call;
            }
            auto vr = std::make_shared<VarRefExpr>();
            vr->name = name;
            return vr;
        }
        if (match(TokenType::LParen)) {
            auto e = parse_expression();
            expect(TokenType::RParen, "')'");
            return e;
        }

        throw std::runtime_error("Unexpected token in expression: '" + m_cur.text + "' at line " +
                                 std::to_string(m_cur.line));
    }

    Lexer m_lexer;
    Token m_cur;
};

} // namespace LogicElements

#endif /* LE_CB_PARSER_H */

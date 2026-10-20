/**
 * @file le_cb_lexer.h
 * @brief Lexical analyzer and tokenizer for the LogicElements Code Block frontend.
 *
 * Scans C-like source code into a stream of discrete lexical tokens (@ref Token).
 * Supports whitespace stripping, line comments, block comments,
 * identifiers, keyword mapping, integer literals, and locale-independent float parsing.
 */

#ifndef LE_CB_LEXER_H
#define LE_CB_LEXER_H

#include "le_cb_ast.h"

#include <cctype>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace LogicElements {

/**
 * @brief Tokenizes structured mini-C code block text into AST tokens.
 */
class Lexer {
public:
    /**
     * @brief Constructs a lexer instance bound to the provided source code string.
     *
     * @param[in] src Source code string to scan.
     */
    explicit Lexer(const std::string& src)
        : m_src(src), m_pos(0), m_line(1), m_col(1) {}

    /**
     * @brief Scans and returns the next lexical token from the input stream.
     *
     * @return Next scanned @ref Token. Returns @ref TokenType::Eof upon reaching end of input.
     * @throws std::runtime_error On illegal characters or malformed tokens.
     */
    Token next_token() {
        skip_whitespace_and_comments();
        if (m_pos >= m_src.size()) {
            return {TokenType::Eof, "", 0, 0.0, m_line, m_col};
        }

        int start_line = m_line;
        int start_col = m_col;
        char c = m_src[m_pos];

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = m_pos;
            while (m_pos < m_src.size() &&
                   (std::isalnum(static_cast<unsigned char>(m_src[m_pos])) || m_src[m_pos] == '_')) {
                advance();
            }
            std::string word = m_src.substr(start, m_pos - start);
            TokenType tt = TokenType::Identifier;
            if (word == "bool") tt = TokenType::KwBool;
            else if (word == "int") tt = TokenType::KwInt;
            else if (word == "float") tt = TokenType::KwFloat;
            else if (word == "complex") tt = TokenType::KwComplex;
            else if (word == "if") tt = TokenType::KwIf;
            else if (word == "else") tt = TokenType::KwElse;
            else if (word == "for") tt = TokenType::KwFor;
            else if (word == "return") tt = TokenType::KwReturn;
            else if (word == "static") tt = TokenType::KwStatic;
            else if (word == "true") tt = TokenType::KwTrue;
            else if (word == "false") tt = TokenType::KwFalse;
            else if (word == "void") tt = TokenType::KwVoid;
            return {tt, word, 0, 0.0, start_line, start_col};
        }

        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && m_pos + 1 < m_src.size() &&
             std::isdigit(static_cast<unsigned char>(m_src[m_pos + 1])))) {
            size_t start = m_pos;
            bool is_float = false;
            while (m_pos < m_src.size() &&
                   (std::isdigit(static_cast<unsigned char>(m_src[m_pos])) ||
                    m_src[m_pos] == '.' || m_src[m_pos] == 'f' || m_src[m_pos] == 'F' ||
                    m_src[m_pos] == 'e' || m_src[m_pos] == 'E' ||
                    ((m_src[m_pos] == '+' || m_src[m_pos] == '-') && m_pos > start &&
                     (m_src[m_pos - 1] == 'e' || m_src[m_pos - 1] == 'E')))) {
                if (m_src[m_pos] == '.' || m_src[m_pos] == 'f' || m_src[m_pos] == 'F' ||
                    m_src[m_pos] == 'e' || m_src[m_pos] == 'E') {
                    is_float = true;
                }
                advance();
            }
            std::string num_str = m_src.substr(start, m_pos - start);
            if (!num_str.empty() && (num_str.back() == 'f' || num_str.back() == 'F')) {
                num_str.pop_back();
                is_float = true;
            }
            if (is_float) {
                double f = parse_locale_independent_double(num_str);
                return {TokenType::FloatLiteral, num_str, 0, f, start_line, start_col};
            } else {
                int i = 0;
                try {
                    i = std::stoi(num_str);
                } catch (...) {
                    i = static_cast<int>(parse_locale_independent_double(num_str));
                }
                return {TokenType::IntLiteral, num_str, i, static_cast<double>(i), start_line, start_col};
            }
        }

        advance();
        switch (c) {
            case '+':
                if (match('+')) return {TokenType::PlusPlus, "++", 0, 0.0, start_line, start_col};
                if (match('=')) return {TokenType::PlusEqual, "+=", 0, 0.0, start_line, start_col};
                return {TokenType::Plus, "+", 0, 0.0, start_line, start_col};
            case '-':
                if (match('-')) return {TokenType::MinusMinus, "--", 0, 0.0, start_line, start_col};
                if (match('=')) return {TokenType::MinusEqual, "-=", 0, 0.0, start_line, start_col};
                return {TokenType::Minus, "-", 0, 0.0, start_line, start_col};
            case '*':
                if (match('=')) return {TokenType::StarEqual, "*=", 0, 0.0, start_line, start_col};
                return {TokenType::Star, "*", 0, 0.0, start_line, start_col};
            case '/':
                if (match('=')) return {TokenType::SlashEqual, "/=", 0, 0.0, start_line, start_col};
                return {TokenType::Slash, "/", 0, 0.0, start_line, start_col};
            case '%': return {TokenType::Percent, "%", 0, 0.0, start_line, start_col};
            case '=':
                if (match('=')) return {TokenType::EqualEqual, "==", 0, 0.0, start_line, start_col};
                return {TokenType::Equal, "=", 0, 0.0, start_line, start_col};
            case '!':
                if (match('=')) return {TokenType::ExclEqual, "!=", 0, 0.0, start_line, start_col};
                return {TokenType::Excl, "!", 0, 0.0, start_line, start_col};
            case '<':
                if (match('=')) return {TokenType::LessEqual, "<=", 0, 0.0, start_line, start_col};
                return {TokenType::Less, "<", 0, 0.0, start_line, start_col};
            case '>':
                if (match('=')) return {TokenType::GreaterEqual, ">=", 0, 0.0, start_line, start_col};
                return {TokenType::Greater, ">", 0, 0.0, start_line, start_col};
            case '&':
                if (match('&')) return {TokenType::AmpAmp, "&&", 0, 0.0, start_line, start_col};
                return {TokenType::Amp, "&", 0, 0.0, start_line, start_col};
            case '|':
                if (match('|')) return {TokenType::PipePipe, "||", 0, 0.0, start_line, start_col};
                return {TokenType::Pipe, "|", 0, 0.0, start_line, start_col};
            case '^': return {TokenType::Caret, "^", 0, 0.0, start_line, start_col};
            case '~': return {TokenType::Tilde, "~", 0, 0.0, start_line, start_col};
            case '?': return {TokenType::Question, "?", 0, 0.0, start_line, start_col};
            case ':': return {TokenType::Colon, ":", 0, 0.0, start_line, start_col};
            case ';': return {TokenType::Semicolon, ";", 0, 0.0, start_line, start_col};
            case ',': return {TokenType::Comma, ",", 0, 0.0, start_line, start_col};
            case '.': return {TokenType::Dot, ".", 0, 0.0, start_line, start_col};
            case '(': return {TokenType::LParen, "(", 0, 0.0, start_line, start_col};
            case ')': return {TokenType::RParen, ")", 0, 0.0, start_line, start_col};
            case '{': return {TokenType::LBrace, "{", 0, 0.0, start_line, start_col};
            case '}': return {TokenType::RBrace, "}", 0, 0.0, start_line, start_col};
            case '[': return {TokenType::LBracket, "[", 0, 0.0, start_line, start_col};
            case ']': return {TokenType::RBracket, "]", 0, 0.0, start_line, start_col};
            default:
                throw std::runtime_error("Unexpected character '" + std::string(1, c) + "' at line " +
                                         std::to_string(start_line) + ":" + std::to_string(start_col));
        }
    }

private:
    static double parse_locale_independent_double(const std::string& str) {
        if (str.empty()) return 0.0;
        size_t idx = 0;
        bool negative = false;
        if (str[idx] == '-') { negative = true; idx++; }
        else if (str[idx] == '+') { idx++; }

        double integer_part = 0.0;
        while (idx < str.size() && std::isdigit(static_cast<unsigned char>(str[idx]))) {
            integer_part = integer_part * 10.0 + (str[idx] - '0');
            idx++;
        }

        double fractional_part = 0.0;
        if (idx < str.size() && str[idx] == '.') {
            idx++;
            double divisor = 10.0;
            while (idx < str.size() && std::isdigit(static_cast<unsigned char>(str[idx]))) {
                fractional_part += (str[idx] - '0') / divisor;
                divisor *= 10.0;
                idx++;
            }
        }

        double result = integer_part + fractional_part;
        if (idx < str.size() && (str[idx] == 'e' || str[idx] == 'E')) {
            idx++;
            bool exp_neg = false;
            if (idx < str.size() && str[idx] == '-') { exp_neg = true; idx++; }
            else if (idx < str.size() && str[idx] == '+') { idx++; }
            int exp_val = 0;
            while (idx < str.size() && std::isdigit(static_cast<unsigned char>(str[idx]))) {
                exp_val = exp_val * 10 + (str[idx] - '0');
                idx++;
            }
            double factor = std::pow(10.0, exp_neg ? -exp_val : exp_val);
            result *= factor;
        }

        return negative ? -result : result;
    }

    char advance() {
        if (m_pos >= m_src.size()) return '\0';
        char c = m_src[m_pos++];
        if (c == '\n') {
            m_line++;
            m_col = 1;
        } else {
            m_col++;
        }
        return c;
    }

    bool match(char expected) {
        if (m_pos >= m_src.size() || m_src[m_pos] != expected) return false;
        advance();
        return true;
    }

    void skip_whitespace_and_comments() {
        while (m_pos < m_src.size()) {
            char c = m_src[m_pos];
            if (std::isspace(static_cast<unsigned char>(c))) {
                advance();
            } else if (c == '/' && m_pos + 1 < m_src.size() && m_src[m_pos + 1] == '/') {
                advance(); advance();
                while (m_pos < m_src.size() && m_src[m_pos] != '\n') advance();
            } else if (c == '/' && m_pos + 1 < m_src.size() && m_src[m_pos + 1] == '*') {
                advance(); advance();
                while (m_pos + 1 < m_src.size() && !(m_src[m_pos] == '*' && m_src[m_pos + 1] == '/')) advance();
                if (m_pos + 1 < m_src.size()) { advance(); advance(); }
            } else {
                break;
            }
        }
    }

    std::string m_src;
    size_t m_pos{0};
    int m_line{1};
    int m_col{1};
};

} // namespace LogicElements

#endif /* LE_CB_LEXER_H */

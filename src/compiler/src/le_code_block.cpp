#include "le_code_block.h"
#include "le_cb_ast.h"
#include "le_cb_lexer.h"
#include "le_cb_parser.h"
#include "le_math.h"
#include <iostream>
#include <sstream>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <set>

namespace LogicElements {

// -----------------------------------------------------------------------------
// Lowering & Expansion Engine
// -----------------------------------------------------------------------------
struct WireRef {
    TypeKind kind{TypeKind::Float};
    bool is_const{false};
    bool b_val{false};
    int i_val{0};
    float f_val{0.0f};
    float c_real{0.0f};
    float c_imag{0.0f};
    std::string node_name;
    std::string port_name{"out"};

    bool operator==(const WireRef& o) const {
        if (kind != o.kind || is_const != o.is_const) return false;
        if (is_const) {
            if (kind == TypeKind::Bool) return b_val == o.b_val;
            if (kind == TypeKind::Int) return i_val == o.i_val;
            if (kind == TypeKind::Complex) return c_real == o.c_real && c_imag == o.c_imag;
            return std::abs(f_val - o.f_val) < 1e-6f;
        }
        return node_name == o.node_name && port_name == o.port_name;
    }
    bool operator!=(const WireRef& o) const { return !(*this == o); }
};

class LoweringContext {
public:
    LoweringContext(const std::string& block_name,
                    std::vector<JsonValue>& elements,
                    std::vector<JsonValue>& nets,
                    std::vector<std::string>& warnings)
        : m_block_name(block_name), m_elements(elements), m_nets(nets), m_warnings(warnings) {}

    std::set<std::string> call_stack;

    std::string new_element_name(const std::string& prefix) {
        return "_cb_" + m_block_name + "_" + prefix + "_" + std::to_string(m_counter++);
    }

    WireRef make_const_bool(bool b) {
        WireRef w;
        w.kind = TypeKind::Bool;
        w.is_const = true;
        w.b_val = b;
        return w;
    }

    WireRef make_const_int(int i) {
        WireRef w;
        w.kind = TypeKind::Int;
        w.is_const = true;
        w.i_val = i;
        return w;
    }

    WireRef make_const_float(float f) {
        WireRef w;
        w.kind = TypeKind::Float;
        w.is_const = true;
        w.f_val = f;
        return w;
    }

    WireRef make_const_complex(float r, float i) {
        WireRef w;
        w.kind = TypeKind::Complex;
        w.is_const = true;
        w.c_real = r;
        w.c_imag = i;
        return w;
    }

    void add_element(const JsonValue& el) {
        m_elements.push_back(el);
    }

    void add_net(const WireRef& src, const std::string& dst_node, const std::string& dst_port) {
        if (src.is_const) {
            // Instantiate a constant node if not already existing
            std::string cname = new_element_name("const");
            JsonValue c_el;
            c_el.set("name", cname);
            c_el.set("type", "CONSTANT");
            if (src.kind == TypeKind::Bool) {
                c_el.set("dataType", "bool");
                c_el.set("value", src.b_val);
            } else if (src.kind == TypeKind::Int) {
                c_el.set("dataType", "int");
                c_el.set("value", (double)src.i_val);
            } else if (src.kind == TypeKind::Float) {
                c_el.set("dataType", "float");
                c_el.set("value", (double)src.f_val);
            } else if (src.kind == TypeKind::Complex) {
                // Complex constant: use RECT2COMPLEX
                std::string rname = new_element_name("c_r");
                JsonValue rel;
                rel.set("name", rname);
                rel.set("type", "CONSTANT");
                rel.set("dataType", "float");
                rel.set("value", (double)src.c_real);
                add_element(rel);

                std::string iname = new_element_name("c_i");
                JsonValue iel;
                iel.set("name", iname);
                iel.set("type", "CONSTANT");
                iel.set("dataType", "float");
                iel.set("value", (double)src.c_imag);
                add_element(iel);

                std::string rect_name = new_element_name("r2c");
                JsonValue rect_el;
                rect_el.set("name", rect_name);
                rect_el.set("type", "RECT2COMPLEX");
                add_element(rect_el);

                add_net_raw(rname, "out", rect_name, "real");
                add_net_raw(iname, "out", rect_name, "imag");
                add_net_raw(rect_name, "out", dst_node, dst_port);
                return;
            }
            add_element(c_el);
            add_net_raw(cname, "out", dst_node, dst_port);
        } else {
            add_net_raw(src.node_name, src.port_name, dst_node, dst_port);
        }
    }

    void add_net_raw(const std::string& src_node, const std::string& src_port,
                     const std::string& dst_node, const std::string& dst_port) {
        JsonValue net;
        JsonValue out_obj;
        out_obj.set("name", src_node);
        out_obj.set("port", src_port);
        net.set("output", out_obj);

        JsonValue inp_obj;
        inp_obj.set("name", dst_node);
        inp_obj.set("port", dst_port);
        JsonValue inps_arr;
        inps_arr.push_back(inp_obj);
        net.set("inputs", inps_arr);

        m_nets.push_back(net);
    }

    WireRef synthesize_binary_op(TokenType op, const WireRef& a, const WireRef& b) {
        // Constant folding
        if (a.is_const && b.is_const) {
            if (a.kind == TypeKind::Int && b.kind == TypeKind::Int) {
                switch (op) {
                    case TokenType::Plus: return make_const_int(a.i_val + b.i_val);
                    case TokenType::Minus: return make_const_int(a.i_val - b.i_val);
                    case TokenType::Star: return make_const_int(a.i_val * b.i_val);
                    case TokenType::Slash: return make_const_int(b.i_val != 0 ? a.i_val / b.i_val : 0);
                    case TokenType::Percent: return make_const_int(b.i_val != 0 ? a.i_val % b.i_val : 0);
                    case TokenType::Amp: return make_const_int(a.i_val & b.i_val);
                    case TokenType::Pipe: return make_const_int(a.i_val | b.i_val);
                    case TokenType::Caret: return make_const_int(a.i_val ^ b.i_val);
                    case TokenType::EqualEqual: return make_const_bool(a.i_val == b.i_val);
                    case TokenType::ExclEqual: return make_const_bool(a.i_val != b.i_val);
                    case TokenType::Less: return make_const_bool(a.i_val < b.i_val);
                    case TokenType::LessEqual: return make_const_bool(a.i_val <= b.i_val);
                    case TokenType::Greater: return make_const_bool(a.i_val > b.i_val);
                    case TokenType::GreaterEqual: return make_const_bool(a.i_val >= b.i_val);
                    default: break;
                }
            } else if ((a.kind == TypeKind::Float || a.kind == TypeKind::Int) &&
                       (b.kind == TypeKind::Float || b.kind == TypeKind::Int)) {
                float fa = (a.kind == TypeKind::Float) ? a.f_val : (float)a.i_val;
                float fb = (b.kind == TypeKind::Float) ? b.f_val : (float)b.i_val;
                switch (op) {
                    case TokenType::Plus: return make_const_float(fa + fb);
                    case TokenType::Minus: return make_const_float(fa - fb);
                    case TokenType::Star: return make_const_float(fa * fb);
                    case TokenType::Slash: return make_const_float(fb != 0.0f ? fa / fb : 0.0f);
                    case TokenType::EqualEqual: return make_const_bool(std::abs(fa - fb) < 1e-6f);
                    case TokenType::ExclEqual: return make_const_bool(std::abs(fa - fb) >= 1e-6f);
                    case TokenType::Less: return make_const_bool(fa < fb);
                    case TokenType::LessEqual: return make_const_bool(fa <= fb);
                    case TokenType::Greater: return make_const_bool(fa > fb);
                    case TokenType::GreaterEqual: return make_const_bool(fa >= fb);
                    default: break;
                }
            } else if (a.kind == TypeKind::Bool && b.kind == TypeKind::Bool) {
                switch (op) {
                    case TokenType::AmpAmp:
                    case TokenType::Amp: return make_const_bool(a.b_val && b.b_val);
                    case TokenType::PipePipe:
                    case TokenType::Pipe: return make_const_bool(a.b_val || b.b_val);
                    case TokenType::Caret: return make_const_bool(a.b_val ^ b.b_val);
                    case TokenType::EqualEqual: return make_const_bool(a.b_val == b.b_val);
                    case TokenType::ExclEqual: return make_const_bool(a.b_val != b.b_val);
                    default: break;
                }
            }
        }

        // Determine target opcode and element type
        TypeKind res_kind = TypeKind::Float;
        std::string elem_type;
        std::string dt_str;

        bool is_comparison = (op == TokenType::EqualEqual || op == TokenType::ExclEqual ||
                              op == TokenType::Less || op == TokenType::LessEqual ||
                              op == TokenType::Greater || op == TokenType::GreaterEqual);
        bool is_logical = (op == TokenType::AmpAmp || op == TokenType::PipePipe);

        if (is_logical) {
            res_kind = TypeKind::Bool;
            elem_type = (op == TokenType::AmpAmp) ? "AND" : "OR";
        } else if (is_comparison) {
            res_kind = TypeKind::Bool;
            if (op == TokenType::EqualEqual) elem_type = "CMP_EQ";
            else if (op == TokenType::ExclEqual) elem_type = "CMP_NE";
            else if (op == TokenType::Less) elem_type = "CMP_LT";
            else if (op == TokenType::LessEqual) elem_type = "CMP_LE";
            else if (op == TokenType::Greater) elem_type = "CMP_GT";
            else if (op == TokenType::GreaterEqual) elem_type = "CMP_GE";
        } else if (a.kind == TypeKind::Complex || b.kind == TypeKind::Complex) {
            res_kind = TypeKind::Complex;
            if (op == TokenType::Plus) elem_type = "CADD";
            else if (op == TokenType::Minus) elem_type = "CSUB";
            else if (op == TokenType::Star) elem_type = "CMUL";
            else if (op == TokenType::Slash) elem_type = "CDIV";
        } else if (a.kind == TypeKind::Int && b.kind == TypeKind::Int) {
            res_kind = TypeKind::Int;
            dt_str = "int";
            if (op == TokenType::Plus) elem_type = "ADD_I";
            else if (op == TokenType::Minus) elem_type = "SUB_I";
            else if (op == TokenType::Star) elem_type = "MUL_I";
            else if (op == TokenType::Slash) elem_type = "DIV_I";
            else if (op == TokenType::Amp) elem_type = "WORD_AND";
            else if (op == TokenType::Pipe) elem_type = "WORD_OR";
            else if (op == TokenType::Caret) elem_type = "WORD_XOR";
        } else if (a.kind == TypeKind::Bool && b.kind == TypeKind::Bool) {
            res_kind = TypeKind::Bool;
            if (op == TokenType::Amp) elem_type = "AND";
            else if (op == TokenType::Pipe) elem_type = "OR";
            else if (op == TokenType::Caret) elem_type = "XOR";
        } else {
            if (op == TokenType::Amp || op == TokenType::Pipe || op == TokenType::Caret) {
                throw std::runtime_error("Bitwise operations (&, |, ^) are only supported on 'int' or 'bool' types, not floating-point or complex");
            }
            res_kind = TypeKind::Float;
            dt_str = "float";
            if (op == TokenType::Plus) elem_type = "ADD";
            else if (op == TokenType::Minus) elem_type = "SUB";
            else if (op == TokenType::Star) elem_type = "MUL";
            else if (op == TokenType::Slash) elem_type = "DIV";
        }

        if (elem_type.empty()) {
            throw std::runtime_error("Unsupported binary operator for given operand types");
        }

        std::string ename = new_element_name(elem_type);
        JsonValue el;
        el.set("name", ename);
        el.set("type", elem_type);
        if (!dt_str.empty()) el.set("dataType", dt_str);
        add_element(el);

        WireRef act_a = a;
        WireRef act_b = b;
        if (res_kind == TypeKind::Float) {
            if (act_a.is_const && act_a.kind == TypeKind::Int) act_a = make_const_float((float)act_a.i_val);
            if (act_b.is_const && act_b.kind == TypeKind::Int) act_b = make_const_float((float)act_b.i_val);
        }
        add_net(act_a, ename, "a");
        add_net(act_b, ename, "b");

        WireRef out;
        out.kind = res_kind;
        out.node_name = ename;
        out.port_name = "out";
        return out;
    }

    WireRef synthesize_unary_op(TokenType op, const WireRef& a) {
        if (a.is_const) {
            if (a.kind == TypeKind::Bool && op == TokenType::Excl) {
                return make_const_bool(!a.b_val);
            }
            if (a.kind == TypeKind::Int) {
                if (op == TokenType::Minus) return make_const_int(-a.i_val);
                if (op == TokenType::Tilde) return make_const_int(~a.i_val);
            }
            if (a.kind == TypeKind::Float && op == TokenType::Minus) {
                return make_const_float(-a.f_val);
            }
        }

        if (op == TokenType::Excl) {
            std::string ename = new_element_name("NOT");
            JsonValue el;
            el.set("name", ename);
            el.set("type", "NOT");
            add_element(el);
            add_net(a, ename, "in");
            return {TypeKind::Bool, false, false, 0, 0.0f, 0.0f, 0.0f, ename, "out"};
        } else if (op == TokenType::Minus) {
            if (a.kind == TypeKind::Int) {
                std::string ename = new_element_name("NEG_I");
                JsonValue el;
                el.set("name", ename);
                el.set("type", "NEG_I");
                add_element(el);
                add_net(a, ename, "a");
                return {TypeKind::Int, false, false, 0, 0.0f, 0.0f, 0.0f, ename, "out"};
            } else {
                std::string ename = new_element_name("NEG");
                JsonValue el;
                el.set("name", ename);
                el.set("type", "NEG");
                add_element(el);
                add_net(a, ename, "a");
                return {TypeKind::Float, false, false, 0, 0.0f, 0.0f, 0.0f, ename, "out"};
            }
        } else if (op == TokenType::Tilde && a.kind == TypeKind::Int) {
            std::string ename = new_element_name("WORD_NOT");
            JsonValue el;
            el.set("name", ename);
            el.set("type", "WORD_NOT");
            add_element(el);
            add_net(a, ename, "a");
            return {TypeKind::Int, false, false, 0, 0.0f, 0.0f, 0.0f, ename, "out"};
        } else if (op == TokenType::Tilde) {
            throw std::runtime_error("Bitwise NOT (~) is only supported on 'int' type, not floating-point or complex");
        }

        throw std::runtime_error("Unsupported unary operator");
    }

    WireRef synthesize_mux(const WireRef& cond, const WireRef& in_then, const WireRef& in_else) {
        if (cond.is_const) {
            return cond.b_val ? in_then : in_else;
        }

        TypeKind tk = in_then.kind;
        std::string dt_str = "float";
        if (tk == TypeKind::Bool) dt_str = "bool";
        else if (tk == TypeKind::Int) dt_str = "int";
        else if (tk == TypeKind::Complex) dt_str = "complex";

        std::string mname = new_element_name("MUX");
        JsonValue el;
        el.set("name", mname);
        el.set("type", "MUX");
        el.set("dataType", dt_str);
        add_element(el);

        WireRef act_th = in_then;
        WireRef act_el = in_else;
        if (tk == TypeKind::Float) {
            if (act_th.is_const && act_th.kind == TypeKind::Int) act_th = make_const_float((float)act_th.i_val);
            if (act_el.is_const && act_el.kind == TypeKind::Int) act_el = make_const_float((float)act_el.i_val);
        }

        add_net(cond, mname, "sel");
        add_net(act_el, mname, "in0");
        add_net(act_th, mname, "in1");

        WireRef out;
        out.kind = tk;
        out.node_name = mname;
        out.port_name = "out";
        return out;
    }

    void emit_warning(const std::string& msg, const std::string& level = "WARNING") {
        m_warnings.push_back("[" + level + "] Code block '" + m_block_name + "': " + msg);
    }

private:
    std::string m_block_name;
    std::vector<JsonValue>& m_elements;
    std::vector<JsonValue>& m_nets;
    std::vector<std::string>& m_warnings;
    int m_counter{0};
};

// -----------------------------------------------------------------------------
// Evaluator & Inliner
// -----------------------------------------------------------------------------
class CodeBlockEvaluator {
public:
    CodeBlockEvaluator(LoweringContext& ctx,
                       const std::map<std::string, FunctionDef>& functions)
        : m_ctx(ctx), m_functions(functions) {}

    // Environment: maps variable name (scalar or "arr_idx") to WireRef
    std::map<std::string, WireRef> env;
    std::set<std::string> declared_static;
    std::set<std::string> defined_in_scan;

    std::set<std::string> m_referenced_identifiers;
    std::set<std::string> m_assigned_keys;
    std::set<std::string> m_loop_vars;
    std::vector<std::map<std::string, int>> m_scope_assignment_counts;
    std::vector<std::set<std::string>> m_loop_assigned_scalars_stack;
    std::vector<std::set<std::string>> m_loop_warned_scalars_stack;

    void record_assignment(const std::string& key) {
        m_assigned_keys.insert(key);

        if (m_loop_vars.count(key) > 0) {
            return;
        }

        if (!m_scope_assignment_counts.empty()) {
            int& count = m_scope_assignment_counts.back()[key];
            count++;
            if (count == 2) {
                m_ctx.emit_warning("multiple unconditional drivers for net '" + key + "'. In circuit synthesis, each net can only have one driver per execution path.");
            }
        }

        for (size_t i = 0; i < m_loop_assigned_scalars_stack.size(); ++i) {
            auto& loop_set = m_loop_assigned_scalars_stack[i];
            auto& warned_set = m_loop_warned_scalars_stack[i];
            if (loop_set.count(key) > 0) {
                if (warned_set.insert(key).second) {
                    m_ctx.emit_warning("multiple unconditional drivers for net '" + key + "' across loop iterations. In circuit synthesis, each net can only have one driver.");
                }
            } else {
                loop_set.insert(key);
            }
        }
    }

    WireRef eval_expr(const std::shared_ptr<Expr>& expr) {
        if (auto lit = std::dynamic_pointer_cast<LiteralExpr>(expr)) {
            WireRef w;
            w.kind = lit->type.kind;
            w.is_const = true;
            w.b_val = lit->b_val;
            w.i_val = lit->i_val;
            w.f_val = lit->f_val;
            w.c_real = lit->c_real;
            w.c_imag = lit->c_imag;
            return w;
        }

        if (auto vr = std::dynamic_pointer_cast<VarRefExpr>(expr)) {
            std::string var_name = vr->name;
            if (vr->index_expr) {
                // Array indexing
                WireRef idx_w = eval_expr(vr->index_expr);
                if (idx_w.is_const && idx_w.kind == TypeKind::Int) {
                    var_name += "_" + std::to_string(idx_w.i_val);
                    return lookup_var(var_name);
                } else {
                    // Variable array read: find array size and generate MUX tree
                    int arr_len = get_array_length(vr->name);
                    if (arr_len <= 0) {
                        throw std::runtime_error("Dynamic indexing on non-array variable '" + vr->name + "'");
                    }
                    // MUX tree: arr[0] if idx==0, else arr[1] if idx==1, ...
                    WireRef result = lookup_var(vr->name + "_0");
                    for (int i = 1; i < arr_len; i++) {
                        WireRef elem_w = lookup_var(vr->name + "_" + std::to_string(i));
                        WireRef eq_w = m_ctx.synthesize_binary_op(TokenType::EqualEqual, idx_w, m_ctx.make_const_int(i));
                        result = m_ctx.synthesize_mux(eq_w, elem_w, result);
                    }
                    return result;
                }
            } else {
                WireRef w = lookup_var(var_name);
                if (!vr->member.empty()) {
                    if (w.kind != TypeKind::Complex) {
                        throw std::runtime_error("Member access '." + vr->member + "' on non-complex variable");
                    }
                    if (w.is_const) {
                        return m_ctx.make_const_float(vr->member == "real" ? w.c_real : w.c_imag);
                    }
                    // Complex member extraction
                    std::string c2r_name = m_ctx.new_element_name("c2r");
                    JsonValue c2r_el;
                    c2r_el.set("name", c2r_name);
                    c2r_el.set("type", "COMPLEX2RECT");
                    m_ctx.add_element(c2r_el);
                    m_ctx.add_net(w, c2r_name, "in");
                    return {TypeKind::Float, false, false, 0, 0.0f, 0.0f, 0.0f, c2r_name, vr->member};
                }
                return w;
            }
        }

        if (auto un = std::dynamic_pointer_cast<UnaryExpr>(expr)) {
            WireRef sub = eval_expr(un->expr);
            return m_ctx.synthesize_unary_op(un->op, sub);
        }

        if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
            WireRef left = eval_expr(bin->left);
            WireRef right = eval_expr(bin->right);
            return m_ctx.synthesize_binary_op(bin->op, left, right);
        }

        if (auto tern = std::dynamic_pointer_cast<TernaryExpr>(expr)) {
            WireRef cond = eval_expr(tern->cond);
            WireRef th = eval_expr(tern->then_expr);
            WireRef el = eval_expr(tern->else_expr);
            return m_ctx.synthesize_mux(cond, th, el);
        }

        if (auto call = std::dynamic_pointer_cast<CallExpr>(expr)) {
            auto to_float_val = [](const WireRef& w) -> float {
                if (w.kind == TypeKind::Int) return static_cast<float>(w.i_val);
                if (w.kind == TypeKind::Bool) return w.b_val ? 1.0f : 0.0f;
                return w.f_val;
            };

            if (call->name == "complex") {
                if (call->args.size() != 2) throw std::runtime_error("complex() constructor takes 2 arguments (real, imag)");
                WireRef r = eval_expr(call->args[0]);
                WireRef i = eval_expr(call->args[1]);
                if (r.is_const && i.is_const) {
                    float fr = (r.kind == TypeKind::Float) ? r.f_val : (float)r.i_val;
                    float fi = (i.kind == TypeKind::Float) ? i.f_val : (float)i.i_val;
                    return m_ctx.make_const_complex(fr, fi);
                }
                std::string r2c_name = m_ctx.new_element_name("r2c");
                JsonValue r2c_el;
                r2c_el.set("name", r2c_name);
                r2c_el.set("type", "RECT2COMPLEX");
                m_ctx.add_element(r2c_el);
                m_ctx.add_net(r, r2c_name, "real");
                m_ctx.add_net(i, r2c_name, "imag");
                return {TypeKind::Complex, false, false, 0, 0.0f, 0.0f, 0.0f, r2c_name, "out"};
            }

            // 1-argument math & trigonometric built-ins
            if (call->name == "sqrt" || call->name == "sin" || call->name == "cos" ||
                call->name == "tan" || call->name == "asin" || call->name == "acos" ||
                call->name == "atan" || call->name == "exp" || call->name == "log" ||
                call->name == "ln" || call->name == "log10" || call->name == "abs" ||
                call->name == "fabs") {
                if (call->args.size() != 1) {
                    throw std::runtime_error(call->name + "() expects exactly 1 argument");
                }
                WireRef a = eval_expr(call->args[0]);
                if (a.is_const) {
                    if (call->name == "abs" && a.kind == TypeKind::Int) {
                        return m_ctx.make_const_int(std::abs(a.i_val));
                    }
                    float val = to_float_val(a);
                    float res = 0.0f;
                    if (call->name == "sqrt") res = le_fast_sqrt(val);
                    else if (call->name == "sin") res = le_fast_sin(val);
                    else if (call->name == "cos") res = le_fast_cos(val);
                    else if (call->name == "tan") res = le_fast_tan(val);
                    else if (call->name == "asin") res = le_fast_asin(val);
                    else if (call->name == "acos") res = le_fast_acos(val);
                    else if (call->name == "atan") res = le_fast_atan(val);
                    else if (call->name == "exp") res = le_fast_exp(val);
                    else if (call->name == "log" || call->name == "ln") res = le_fast_log(val);
                    else if (call->name == "log10") res = le_fast_log10(val);
                    else if (call->name == "abs" || call->name == "fabs") res = fabsf(val);
                    return m_ctx.make_const_float(res);
                }
                std::string elem_type;
                if (call->name == "sqrt") elem_type = "SQRT";
                else if (call->name == "sin") elem_type = "SIN";
                else if (call->name == "cos") elem_type = "COS";
                else if (call->name == "tan") elem_type = "TAN";
                else if (call->name == "asin") elem_type = "ASIN";
                else if (call->name == "acos") elem_type = "ACOS";
                else if (call->name == "atan") elem_type = "ATAN";
                else if (call->name == "exp") elem_type = "EXP";
                else if (call->name == "log" || call->name == "ln") elem_type = "LOG";
                else if (call->name == "log10") elem_type = "LOG10";
                else if (call->name == "abs" || call->name == "fabs") elem_type = "ABS";

                std::string ename = m_ctx.new_element_name(elem_type);
                JsonValue el;
                el.set("name", ename);
                el.set("type", elem_type);
                m_ctx.add_element(el);
                m_ctx.add_net(a, ename, "a");
                return {TypeKind::Float, false, false, 0, 0.0f, 0.0f, 0.0f, ename, "out"};
            }

            // 2-argument math & trigonometric built-ins
            if (call->name == "atan2" || call->name == "pow" || call->name == "power" ||
                call->name == "min" || call->name == "max") {
                if (call->args.size() != 2) {
                    throw std::runtime_error(call->name + "() expects exactly 2 arguments");
                }
                WireRef a = eval_expr(call->args[0]);
                WireRef b = eval_expr(call->args[1]);
                if (a.is_const && b.is_const) {
                    if ((call->name == "min" || call->name == "max") &&
                        a.kind == TypeKind::Int && b.kind == TypeKind::Int) {
                        int ires = (call->name == "min") ? std::min(a.i_val, b.i_val) : std::max(a.i_val, b.i_val);
                        return m_ctx.make_const_int(ires);
                    }
                    float fa = to_float_val(a);
                    float fb = to_float_val(b);
                    float res = 0.0f;
                    if (call->name == "atan2") res = le_fast_atan2(fa, fb);
                    else if (call->name == "pow" || call->name == "power") res = le_fast_pow(fa, fb);
                    else if (call->name == "min") res = (fa < fb) ? fa : fb;
                    else if (call->name == "max") res = (fa > fb) ? fa : fb;
                    return m_ctx.make_const_float(res);
                }
                std::string elem_type;
                if (call->name == "atan2") elem_type = "ATAN2";
                else if (call->name == "pow" || call->name == "power") elem_type = "POW";
                else if (call->name == "min") elem_type = "MIN";
                else if (call->name == "max") elem_type = "MAX";

                std::string ename = m_ctx.new_element_name(elem_type);
                JsonValue el;
                el.set("name", ename);
                el.set("type", elem_type);
                m_ctx.add_element(el);
                m_ctx.add_net(a, ename, "a");
                m_ctx.add_net(b, ename, "b");
                return {TypeKind::Float, false, false, 0, 0.0f, 0.0f, 0.0f, ename, "out"};
            }

            // 3-argument math built-ins (clamp)
            if (call->name == "clamp") {
                if (call->args.size() != 3) {
                    throw std::runtime_error("clamp() expects exactly 3 arguments (val, min, max)");
                }
                WireRef v = eval_expr(call->args[0]);
                WireRef lo = eval_expr(call->args[1]);
                WireRef hi = eval_expr(call->args[2]);
                if (v.is_const && lo.is_const && hi.is_const) {
                    if (v.kind == TypeKind::Int && lo.kind == TypeKind::Int && hi.kind == TypeKind::Int) {
                        int iv = v.i_val, ilo = lo.i_val, ihi = hi.i_val;
                        int ires = (iv < ilo) ? ilo : ((iv > ihi) ? ihi : iv);
                        return m_ctx.make_const_int(ires);
                    }
                    float fv = to_float_val(v), flo = to_float_val(lo), fhi = to_float_val(hi);
                    float fres = (fv < flo) ? flo : ((fv > fhi) ? fhi : fv);
                    return m_ctx.make_const_float(fres);
                }
                std::string ename = m_ctx.new_element_name("CLAMP");
                JsonValue el;
                el.set("name", ename);
                el.set("type", "CLAMP");
                m_ctx.add_element(el);
                m_ctx.add_net(v, ename, "in");
                m_ctx.add_net(lo, ename, "min");
                m_ctx.add_net(hi, ename, "max");
                return {TypeKind::Float, false, false, 0, 0.0f, 0.0f, 0.0f, ename, "out"};
            }

            // User-defined function call
            auto it = m_functions.find(call->name);
            if (it == m_functions.end()) {
                throw std::runtime_error("Undefined function '" + call->name + "'");
            }
            return inline_function_call(it->second, call->args);
        }

        throw std::runtime_error("Unknown expression node in evaluator");
    }

    void execute_stmt(const std::shared_ptr<Stmt>& stmt) {
        if (!stmt) return;

        if (auto bs = std::dynamic_pointer_cast<BlockStmt>(stmt)) {
            m_scope_assignment_counts.emplace_back();
            for (const auto& s : bs->stmts) {
                execute_stmt(s);
                auto it_has = env.find("__has_returned");
                if (it_has != env.end() && it_has->second.is_const && it_has->second.b_val) {
                    break;
                }
            }
            m_scope_assignment_counts.pop_back();
            return;
        }

        if (auto es = std::dynamic_pointer_cast<ExprStmt>(stmt)) {
            eval_expr(es->expr);
            return;
        }

        if (auto ds = std::dynamic_pointer_cast<VarDeclStmt>(stmt)) {
            if (ds->type.array_size > 0) {
                set_array_length(ds->name, ds->type.array_size);
                for (int i = 0; i < ds->type.array_size; i++) {
                    std::string key = ds->name + "_" + std::to_string(i);
                    if (ds->is_static) {
                        declared_static.insert(key);
                        declared_static.insert(ds->name);
                        WireRef init_w;
                        if (ds->init_expr) {
                            init_w = eval_expr(ds->init_expr);
                            record_assignment(key);
                        }
                        WireRef reg = get_or_create_feedback_register(key, ds->type.kind, ds->init_expr ? &init_w : nullptr);
                        env[key] = reg;
                    } else if (ds->init_expr) {
                        env[key] = eval_expr(ds->init_expr);
                        record_assignment(key);
                        defined_in_scan.insert(key);
                    } else {
                        env[key] = default_value_for(ds->type.kind);
                        defined_in_scan.insert(key);
                    }
                }
            } else {
                if (ds->is_static) {
                    declared_static.insert(ds->name);
                    WireRef init_w;
                    if (ds->init_expr) {
                        init_w = eval_expr(ds->init_expr);
                        record_assignment(ds->name);
                    }
                    WireRef reg = get_or_create_feedback_register(ds->name, ds->type.kind, ds->init_expr ? &init_w : nullptr);
                    env[ds->name] = reg;
                } else if (ds->init_expr) {
                    env[ds->name] = eval_expr(ds->init_expr);
                    record_assignment(ds->name);
                    defined_in_scan.insert(ds->name);
                } else {
                    env[ds->name] = default_value_for(ds->type.kind);
                    defined_in_scan.insert(ds->name);
                }
            }
            return;
        }

        if (auto as = std::dynamic_pointer_cast<AssignStmt>(stmt)) {
            WireRef rhs_val = eval_expr(as->rhs);
            if (as->index_expr) {
                WireRef idx_w = eval_expr(as->index_expr);
                if (idx_w.is_const && idx_w.kind == TypeKind::Int) {
                    std::string key = as->name + "_" + std::to_string(idx_w.i_val);
                    apply_assignment(key, as->op, rhs_val);
                } else {
                    // Variable array write: arr[k] = val -> for all i: arr_i = (k == i) ? val : arr_i
                    int arr_len = get_array_length(as->name);
                    if (arr_len <= 0) throw std::runtime_error("Dynamic array assignment to non-array '" + as->name + "'");
                    for (int i = 0; i < arr_len; i++) {
                        std::string key = as->name + "_" + std::to_string(i);
                        WireRef cur = lookup_var(key);
                        WireRef eq_w = m_ctx.synthesize_binary_op(TokenType::EqualEqual, idx_w, m_ctx.make_const_int(i));
                        WireRef updated_val = rhs_val;
                        if (as->op != TokenType::Equal) {
                            TokenType bin_op = assign_op_to_binary(as->op);
                            updated_val = m_ctx.synthesize_binary_op(bin_op, cur, rhs_val);
                        }
                        record_assignment(key);
                        env[key] = m_ctx.synthesize_mux(eq_w, updated_val, cur);
                        defined_in_scan.insert(key);
                    }
                }
            } else {
                apply_assignment(as->name, as->op, rhs_val);
            }
            return;
        }

        if (auto ifs = std::dynamic_pointer_cast<IfStmt>(stmt)) {
            WireRef cond_w = eval_expr(ifs->cond);
            if (cond_w.is_const) {
                if (cond_w.b_val) execute_stmt(ifs->then_branch);
                else if (ifs->else_branch) execute_stmt(ifs->else_branch);
                return;
            }

            // Non-constant condition: fork environments
            auto env_before = env;
            auto defined_before = defined_in_scan;

            execute_stmt(ifs->then_branch);
            auto env_then = env;

            env = env_before;
            if (ifs->else_branch) {
                execute_stmt(ifs->else_branch);
            }
            auto env_else = env;

            // Merge back using MUX
            std::set<std::string> all_modified;
            for (const auto& kv : env_then) {
                if (env_before.find(kv.first) == env_before.end() ||
                    env_before[kv.first] != kv.second) {
                    all_modified.insert(kv.first);
                }
            }
            for (const auto& kv : env_else) {
                if (env_before.find(kv.first) == env_before.end() ||
                    env_before[kv.first] != kv.second) {
                    all_modified.insert(kv.first);
                }
            }

            for (const std::string& var : all_modified) {
                WireRef val_then = (env_then.find(var) != env_then.end()) ? env_then[var] : env_before[var];
                WireRef val_else;
                if (env_else.find(var) != env_else.end() && ifs->else_branch) {
                    val_else = env_else[var];
                } else {
                    // Variable modified in then but no else (or not modified in else)
                    if (defined_before.find(var) != defined_before.end()) {
                        val_else = env_before[var];
                    } else {
                        // Inferred latch!
                        val_else = get_or_create_feedback_register(var, val_then.kind);
                        if (declared_static.find(var) == declared_static.end()) {
                            m_ctx.emit_warning("inferred latch for variable '" + var +
                                               "' (retains prior cycle value). Add explicit else branch or declare as 'static' to suppress.");
                        }
                    }
                }
                env[var] = m_ctx.synthesize_mux(cond_w, val_then, val_else);
                defined_in_scan.insert(var);
            }
            return;
        }

        if (auto fs = std::dynamic_pointer_cast<ForStmt>(stmt)) {
            // Unroll for-loop
            // Must have constant bounds
            std::string loop_var_name;
            if (auto vds = std::dynamic_pointer_cast<VarDeclStmt>(fs->init_stmt)) {
                loop_var_name = vds->name;
                m_loop_vars.insert(loop_var_name);
            } else if (auto as = std::dynamic_pointer_cast<AssignStmt>(fs->init_stmt)) {
                loop_var_name = as->name;
                m_loop_vars.insert(loop_var_name);
            }

            execute_stmt(fs->init_stmt);
            m_loop_assigned_scalars_stack.emplace_back();
            m_loop_warned_scalars_stack.emplace_back();

            int loop_iterations = 0;
            const int MAX_UNROLL = 1024;
            while (true) {
                if (loop_iterations++ > MAX_UNROLL) {
                    throw std::runtime_error("Exceeded maximum loop unroll limit (1024 iterations)");
                }
                if (fs->cond_expr) {
                    WireRef cond_w = eval_expr(fs->cond_expr);
                    if (!cond_w.is_const || cond_w.kind != TypeKind::Bool) {
                        throw std::runtime_error("For loop condition must evaluate to a compile-time constant boolean");
                    }
                    if (!cond_w.b_val) break;
                }
                execute_stmt(fs->body);
                if (fs->inc_stmt) {
                    execute_stmt(fs->inc_stmt);
                }
            }

            m_loop_assigned_scalars_stack.pop_back();
            m_loop_warned_scalars_stack.pop_back();
            if (!loop_var_name.empty()) {
                m_loop_vars.erase(loop_var_name);
            }
            return;
        }

        if (auto rs = std::dynamic_pointer_cast<ReturnStmt>(stmt)) {
            WireRef val = rs->expr ? eval_expr(rs->expr) : default_value_for(TypeKind::Float);
            auto it_has = env.find("__has_returned");
            if (it_has != env.end()) {
                WireRef cur_has = it_has->second;
                WireRef cur_ret = env["__return_val"];
                env["__return_val"] = m_ctx.synthesize_mux(cur_has, cur_ret, val);
                env["__has_returned"] = m_ctx.synthesize_binary_op(TokenType::PipePipe, cur_has, m_ctx.make_const_bool(true));
            } else {
                m_return_value = val;
            }
            m_returned = true;
            return;
        }

        throw std::runtime_error("Unknown statement type in evaluator");
    }

    void set_array_length(const std::string& name, int len) {
        m_array_lengths[name] = len;
    }

    int get_array_length(const std::string& name) const {
        auto it = m_array_lengths.find(name);
        return (it != m_array_lengths.end()) ? it->second : 0;
    }

    WireRef get_or_create_feedback_register(const std::string& var_name, TypeKind kind, const WireRef* initial_val = nullptr) {
        auto it = m_feedback_registers.find(var_name);
        if (it != m_feedback_registers.end()) return it->second;

        std::string rtype = "FLOATREGISTER";
        if (kind == TypeKind::Bool) rtype = "BOOLREGISTER";
        else if (kind == TypeKind::Int) rtype = "INTREGISTER";
        else if (kind == TypeKind::Complex) rtype = "COMPLEXREGISTER";

        std::string reg_name = m_ctx.new_element_name(rtype + "_" + var_name);
        JsonValue el;
        el.set("name", reg_name);
        el.set("type", rtype);
        if (initial_val && initial_val->is_const) {
            if (kind == TypeKind::Bool) el.set("value", initial_val->b_val);
            else if (kind == TypeKind::Int) el.set("value", initial_val->i_val);
            else if (kind == TypeKind::Float) el.set("value", initial_val->f_val);
        }
        m_ctx.add_element(el);

        WireRef out;
        out.kind = kind;
        out.node_name = reg_name;
        out.port_name = "out";
        m_feedback_registers[var_name] = out;
        return out;
    }

    const std::map<std::string, WireRef>& feedback_registers() const {
        return m_feedback_registers;
    }

private:
    WireRef lookup_var(const std::string& name) {
        m_referenced_identifiers.insert(name);
        if (declared_static.find(name) != declared_static.end()) {
            auto it_reg = m_feedback_registers.find(name);
            if (it_reg != m_feedback_registers.end()) {
                return it_reg->second;
            }
        }
        auto it = env.find(name);
        if (it != env.end()) return it->second;

        // If variable has not been initialized yet, treat as registered feedback state
        WireRef reg = get_or_create_feedback_register(name, TypeKind::Float);
        if (declared_static.find(name) == declared_static.end()) {
            m_ctx.emit_warning("inferred latch for uninitialized variable '" + name + "'. Combinatorial nets cannot feed back without clocked storage; declare as 'static' to create a dedicated register.");
        }
        env[name] = reg;
        return reg;
    }

    void apply_assignment(const std::string& key, TokenType op, const WireRef& rhs_val) {
        record_assignment(key);
        if (op == TokenType::Equal) {
            env[key] = rhs_val;
        } else {
            TokenType bin_op = assign_op_to_binary(op);
            WireRef cur = lookup_var(key);
            env[key] = m_ctx.synthesize_binary_op(bin_op, cur, rhs_val);
        }
        defined_in_scan.insert(key);
    }

    static TokenType assign_op_to_binary(TokenType op) {
        switch (op) {
            case TokenType::PlusEqual: return TokenType::Plus;
            case TokenType::MinusEqual: return TokenType::Minus;
            case TokenType::StarEqual: return TokenType::Star;
            case TokenType::SlashEqual: return TokenType::Slash;
            default: return TokenType::Equal;
        }
    }

    WireRef default_value_for(TypeKind tk) {
        switch (tk) {
            case TypeKind::Bool: return m_ctx.make_const_bool(false);
            case TypeKind::Int: return m_ctx.make_const_int(0);
            case TypeKind::Complex: return m_ctx.make_const_complex(0.0f, 0.0f);
            default: return m_ctx.make_const_float(0.0f);
        }
    }

    WireRef inline_function_call(const FunctionDef& fn, const std::vector<std::shared_ptr<Expr>>& args) {
        if (m_ctx.call_stack.find(fn.name) != m_ctx.call_stack.end()) {
            throw std::runtime_error("Recursion is prohibited in deterministic code block ('" + fn.name + "')");
        }
        if (args.size() != fn.params.size()) {
            throw std::runtime_error("Argument count mismatch calling '" + fn.name + "'");
        }
        m_ctx.call_stack.insert(fn.name);

        CodeBlockEvaluator sub_eval(m_ctx, m_functions);
        sub_eval.m_array_lengths = m_array_lengths;
        for (size_t i = 0; i < fn.params.size(); i++) {
            sub_eval.env[fn.params[i].name] = eval_expr(args[i]);
            sub_eval.defined_in_scan.insert(fn.params[i].name);
        }
        sub_eval.env["__return_val"] = sub_eval.default_value_for(fn.return_type.kind);
        sub_eval.env["__has_returned"] = m_ctx.make_const_bool(false);
        sub_eval.defined_in_scan.insert("__return_val");
        sub_eval.defined_in_scan.insert("__has_returned");

        sub_eval.execute_stmt(fn.body);
        m_ctx.call_stack.erase(fn.name);
        return sub_eval.env["__return_val"];
    }

    LoweringContext& m_ctx;
    const std::map<std::string, FunctionDef>& m_functions;
    std::map<std::string, int> m_array_lengths;
    std::map<std::string, WireRef> m_feedback_registers;
    WireRef m_return_value;
    bool m_returned{false};
};

// -----------------------------------------------------------------------------
// Top-Level CodeBlockExpander Implementation
// -----------------------------------------------------------------------------
bool CodeBlockExpander::expand_code_blocks(JsonValue& circuit_doc, std::string& out_error, std::vector<std::string>& out_warnings) {
    if (!circuit_doc.is_object() || !circuit_doc.contains("elements")) {
        return true;
    }

    auto& elem_arr = circuit_doc["elements"].arr_val;
    std::vector<size_t> code_block_indices;
    for (size_t i = 0; i < elem_arr.size(); i++) {
        if (!elem_arr[i].is_object()) continue;
        std::string type = elem_arr[i].get("type").as_string();
        std::transform(type.begin(), type.end(), type.begin(), ::toupper);
        if (type == "CODE_BLOCK" || type == "LE_CODE_BLOCK" || type == "CODEBLOCK") {
            code_block_indices.push_back(i);
        }
    }

    if (code_block_indices.empty()) {
        return true;
    }

    auto& net_arr = circuit_doc["nets"].arr_val;
    std::vector<JsonValue> new_elements;
    std::vector<JsonValue> new_nets;
    std::set<std::string> removed_code_block_names;

    try {
        for (size_t idx : code_block_indices) {
            const auto& cb_el = elem_arr[idx];
            std::string block_name = cb_el.get("name").as_string();
            removed_code_block_names.insert(block_name);

            std::string code = cb_el.get("code").as_string();
            if (code.empty()) {
                throw std::runtime_error("CODE_BLOCK '" + block_name + "' has empty 'code' property");
            }

            // Parse C code into AST
            Parser parser(code);
            std::vector<FunctionDef> functions;
            std::vector<std::shared_ptr<Stmt>> top_stmts;
            parser.parse_program(functions, top_stmts);

            std::map<std::string, FunctionDef> func_map;
            for (const auto& fn : functions) {
                func_map[fn.name] = fn;
            }

            // Lowering context
            LoweringContext ctx(block_name, new_elements, new_nets, out_warnings);
            CodeBlockEvaluator evaluator(ctx, func_map);

            // Bind declared inputs
            if (cb_el.contains("inputs") && cb_el.get("inputs").is_array()) {
                for (const auto& inp : cb_el.get("inputs").as_array()) {
                    std::string in_name = inp.get("name").as_string();
                    std::string in_type = inp.get("type").as_string("float");
                    std::transform(in_type.begin(), in_type.end(), in_type.begin(), ::tolower);

                    TypeKind tk = TypeKind::Float;
                    int arr_len = 0;
                    if (in_type.find("bool") != std::string::npos) tk = TypeKind::Bool;
                    else if (in_type.find("int") != std::string::npos) tk = TypeKind::Int;
                    else if (in_type.find("complex") != std::string::npos) tk = TypeKind::Complex;

                    auto bpos = in_type.find('[');
                    if (bpos != std::string::npos) {
                        arr_len = std::stoi(in_type.substr(bpos + 1));
                        evaluator.set_array_length(in_name, arr_len);
                        for (int k = 0; k < arr_len; k++) {
                            WireRef w;
                            w.kind = tk;
                            w.node_name = block_name;
                            w.port_name = in_name + "_" + std::to_string(k);
                            evaluator.env[in_name + "_" + std::to_string(k)] = w;
                            evaluator.defined_in_scan.insert(in_name + "_" + std::to_string(k));
                        }
                    } else {
                        WireRef w;
                        w.kind = tk;
                        w.node_name = block_name;
                        w.port_name = in_name;
                        evaluator.env[in_name] = w;
                        evaluator.defined_in_scan.insert(in_name);
                    }
                }
            }

            // Bind circuit-level and element-level scoped variables as compile-time constants
            auto bind_scoped_variables = [&](const JsonValue& v_obj) {
                if (!v_obj.is_object()) return;
                for (const auto& kv : v_obj.as_object()) {
                    const std::string& v_name = kv.first;
                    if (v_name.empty()) continue;
                    // Do not overwrite declared inputs
                    if (evaluator.env.find(v_name) != evaluator.env.end()) continue;

                    const JsonValue& v_val = kv.second;
                    if (v_val.type == JsonType::Boolean) {
                        evaluator.env[v_name] = ctx.make_const_bool(v_val.as_bool());
                    } else if (v_val.type == JsonType::Number) {
                        double d = v_val.as_double();
                        if (std::floor(d) == d && std::abs(d) <= 2147483647.0) {
                            evaluator.env[v_name] = ctx.make_const_int(static_cast<int>(d));
                        } else {
                            evaluator.env[v_name] = ctx.make_const_float(static_cast<float>(d));
                        }
                    } else if (v_val.type == JsonType::String) {
                        if (v_val.str_val == "true" || v_val.str_val == "false" ||
                            v_val.str_val == "True" || v_val.str_val == "False") {
                            evaluator.env[v_name] = ctx.make_const_bool(v_val.as_bool());
                        } else {
                            try {
                                bool is_int = (v_val.str_val.find('.') == std::string::npos &&
                                               v_val.str_val.find('e') == std::string::npos &&
                                               v_val.str_val.find('E') == std::string::npos);
                                if (is_int) {
                                    evaluator.env[v_name] = ctx.make_const_int(std::stoi(v_val.str_val));
                                } else {
                                    evaluator.env[v_name] = ctx.make_const_float(std::stof(v_val.str_val));
                                }
                            } catch (...) {}
                        }
                    }
                    evaluator.defined_in_scan.insert(v_name);
                }
            };

            if (circuit_doc.contains("variables")) {
                bind_scoped_variables(circuit_doc["variables"]);
            }
            if (cb_el.contains("variables")) {
                bind_scoped_variables(cb_el.get("variables"));
            }

            // Execute all statements
            evaluator.m_scope_assignment_counts.emplace_back();
            for (const auto& stmt : top_stmts) {
                evaluator.execute_stmt(stmt);
            }
            evaluator.m_scope_assignment_counts.pop_back();

            // HDL Lints: Check for unused input pins
            if (cb_el.contains("inputs") && cb_el.get("inputs").is_array()) {
                for (const auto& inp : cb_el.get("inputs").as_array()) {
                    std::string in_name = inp.get("name").as_string();
                    if (evaluator.m_referenced_identifiers.count(in_name) == 0) {
                        ctx.emit_warning("input pin '" + in_name + "' is declared in the interface but never referenced in the circuit.", "NOTICE");
                    }
                }
            }

            // HDL Lints: Check for undriven output pins
            if (cb_el.contains("outputs") && cb_el.get("outputs").is_array()) {
                for (const auto& out_spec : cb_el.get("outputs").as_array()) {
                    std::string out_name = out_spec.get("name").as_string();
                    int arr_len = evaluator.get_array_length(out_name);
                    if (arr_len > 0) {
                        for (int k = 0; k < arr_len; ++k) {
                            std::string k_key = out_name + "_" + std::to_string(k);
                            if (evaluator.m_assigned_keys.count(k_key) == 0 && evaluator.env.find(k_key) == evaluator.env.end()) {
                                ctx.emit_warning("output pin '" + out_name + "[" + std::to_string(k) + "]' is declared in the interface but never driven in the circuit.");
                            }
                        }
                    } else {
                        if (evaluator.m_assigned_keys.count(out_name) == 0 && evaluator.env.find(out_name) == evaluator.env.end()) {
                            ctx.emit_warning("output pin '" + out_name + "' is declared in the interface but never driven in the circuit.");
                        }
                    }
                }
            }

            // Close feedback loop for state registers
            for (const auto& kv : evaluator.feedback_registers()) {
                const std::string& var_name = kv.first;
                const WireRef& reg_ref = kv.second;
                auto it_final = evaluator.env.find(var_name);
                if (it_final != evaluator.env.end()) {
                    ctx.add_net(it_final->second, reg_ref.node_name, "in");
                }
            }

            // Rewire external nets connected to outputs of this CODE_BLOCK
            if (cb_el.contains("outputs") && cb_el.get("outputs").is_array()) {
                for (const auto& out_spec : cb_el.get("outputs").as_array()) {
                    std::string out_name = out_spec.get("name").as_string();
                    int arr_len = evaluator.get_array_length(out_name);
                    if (arr_len > 0) {
                        for (int k = 0; k < arr_len; k++) {
                            std::string key = out_name + "_" + std::to_string(k);
                            if (evaluator.env.find(key) != evaluator.env.end()) {
                                WireRef final_w = evaluator.env[key];
                                std::string node_nm = final_w.node_name;
                                std::string port_nm = final_w.port_name;
                                if (final_w.node_name == block_name) {
                                    std::string mtype = "MOVE_F";
                                    if (final_w.kind == TypeKind::Bool) mtype = "MOVE";
                                    else if (final_w.kind == TypeKind::Int) mtype = "MOVE_I";
                                    else if (final_w.kind == TypeKind::Complex) mtype = "MOVE_C";

                                    std::string mv_name = ctx.new_element_name("passthrough");
                                    JsonValue mv_el;
                                    mv_el.set("name", mv_name);
                                    mv_el.set("type", mtype);
                                    ctx.add_element(mv_el);
                                    ctx.add_net_raw(block_name, final_w.port_name, mv_name, "in");
                                    node_nm = mv_name;
                                    port_nm = "out";
                                } else if (final_w.is_const) {
                                    node_nm = ctx.new_element_name("out_const");
                                    port_nm = "out";
                                    JsonValue c_el;
                                    c_el.set("name", node_nm);
                                    c_el.set("type", "CONSTANT");
                                    if (final_w.kind == TypeKind::Bool) {
                                        c_el.set("dataType", "bool");
                                        c_el.set("value", final_w.b_val);
                                    } else if (final_w.kind == TypeKind::Int) {
                                        c_el.set("dataType", "int");
                                        c_el.set("value", (double)final_w.i_val);
                                    } else {
                                        c_el.set("dataType", "float");
                                        c_el.set("value", (double)final_w.f_val);
                                    }
                                    ctx.add_element(c_el);
                                }
                                for (auto& net : net_arr) {
                                    if (!net.is_object() || !net.contains("output")) continue;
                                    auto& out_obj = net["output"];
                                    if (out_obj.get("name").as_string() == block_name) {
                                        std::string p = out_obj.get("port").as_string();
                                        if (p == key || p == (out_name + "[" + std::to_string(k) + "]") ||
                                            p == (out_name + "." + std::to_string(k))) {
                                            out_obj["name"] = JsonValue(node_nm);
                                            out_obj["port"] = JsonValue(port_nm);
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (evaluator.env.find(out_name) != evaluator.env.end()) {
                            WireRef final_w = evaluator.env[out_name];
                            std::string node_nm = final_w.node_name;
                            std::string port_nm = final_w.port_name;
                            if (final_w.node_name == block_name) {
                                std::string mtype = "MOVE_F";
                                if (final_w.kind == TypeKind::Bool) mtype = "MOVE";
                                else if (final_w.kind == TypeKind::Int) mtype = "MOVE_I";
                                else if (final_w.kind == TypeKind::Complex) mtype = "MOVE_C";

                                std::string mv_name = ctx.new_element_name("passthrough");
                                JsonValue mv_el;
                                mv_el.set("name", mv_name);
                                mv_el.set("type", mtype);
                                ctx.add_element(mv_el);
                                ctx.add_net_raw(block_name, final_w.port_name, mv_name, "in");
                                node_nm = mv_name;
                                port_nm = "out";
                            } else if (final_w.is_const) {
                                node_nm = ctx.new_element_name("out_const");
                                port_nm = "out";
                                JsonValue c_el;
                                c_el.set("name", node_nm);
                                c_el.set("type", "CONSTANT");
                                if (final_w.kind == TypeKind::Bool) {
                                    c_el.set("dataType", "bool");
                                    c_el.set("value", final_w.b_val);
                                } else if (final_w.kind == TypeKind::Int) {
                                    c_el.set("dataType", "int");
                                    c_el.set("value", (double)final_w.i_val);
                                } else {
                                    c_el.set("dataType", "float");
                                    c_el.set("value", (double)final_w.f_val);
                                }
                                ctx.add_element(c_el);
                            }
                            for (auto& net : net_arr) {
                                if (!net.is_object() || !net.contains("output")) continue;
                                auto& out_obj = net["output"];
                                if (out_obj.get("name").as_string() == block_name &&
                                    out_obj.get("port").as_string() == out_name) {
                                    out_obj["name"] = JsonValue(node_nm);
                                    out_obj["port"] = JsonValue(port_nm);
                                }
                            }
                        }
                    }
                }
            }

            // Rewire external nets connected to inputs of this CODE_BLOCK
            for (auto& net : net_arr) {
                if (!net.is_object() || !net.contains("inputs")) continue;
                std::vector<JsonValue> rewired_inputs;
                for (const auto& inp_obj : net["inputs"].as_array()) {
                    if (inp_obj.get("name").as_string() == block_name) {
                        std::string target_port = inp_obj.get("port").as_string();
                        // Find all synthesized entry nodes receiving target_port
                        bool found = false;
                        for (const auto& s_net : new_nets) {
                            if (s_net.get("output").get("name").as_string() == block_name &&
                                s_net.get("output").get("port").as_string() == target_port) {
                                for (const auto& dest_inp : s_net.get("inputs").as_array()) {
                                    rewired_inputs.push_back(dest_inp);
                                    found = true;
                                }
                            }
                        }
                        if (!found) {
                            // If input is unused by the synthesized logic, keep as-is or drop
                        }
                    } else {
                        rewired_inputs.push_back(inp_obj);
                    }
                }
                net["inputs"] = JsonValue(rewired_inputs);
            }
        }
    } catch (const std::exception& ex) {
        out_error = ex.what();
        return false;
    }

    // Filter out dummy entry nets and add new nets
    std::vector<JsonValue> filtered_new_nets;
    for (const auto& n : new_nets) {
        if (removed_code_block_names.count(n.get("output").get("name").as_string()) == 0) {
            filtered_new_nets.push_back(n);
        }
    }

    // Remove CODE_BLOCK elements from elements array
    std::vector<JsonValue> remaining_elements;
    for (const auto& el : elem_arr) {
        if (removed_code_block_names.count(el.get("name").as_string()) == 0) {
            remaining_elements.push_back(el);
        }
    }

    // Append newly synthesized elements and nets
    for (const auto& el : new_elements) {
        remaining_elements.push_back(el);
    }
    for (const auto& net : filtered_new_nets) {
        net_arr.push_back(net);
    }

    JsonValue final_elem_val;
    for (const auto& el : remaining_elements) {
        final_elem_val.push_back(el);
    }
    circuit_doc["elements"] = final_elem_val;

    return true;
}

} // namespace LogicElements

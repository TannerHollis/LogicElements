#include "le_compiler_core.h"
#include "le_disasm_core.h"
#include "le_optimizer.h"
#include "le_types.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <functional>

namespace LogicElements {

/* Pack an integer payload into a float bit-storage field losslessly.
 * The runtime reinterprets the same 32 bits via ctype. */
static float le_pack_i32_bits(int32_t v)
{
    union { int32_t i; float f; } u;
    u.i = v;
    return u.f;
}
static float le_pack_u32_bits(uint32_t v)
{
    union { uint32_t u; float f; } x;
    x.u = v;
    return x.f;
}

CompilerCore::CompilerCore()
    : m_user_bool_count(0)
    , m_user_int_count(0)
    , m_user_float_count(0)
    , m_user_complex_count(0)
    , m_peak_temp_bool(0)
    , m_peak_temp_float(0)
    , m_peak_temp_complex(0)
    , m_peak_temp_int(0)
    , m_timer_counter(0)
    , m_counter_counter(0)
{
}

uint16_t CompilerCore::allocate_user_bool()
{
    uint16_t addr = static_cast<uint16_t>(LE_REGION_BOOL_REG | (m_user_bool_count & 0x1FFF));
    m_user_bool_count++;
    return addr;
}

uint16_t CompilerCore::allocate_user_int()
{
    uint16_t addr = static_cast<uint16_t>(LE_REGION_INT_REG | (m_user_int_count & 0x0FFF));
    m_user_int_count++;
    return addr;
}

uint16_t CompilerCore::allocate_user_float()
{
    uint16_t addr = static_cast<uint16_t>(LE_REGION_FLOAT | (m_user_float_count & 0x3FFF));
    m_user_float_count++;
    return addr;
}

uint16_t CompilerCore::allocate_user_complex()
{
    uint16_t addr = static_cast<uint16_t>(LE_REGION_CMPLX | (m_user_complex_count & LE_ADDR_INDEX_MASK));
    m_user_complex_count++;
    return addr;
}
uint16_t CompilerCore::allocate_timer()
{
    uint16_t addr = static_cast<uint16_t>(LE_REGION_TIMER | (m_timer_counter & 0x0FFF));
    m_timer_counter++;
    return addr;
}

uint16_t CompilerCore::allocate_counter()
{
    uint16_t addr = static_cast<uint16_t>(LE_REGION_COUNTER | (m_counter_counter & 0x0FFF));
    m_counter_counter++;
    return addr;
}

uint16_t CompilerCore::acquire_temp_bool(int& out_temp_idx)
{
    if (!m_free_temp_bool_pool.empty()) {
        out_temp_idx = m_free_temp_bool_pool.back();
        m_free_temp_bool_pool.pop_back();
    } else {
        out_temp_idx = m_peak_temp_bool++;
    }
    return static_cast<uint16_t>(LE_REGION_BOOL_REG | ((m_user_bool_count + out_temp_idx) & 0x1FFF));
}

void CompilerCore::release_temp_bool(int temp_idx)
{
    if (temp_idx >= 0 && std::find(m_free_temp_bool_pool.begin(), m_free_temp_bool_pool.end(), temp_idx) == m_free_temp_bool_pool.end()) {
        m_free_temp_bool_pool.push_back(temp_idx);
    }
}

uint16_t CompilerCore::acquire_temp_float(int& out_temp_idx)
{
    if (!m_free_temp_float_pool.empty()) {
        out_temp_idx = m_free_temp_float_pool.back();
        m_free_temp_float_pool.pop_back();
    } else {
        out_temp_idx = m_peak_temp_float++;
    }
    return static_cast<uint16_t>(LE_REGION_FLOAT | ((m_user_float_count + out_temp_idx) & 0x3FFF));
}

void CompilerCore::release_temp_float(int temp_idx)
{
    if (temp_idx >= 0 && std::find(m_free_temp_float_pool.begin(), m_free_temp_float_pool.end(), temp_idx) == m_free_temp_float_pool.end()) {
        m_free_temp_float_pool.push_back(temp_idx);
    }
}

uint16_t CompilerCore::acquire_temp_complex(int& out_temp_idx)
{
    if (!m_free_temp_complex_pool.empty()) {
        out_temp_idx = m_free_temp_complex_pool.back();
        m_free_temp_complex_pool.pop_back();
    } else {
        out_temp_idx = m_peak_temp_complex++;
    }
    return static_cast<uint16_t>(LE_REGION_CMPLX | ((m_user_complex_count + out_temp_idx) & LE_ADDR_INDEX_MASK));
}

void CompilerCore::release_temp_complex(int temp_idx)
{
    if (temp_idx >= 0 && std::find(m_free_temp_complex_pool.begin(), m_free_temp_complex_pool.end(), temp_idx) == m_free_temp_complex_pool.end()) {
        m_free_temp_complex_pool.push_back(temp_idx);
    }
}
uint16_t CompilerCore::acquire_temp_int(int& out_temp_idx)
{
    if (!m_free_temp_int_pool.empty()) {
        out_temp_idx = m_free_temp_int_pool.back();
        m_free_temp_int_pool.pop_back();
    } else {
        out_temp_idx = m_peak_temp_int++;
    }
    return static_cast<uint16_t>(LE_REGION_INT_REG | ((m_user_int_count + out_temp_idx) & 0x0FFF));
}

void CompilerCore::release_temp_int(int temp_idx)
{
    if (temp_idx >= 0 && std::find(m_free_temp_int_pool.begin(), m_free_temp_int_pool.end(), temp_idx) == m_free_temp_int_pool.end()) {
        m_free_temp_int_pool.push_back(temp_idx);
    }
}

uint32_t CompilerCore::compute_crc32(const uint8_t* data, size_t length)
{
    if (!data || length == 0) return 0;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320U;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

std::string CompilerCore::disassemble(const uint8_t* bin_data,
                                     size_t bin_len,
                                     int user_bool_count,
                                     int user_float_count,
                                     int user_int_count)
{
    return disassemble_binary(bin_data, bin_len, user_bool_count, user_float_count, user_int_count);
}

std::string CompilerCore::export_c_header(const uint8_t* bin_data,
                                         size_t bin_len,
                                         const std::string& array_name,
                                         const std::vector<std::string>& custom_headers,
                                         const std::vector<ScalerInfo>& scalers)
{
    std::ostringstream ss;
    ss << "/* Auto-generated by LogicElements Compiler - DO NOT EDIT */\n";
    ss << "#ifndef LE_DEFAULT_PROGRAM_H\n";
    ss << "#define LE_DEFAULT_PROGRAM_H\n\n";

    if (!scalers.empty()) {
        ss << "/*\n * Scaler Definitions (SCL):\n";
        for (const auto& s : scalers) {
            ss << " *   SCL[" << s.index << "]: \"" << s.name << "\" (Channel: AIN[" << s.channel << "])"
               << " -> Raw: [" << s.raw_min << ".." << s.raw_max << "]"
               << " -> Scaled: [" << s.scale_min << ".." << s.scale_max << "] " << s.units
               << " (Clamp: " << (s.clamp ? "True" : "False") << ")\n";
        }
        ss << " */\n\n";
    }

    ss << "#include <stdint.h>\n";
    for (const auto& ch : custom_headers) {
        if (!ch.empty()) {
            if (ch[0] == '<' || ch[0] == '"') {
                ss << "#include " << ch << "\n";
            } else {
                ss << "#include \"" << ch << "\"\n";
            }
        }
    }
    ss << "\n";

    std::string var_name = array_name.empty() ? "le_default_program" : array_name;
    ss << "static const uint8_t " << var_name << "[" << bin_len << "] = {\n";
    for (size_t i = 0; i < bin_len; i += 16) {
        ss << "    ";
        size_t chunk = std::min<size_t>(16, bin_len - i);
        for (size_t j = 0; j < chunk; ++j) {
            ss << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
               << static_cast<int>(bin_data[i + j]);
            if (i + j + 1 < bin_len) ss << ", ";
        }
        ss << "\n";
    }
    ss << "};\n\n";
    ss << "#endif /* LE_DEFAULT_PROGRAM_H */\n";
    return ss.str();
}

static std::string to_upper_str(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static std::string to_lower_str(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static std::string trim_str(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) b++;
    while (e > b && std::isspace((unsigned char)s[e-1])) e--;
    return s.substr(b, e - b);
}

/* Recursive-descent expression parser/evaluator.
 *   expr  := term (('+'|'-') term)*
 *   term  := factor (('*'|'/') factor)*
 *   factor:= unary | call | number | VARNAME | '(' expr ')'
 *   call  := NAME '(' expr (',' expr)? ')'
 * Supported functions: sin cos tan asin acos atan atan2 sqrt cbrt pow exp ln/log
 * log10 log2 abs floor ceil round min max fmod.
 * Variables may be bare (Z_Line) or wrapped in %...% (stripped by the caller). */
double CompilerCore::eval_expr(const std::string& expression, const std::map<std::string,double>& var_values)
{
    const std::string s = trim_str(expression);
    std::size_t pos = 0;

    struct Parser {
        const std::string& s; std::size_t pos; const std::map<std::string,double>& vals;
        std::string ident() {
            std::size_t st = pos;
            while (pos < s.size() && (std::isalnum((unsigned char)s[pos]) || s[pos]=='_' || s[pos]=='.')) pos++;
            return s.substr(st, pos-st);
        }
        double primary() {
            while (pos < s.size() && std::isspace((unsigned char)s[pos])) pos++;
            if (pos >= s.size()) return 0.0;
            char c = s[pos];
            if (c == '(') { pos++; double v = expr(); while (pos<s.size()&&std::isspace((unsigned char)s[pos]))pos++; if (pos<s.size()&&s[pos]==')')pos++; return v; }
            if (std::isdigit((unsigned char)c) || c=='.') {
                std::size_t st = pos;
                while (pos < s.size() && (std::isdigit((unsigned char)s[pos]) || s[pos]=='.')) pos++;
                try { return std::stod(s.substr(st,pos-st)); } catch (...) { return 0.0; }
            }
            std::string n = ident();
            /* function call? */
            std::size_t save = pos;
            while (pos < s.size() && std::isspace((unsigned char)s[pos])) pos++;
            if (pos < s.size() && s[pos]=='(') {
                std::size_t call_pos = pos;
                std::string fn = n;
                for (std::size_t k = 0; k < fn.size(); k++) fn[k] = static_cast<char>(std::tolower((unsigned char)fn[k]));
                pos = call_pos + 1;
                double a = expr();
                /* optional second argument */
                double b = a;
                bool two = false;
                while (pos < s.size() && std::isspace((unsigned char)s[pos])) pos++;
                if (pos < s.size() && s[pos]==',') { pos++; b = expr(); two = true; }
                while (pos < s.size() && std::isspace((unsigned char)s[pos])) pos++;
                if (pos < s.size() && s[pos]==')') pos++;
                if (fn == "sin")  return two ? 0.0 : std::sin(a);
                if (fn == "cos")  return two ? 0.0 : std::cos(a);
                if (fn == "tan")  return two ? 0.0 : std::tan(a);
                if (fn == "asin") return two ? 0.0 : std::asin(a);
                if (fn == "acos") return two ? 0.0 : std::acos(a);
                if (fn == "atan") return two ? std::atan2(a, b) : std::atan(a);
                if (fn == "atan2") return std::atan2(a, b);
                if (fn == "sqrt") return two ? 0.0 : std::sqrt(a);
                if (fn == "cbrt") return two ? 0.0 : std::cbrt(a);
                if (fn == "pow")  return std::pow(a, b);
                if (fn == "exp")  return two ? 0.0 : std::exp(a);
                if (fn == "ln" || fn == "log") return two ? 0.0 : std::log(a);
                if (fn == "log10") return two ? 0.0 : std::log10(a);
                if (fn == "log2")  return two ? 0.0 : (std::log(a) / std::log(2.0));
                if (fn == "abs")  return two ? 0.0 : std::fabs(a);
                if (fn == "floor") return two ? 0.0 : std::floor(a);
                if (fn == "ceil")  return two ? 0.0 : std::ceil(a);
                if (fn == "round") return two ? 0.0 : std::round(a);
                if (fn == "min")  return std::fmin(a, b);
                if (fn == "max")  return std::fmax(a, b);
                if (fn == "fmod") return std::fmod(a, b);
                /* unknown function: fall back to evaluating the parenthesized expr */
                return a;
            }
            pos = save;
            auto it = vals.find(n);
            return (it != vals.end()) ? it->second : 0.0;
        }
        double factor() {
            while (pos < s.size() && std::isspace((unsigned char)s[pos])) pos++;
            if (pos < s.size() && s[pos]=='-') { pos++; return -factor(); }
            if (pos < s.size() && s[pos]=='+') { pos++; return factor(); }
            return primary();
        }
        double term() {
            double v = factor();
            for (;;) {
                while (pos<s.size()&&std::isspace((unsigned char)s[pos]))pos++;
                if (pos>=s.size())break;
                char o=s[pos]; if (o!='*'&&o!='/')break; pos++;
                double r=factor();
                v = (o=='*') ? v*r : ((r!=0.0) ? v/r : 0.0);
            }
            return v;
        }
        double expr() {
            double v = term();
            for (;;) {
                while (pos<s.size()&&std::isspace((unsigned char)s[pos]))pos++;
                if (pos>=s.size())break;
                char o=s[pos]; if (o!='+'&&o!='-')break; pos++;
                double r=term();
                v = (o=='+') ? v+r : v-r;
            }
            return v;
        }
    };
    Parser p{s, 0, var_values};
    return p.expr();
}
/* Resolve a variable into a numeric value (memoized + cycle-detected).
 * Supports direct numbers and indirect definitions ("Z_Line * 1.20") that may
 * reference other %VAR% or bare-name variables. */
static double resolve_var_value(std::string name,
                                std::map<std::string,double>& resolved,
                                std::map<std::string,std::string>& defs,
                                std::map<std::string,int>& state,
                                std::string& err)
{
    /* strip optional %...% wrappers */
    if (name.size() >= 2 && name.front()=='%' && name.back()=='%') name = name.substr(1, name.size()-2);
    int st = state[name];
    if (st == 2) return resolved[name];
    if (st == 1) { if (err.empty()) err = "circular variable reference: " + name; return 0.0; }
    state[name] = 1;

    auto it = defs.find(name);
    double val = 0.0;
    if (it == defs.end()) {
        /* direct numeric variable already in the resolved table */
        auto ri = resolved.find(name);
        if (ri != resolved.end()) { val = ri->second; }
        else {
            /* treat as a literal numeric source */
            try { val = std::stod(name); }
            catch (...) { if (err.empty()) err = "undefined variable reference: " + name; val = 0.0; }
        }
    } else {
        std::string def = trim_str(it->second);
        /* First expand all %VAR% tokens to their resolved numeric literals. */
        std::string inlined;
        for (std::size_t i = 0; i < def.size();) {
            if (def[i]=='%') {
                std::size_t j = def.find('%', i+1);
                if (j != std::string::npos) {
                    std::string inner = def.substr(i+1, j-i-1);
                    double iv = resolve_var_value(inner, resolved, defs, state, err);
                    char buf[48]; std::snprintf(buf, sizeof(buf), "%.10g", iv);
                    inlined += buf; i = j+1; continue;
                }
            }
            inlined += def[i]; i++;
        }
        /* Then inline any bare variable names (Z_Line * 1.20) as numbers, but
         * leave function names (sin( ... ), pow( ... ), ...) for eval_expr. */
        std::string expanded;
        for (std::size_t k = 0; k < inlined.size();) {
            if (std::isalpha((unsigned char)inlined[k]) || inlined[k]=='_') {
                std::size_t ks = k;
                while (k < inlined.size() && (std::isalnum((unsigned char)inlined[k]) || inlined[k]=='_' || inlined[k]=='.')) k++;
                std::string vn = inlined.substr(ks, k-ks);
                /* skip whitespace; if next is '(' it's a math function call, keep it */
                std::size_t t = k;
                while (t < inlined.size() && std::isspace((unsigned char)inlined[t])) t++;
                if (t < inlined.size() && inlined[t]=='(') {
                    /* function call: leave as-is for eval_expr */
                    expanded += vn;
                    continue;
                }
                /* recognized variable: substitute its value */
                if (resolved.find(vn) != resolved.end() || defs.find(vn) != defs.end()) {
                    double vv = resolve_var_value(vn, resolved, defs, state, err);
                    char buf[48]; std::snprintf(buf, sizeof(buf), "%.10g", vv);
                    expanded += buf;
                } else {
                    /* unknown bare word: still a variable reference -> error */
                    if (err.empty()) err = "undefined variable reference: " + vn;
                    expanded += vn;
                }
            } else { expanded += inlined[k]; k++; }
        }
        val = CompilerCore::eval_expr(expanded, resolved);
    }

    state[name] = 2;
    resolved[name] = val;
    return val;
}
/* Numeric property read that supports direct numbers, "%VAR%", or expressions. */
double CompilerCore::prop(const JsonValue& element, const std::string& key, double default_value)
{
    if (!element.is_object() || !element.contains(key)) return default_value;
    const JsonValue& v = element.get(key);
    if (v.is_number()) return v.as_double(default_value);
    if (v.is_string()) {
        std::string s = trim_str(v.as_string());
        if (s.empty()) return default_value;
        /* single %VAR% -> resolve directly */
        if (s.size() >= 2 && s.front()=='%' && s.back()=='%')
            return resolve_var_value(s, m_var_values, m_var_exprs, m_var_state, m_var_error);
        /* embedded %VAR% tokens mixed with arithmetic */
        if (s.find('%') != std::string::npos) {
            std::string inlined;
            for (std::size_t i = 0; i < s.size();) {
                if (s[i]=='%') {
                    std::size_t j = s.find('%', i+1);
                    if (j != std::string::npos) {
                        std::string inner = s.substr(i+1, j-i-1);
                        double iv = resolve_var_value("%"+inner+"%", m_var_values, m_var_exprs, m_var_state, m_var_error);
                        char buf[48]; std::snprintf(buf, sizeof(buf), "%.10g", iv);
                        inlined += buf; i = j+1; continue;
                    }
                }
                inlined += s[i]; i++;
            }
            return eval_expr(inlined, m_var_values);
        }
        /* plain arithmetic over literals / bare variable names */
        return eval_expr(s, m_var_values);
    }
    return default_value;
}

/* Populate the variable tables from the circuit's "variables" object and resolve
 * each entry topologically. Returns 0 on success or -1 if a variable error occurs
 * (m_var_error is set; out_result->error_message is filled by the caller). */
int CompilerCore::build_variables(const JsonValue& circuit_doc)
{
    m_var_values.clear(); m_var_exprs.clear(); m_var_state.clear(); m_var_error.clear();
    if (!circuit_doc.contains("variables") || !circuit_doc.get("variables").is_object())
        return 0;
    const auto& vars = circuit_doc.get("variables").as_object();
    for (const auto& kv : vars) {
        if (kv.second.is_number()) {
            /* direct scalar: value only (no expression stored) */
            m_var_values[kv.first] = kv.second.as_double(0.0);
        } else if (kv.second.is_string()) {
            m_var_exprs[kv.first] = kv.second.as_string();
        }
    }
    /* Resolve every variable once (memoized); direct ones return immediately. */
    for (const auto& kv : vars) {
        resolve_var_value(kv.first, m_var_values, m_var_exprs, m_var_state, m_var_error);
        if (!m_var_error.empty()) break;
    }
    /* mark any remaining direct numerics done */
    for (const auto& kv : vars) {
        if (kv.second.is_number() && m_var_state[kv.first] == 0) {
            m_var_state[kv.first] = 2;
        }
    }
    return m_var_error.empty() ? 0 : -1;
}



/**
 * @brief Parses a register address string into a process-image address.
 *
 * Accepts an optional leading '%' and either a bare decimal index or a
 * bracketed index ("%B[4]"). New mnemonics are supported: %IN %OUT %AIN %B %I
 * %F %C (int register = %I; digital input = %IN). Legacy forms are accepted for
 * back-compat: %M (bool), %R (float), %Q (output), %N (int), plus the
 * DIN/DOUT/BOOL/FLOAT/INT/CMPLX spellings.
 *
 * @param in  The register mnemonic string to parse.
 * @param out On success, receives the encoded 16-bit process-image address.
 * @return True if @p in denotes a known register family + valid index, false
 *         otherwise (unknown mnemonic, missing/invalid index, or the not-yet
 *         allocated analog-output region).
 */
static bool parse_register_addr(const std::string& in, uint16_t* out)
{
    std::string s = in;
    size_t i = 0;
    while (i < s.size() && s[i] == '%') i++;
    size_t j = i;
    while (j < s.size() && ((s[j] >= 'A' && s[j] <= 'Z') || (s[j] >= 'a' && s[j] <= 'z'))) j++;
    std::string tok = to_upper_str(s.substr(i, j - i));
    std::string rest = s.substr(j);

    auto all_digits = [](const std::string& t) -> bool {
        if (t.empty()) return false;
        for (char c : t) if (c < '0' || c > '9') return false;
        return true;
    };

    int idx = -1;
    if (!rest.empty() && rest[0] == '[') {
        size_t close = rest.find(']');
        if (close == std::string::npos) return false;
        std::string num = rest.substr(1, close - 1);
        if (all_digits(num)) idx = std::stoi(num);
    } else if (all_digits(rest)) {
        idx = std::stoi(rest);
    }
    if (idx < 0 || idx > 0x3FFF) return false;

    uint16_t region;
    if      (tok == "IN"   || tok == "DIN")   region = LE_REGION_DIN;
    else if (tok == "OUT"  || tok == "DOUT")  region = LE_REGION_DOUT;
    else if (tok == "B"    || tok == "BOOL")  region = LE_REGION_BOOL_REG;
    else if (tok == "F"    || tok == "FLOAT") region = LE_REGION_FLOAT;
    else if (tok == "I"    || tok == "INT")   region = LE_REGION_INT_REG;
    else if (tok == "C"    || tok == "CMPLX") region = LE_REGION_CMPLX;
    else if (tok == "AIN")                    region = LE_REGION_AIN;
    else if (tok == "AOUT")  return false;   /* analog-output region not yet allocated */
    else if (tok == "M")     region = LE_REGION_BOOL_REG;  /* legacy coils */
    else if (tok == "R")     region = LE_REGION_FLOAT;     /* legacy floats */
    else if (tok == "Q")     region = LE_REGION_DOUT;      /* legacy outputs */
    else if (tok == "N")     region = LE_REGION_INT_REG;   /* legacy ints */
    else return false;

    uint16_t mask = (region == LE_REGION_BOOL_REG) ? 0x1FFFU :
                   ((region == LE_REGION_FLOAT) ? 0x3FFFU : LE_ADDR_INDEX_MASK);
    *out = (uint16_t)((uint16_t)region | ((uint16_t)idx & mask));
    return true;
}

int CompilerCore::compile(const std::string& circuit_json_str,
                          const std::string& board_json_str,
                          le_compile_result_t* out_result)
{
    return compile_ex(circuit_json_str, board_json_str, le_compiler_options_init_default(), out_result);
}

int CompilerCore::compile_ex(const std::string& circuit_json_str,
                             const std::string& board_json_str,
                             const le_compiler_options_t& options,
                             le_compile_result_t* out_result)
{
    if (!out_result) return -1;
    std::memset(out_result, 0, sizeof(le_compile_result_t));

    // 1. Parse Circuit JSON
    JsonValue circuit_doc;
    try {
        circuit_doc = JsonValue::parse(circuit_json_str);
    } catch (const std::exception& ex) {
        std::string err = std::string("JSON Parse Error: ") + ex.what();
        out_result->success = 0;
        out_result->error_message = (char*)std::malloc(err.size() + 1);
        std::strcpy(out_result->error_message, err.c_str());
        return -1;
    }

    if (!circuit_doc.is_object()) {
        out_result->success = 0;
        const char* msg = "Root JSON must be an object containing 'elements' and 'nets'.";
        out_result->error_message = (char*)std::malloc(std::strlen(msg) + 1);
        std::strcpy(out_result->error_message, msg);
        return -1;
    }


    // 1b. Build & resolve the circuit's "variables" table (properties may use %VAR%).
    if (build_variables(circuit_doc) != 0) {
        out_result->success = 0;
        std::string err = std::string("Variable Error: ") + m_var_error;
        out_result->error_message = (char*)std::malloc(err.size() + 1);
        std::strcpy(out_result->error_message, err.c_str());
        return -1;
    }
    // 2. Parse Board Profile JSON if provided
    JsonValue board_doc;
    bool has_board = false;
    if (!board_json_str.empty()) {
        try {
            board_doc = JsonValue::parse(board_json_str);
            has_board = board_doc.is_object();
        } catch (...) {
            // Ignore optional board parsing error or note warning
        }
    }

    // Extract board pin mappings & aliases
    std::map<std::string, std::string> alias_to_addr;
    std::map<std::string, CustomNodeInfo> custom_nodes;
    std::vector<std::string> custom_headers;

    if (has_board) {
        if (board_doc.contains("pin_map") && board_doc.get("pin_map").is_object()) {
            const auto& pm = board_doc.get("pin_map").as_object();
            if (pm.find("inputs") != pm.end() && pm.at("inputs").is_object()) {
                for (const auto& kv : pm.at("inputs").as_object()) {
                    if (kv.second.is_object() && kv.second.contains("alias")) {
                        std::string al = kv.second.get("alias").as_string();
                        if (!al.empty()) alias_to_addr[al] = kv.first;
                    }
                }
            }
            if (pm.find("outputs") != pm.end() && pm.at("outputs").is_object()) {
                for (const auto& kv : pm.at("outputs").as_object()) {
                    if (kv.second.is_object() && kv.second.contains("alias")) {
                        std::string al = kv.second.get("alias").as_string();
                        if (!al.empty()) alias_to_addr[al] = kv.first;
                    }
                }
            }
        }

        if (board_doc.contains("custom_nodes") && board_doc.get("custom_nodes").is_array()) {
            for (const auto& cn_val : board_doc.get("custom_nodes").as_array()) {
                if (!cn_val.is_object()) continue;
                CustomNodeInfo info;
                info.type_id = cn_val.get("type_id").as_string(cn_val.get("type").as_string(cn_val.get("name").as_string()));
                info.display_name = cn_val.get("display_name").as_string(info.type_id);
                info.category = cn_val.get("category").as_string("Custom");
                info.description = cn_val.get("description").as_string();
                info.function_id = static_cast<uint8_t>(cn_val.get("function_id").as_int(1));
                info.c_header = cn_val.get("c_header").as_string(cn_val.get("header").as_string());
                if (!info.c_header.empty()) {
                    if (std::find(custom_headers.begin(), custom_headers.end(), info.c_header) == custom_headers.end()) {
                        custom_headers.push_back(info.c_header);
                    }
                }

                if (cn_val.contains("inputs") && cn_val.get("inputs").is_array()) {
                    for (const auto& p : cn_val.get("inputs").as_array()) {
                        CustomPinInfo pi;
                        pi.name = p.get("name").as_string();
                        pi.type = p.get("type").as_string("bool");
                        info.inputs.push_back(pi);
                    }
                }
                if (cn_val.contains("outputs") && cn_val.get("outputs").is_array()) {
                    for (const auto& p : cn_val.get("outputs").as_array()) {
                        CustomPinInfo pi;
                        pi.name = p.get("name").as_string();
                        pi.type = p.get("type").as_string("bool");
                        info.outputs.push_back(pi);
                    }
                }

                if (!info.type_id.empty()) {
                    custom_nodes[info.type_id] = info;
                }
            }
        }
    }

    const auto& elements = circuit_doc.get("elements").as_array();
    const auto& nets = circuit_doc.get("nets").as_array();

    // Reset allocators
    m_user_bool_count = 0;
    m_user_int_count = 0;
    m_user_float_count = 0;
    m_user_complex_count = 0;
    m_peak_temp_bool = 0;
    m_peak_temp_float = 0;
    m_peak_temp_complex = 0;
    m_peak_temp_int = 0;
    m_timer_counter = 0;
    m_counter_counter = 0;
    m_free_temp_bool_pool.clear();
    m_free_temp_float_pool.clear();
    m_free_temp_complex_pool.clear();
    m_free_temp_int_pool.clear();

    std::map<std::string, uint16_t> din_map;
    std::map<std::string, uint16_t> dout_map;
    std::map<uint16_t, std::vector<std::string>> output_drivers;
    std::map<std::string, uint16_t> ain_map;
    std::map<std::string, uint16_t> element_outputs;   /* element name -> canonical output address */
    std::map<std::string, std::map<std::string, uint16_t>> element_outputs_port; /* element -> {output port -> address} */
    std::map<std::string, std::map<std::string, std::string>> net_source_port;   /* consumer -> {consumer port -> source output port} */
    std::vector<ScalerInfo> scaler_list;
    int auto_din_idx = 0;
    int auto_dout_idx = 0;
    int scaler_counter = 0;
    bool uses_protection = false;
    bool uses_serial_bus = false;
    bool uses_dsp = false;
    int lpf_counter = 0;
    int biquad_counter = 0;
    int moving_avg_counter = 0;
    int rate_limiter_counter = 0;
    int deadband_counter = 0;
    int washout_counter = 0;
    int peak_counter = 0;
    int rms_counter = 0;
    int median_counter = 0;
    int derivative_counter = 0;
    int zero_crossing_counter = 0;
    int lut_1d_counter = 0;
    int totalizer_counter = 0;
    int min_max_hold_counter = 0;

    // Step 1: Discover inputs, outputs, user registers, constants
    for (const auto& el : elements) {
        if (!el.is_object()) continue;
        std::string name = el.get("name").as_string();
        std::string type = to_upper_str(el.get("type").as_string());
        std::string explicit_addr = el.get("address").as_string();

        if (type == "DIGITALINPUT" || type == "INPUT" || (type == "LE_NODE_DIGITAL" && (name.rfind("IN", 0) == 0 || name.rfind("DI", 0) == 0 || alias_to_addr.find(name) != alias_to_addr.end()))) {
            if (explicit_addr.empty() && alias_to_addr.find(name) != alias_to_addr.end()) {
                explicit_addr = alias_to_addr[name];
            }
            while (!explicit_addr.empty() && explicit_addr[0] == '%') explicit_addr.erase(0, 1);

            uint16_t addr;
            if (!explicit_addr.empty() && (explicit_addr[0] == 'I' || explicit_addr[0] == 'i') && explicit_addr.length() > 1 && std::isdigit(explicit_addr[1])) {
                int idx = std::stoi(explicit_addr.substr(1));
                addr = static_cast<uint16_t>(LE_REGION_DIN | (idx & 0x0FFF));
            } else if (explicit_addr.rfind("DIN", 0) == 0) {
                int idx = std::stoi(explicit_addr.substr(3));
                addr = static_cast<uint16_t>(LE_REGION_DIN | (idx & 0x0FFF));
            } else {
                addr = static_cast<uint16_t>(LE_REGION_DIN | (auto_din_idx++ & 0x0FFF));
            }
            din_map[name] = addr;
            element_outputs[name] = addr;
        } else if (type == "DIGITALOUTPUT" || type == "OUTPUT" || (type == "LE_NODE_DIGITAL" && (name.rfind("OUT", 0) == 0 || name.rfind("DO", 0) == 0))) {
            if (explicit_addr.empty() && alias_to_addr.find(name) != alias_to_addr.end()) {
                explicit_addr = alias_to_addr[name];
            }
            while (!explicit_addr.empty() && explicit_addr[0] == '%') explicit_addr.erase(0, 1);

            uint16_t addr;
            if (!explicit_addr.empty() && (explicit_addr[0] == 'Q' || explicit_addr[0] == 'q') && explicit_addr.length() > 1 && std::isdigit(explicit_addr[1])) {
                int idx = std::stoi(explicit_addr.substr(1));
                addr = static_cast<uint16_t>(LE_REGION_DOUT | (idx & 0x0FFF));
            } else if (explicit_addr.rfind("DOUT", 0) == 0) {
                int idx = std::stoi(explicit_addr.substr(4));
                addr = static_cast<uint16_t>(LE_REGION_DOUT | (idx & 0x0FFF));
            } else {
                addr = static_cast<uint16_t>(LE_REGION_DOUT | (auto_dout_idx++ & 0x0FFF));
            }
            dout_map[name] = addr;
            element_outputs[name] = addr;
            output_drivers[addr].push_back(name);
        } else if (type == "BOOLREGISTER" || type == "LE_BOOLREGISTER" || (type == "LE_NODE_DIGITAL" && din_map.find(name) == din_map.end() && dout_map.find(name) == dout_map.end())) {
            element_outputs[name] = allocate_user_bool();
        } else if (type == "INTREGISTER" || type == "LE_INTREGISTER") {
            element_outputs[name] = allocate_user_int();
        } else if (type == "FLOATREGISTER" || type == "LE_FLOATREGISTER") {
            element_outputs[name] = allocate_user_float();
        } else if (type == "COMPLEXREGISTER" || type == "LE_COMPLEXREGISTER") {
            element_outputs[name] = allocate_user_complex();
        } else if (type == "ANALOGINPUT" || type == "LE_ANALOG_INPUT" || type == "LE_ANALOGINPUT") {
            int ch = (int)prop(el, "channel", 0);
            uint16_t addr = static_cast<uint16_t>(LE_REGION_AIN | (ch & 0x0FFF));
            ain_map[name] = addr;
            std::string m = to_lower_str(el.get("mode").as_string());
            if (m != "float" && m != "scaled") {
                element_outputs[name] = addr;
            }
        } else if (type == "CONSTANT" || type == "LE_CONSTANT") {
            std::string dt = to_lower_str(el.get("dataType").as_string(el.get("data_type").as_string("bool")));
            if (dt == "float") {
                float f = prop(el, "value", 0.0f);
                element_outputs[name] = (std::abs(f - 1.0f) < 0.0001f) ? LE_CONST_ONE_F : LE_CONST_ZERO_F;
            } else if (dt == "int" || dt == "integer") {
                int i = (int)prop(el, "value", 0);
                element_outputs[name] = (i != 0) ? LE_CONST_TRUE : LE_CONST_FALSE;
            } else {
                bool b = el.get("value").as_bool(false);
                element_outputs[name] = b ? LE_CONST_TRUE : LE_CONST_FALSE;
            }
        }
    }

    // Check for duplicate drivers on same DOUT
    for (const auto& kv : output_drivers) {
        if (kv.second.size() > 1) {
            std::ostringstream ss;
            ss << "Duplicate Output Error: Multiple output blocks drive the same output channel %Q"
               << (kv.first & 0x0FFF) << ".";
            std::string err = ss.str();
            out_result->success = 0;
            out_result->error_message = (char*)std::malloc(err.size() + 1);
            std::strcpy(out_result->error_message, err.c_str());
            return -1;
        }
    }

    // Step 1b: Parse + resolve the circuit's "aliases" object.
    // Each alias maps a short name (<= LE_ALIAS_NAME_MAX chars) to a register
    // address. The target may be a register mnemonic ("%B0", "%F2", "%OUT1"),
    // an element name (e.g. a BOOLREGISTER/FLOATREGISTER output), or a board
    // pin alias supplied by the board profile. Only explicitly declared aliases
    // (this object) are embedded; board aliases are never auto-baked, so adding
    // a board profile never grows the .lebin.
    std::vector<le_alias_t> alias_list;
    if (circuit_doc.contains("aliases") && circuit_doc.get("aliases").is_object()) {
        const auto& als = circuit_doc.get("aliases").as_object();
        for (const auto& kv : als) {
            std::string nm = kv.first;
            std::string target = kv.second.is_string() ? kv.second.as_string() : "";
            std::string err;
            if (nm.empty() || nm.size() > (size_t)LE_ALIAS_NAME_MAX) {
                err = "Alias Error: alias name '" + nm + "' must be 1.." +
                      std::to_string(LE_ALIAS_NAME_MAX) + " characters.";
            } else if (target.empty()) {
                err = "Alias Error: alias '" + nm + "' has no target register.";
            } else {
                uint16_t addr = 0;
                bool ok = false;
                if (element_outputs.find(target) != element_outputs.end()) {
                    addr = element_outputs[target];
                    ok = true;
                } else {
                    std::string resolved = target;
                    auto bit = alias_to_addr.find(target);
                    if (bit != alias_to_addr.end()) resolved = bit->second;
                    ok = parse_register_addr(resolved, &addr);
                }
                if (!ok) {
                    err = "Alias Error: alias '" + nm + "' target '" + target +
                          "' is not a valid register or element.";
                } else {
                    le_alias_t a;
                    std::memset(&a, 0, sizeof(a));
                    std::strncpy(a.name, nm.c_str(), LE_ALIAS_NAME_MAX);
                    a.kind = 0; a.pad = 0;
                    a.addr = addr;
                    alias_list.push_back(a);
                }
            }
            if (!err.empty()) {
                out_result->success = 0;
                out_result->error_message = (char*)std::malloc(err.size() + 1);
                std::strcpy(out_result->error_message, err.c_str());
                return -1;
            }
        }
    }

    // Step 2: Map connections (target_elem -> { port_name -> source_elem_name })
    std::map<std::string, std::map<std::string, std::string>> net_connections;
    for (const auto& net : nets) {
        if (!net.is_object()) continue;
        std::string out_elem = net.get("output").get("name").as_string();
        std::string out_port = to_lower_str(net.get("output").get("port").as_string("out"));
        for (const auto& inp : net.get("inputs").as_array()) {
            if (!inp.is_object()) continue;
            std::string target_elem = inp.get("name").as_string();
            std::string target_port = to_lower_str(inp.get("port").as_string("input_0"));
            net_connections[target_elem][target_port] = out_elem;
            net_source_port[target_elem][target_port] = out_port;
        }
    }

    // Map TAG senders and receivers
    std::map<std::string, std::string> tag_source_node;
    for (const auto& el : elements) {
        if (!el.is_object()) continue;
        std::string type = to_upper_str(el.get("type").as_string());
        if (type.find("TAG") != std::string::npos) {
            std::string dir = to_lower_str(el.get("direction").as_string());
            if (dir == "send") {
                std::string name = el.get("name").as_string();
                std::string tag_name = el.get("tag_name").as_string(name);
                if (net_connections.find(name) != net_connections.end()) {
                    const auto& ports = net_connections[name];
                    std::string src;
                    auto it_in = ports.find("in");
                    if (it_in != ports.end()) src = it_in->second;
                    else {
                        auto it_a = ports.find("a");
                        if (it_a != ports.end()) src = it_a->second;
                        else if (!ports.empty()) src = ports.begin()->second;
                    }
                    if (!src.empty()) tag_source_node[tag_name] = src;
                }
            }
        }
    }

    std::map<std::string, std::string> tag_effective_source;
    std::vector<std::string> warning_list;

    
    for (const auto& el : elements) {
        if (!el.is_object()) continue;
        std::string type = to_upper_str(el.get("type").as_string());
        if (type.find("TAG") != std::string::npos) {
            std::string dir = to_lower_str(el.get("direction").as_string());
            if (dir != "send") {
                std::string name = el.get("name").as_string();
                std::string tag_name = el.get("tag_name").as_string(name);
                if (tag_source_node.find(tag_name) != tag_source_node.end()) {
                    net_connections[name]["in"] = tag_source_node[tag_name];
                    tag_effective_source[name] = tag_source_node[tag_name];
                } else {
                    warning_list.push_back("Tag '" + tag_name + "' has no matching Sender.");
                }
            }
        }
    }

    // Step 3: Run Compiler Optimizer (Passes: DCE, Constant Folding, Inversion Folding, CSE, Direct Destination)
    Optimizer optimizer(options);
    optimizer.build_ir(elements, nets, alias_to_addr, custom_nodes);
    OptimizationStats stats = optimizer.run_passes();

    const auto& opt_nodes = optimizer.get_nodes();
    const auto& eval_order = optimizer.get_optimized_order();

    // Populate folded constants into element_outputs
    for (const auto& kv : opt_nodes) {
        if (kv.second.is_const) {
            if (kv.second.const_type == "bool") {
                element_outputs[kv.first] = kv.second.const_bool ? LE_CONST_TRUE : LE_CONST_FALSE;
            } else if (kv.second.const_type == "float") {
                element_outputs[kv.first] = (std::abs(kv.second.const_float - 1.0f) < 0.0001f) ? LE_CONST_ONE_F : LE_CONST_ZERO_F;
            } else {
                element_outputs[kv.first] = (kv.second.const_int != 0) ? LE_CONST_TRUE : LE_CONST_FALSE;
            }
        }
    }

    // Step 3.5: Liveness Analysis & Consumer Reference Counting
    enum class TempType { None, Bool, Float, Int, Complex };
    std::map<std::string, int> node_temp_idx;
    std::map<std::string, TempType> node_temp_type;
    std::map<std::string, int> ref_counts;

    for (const auto& kv : opt_nodes) {
        const auto& opt_node = kv.second;
        if (opt_node.is_dead || opt_node.direct_dest_eliminated) continue;
        if (opt_node.type == "DIGITALINPUT" || opt_node.type == "INPUT") continue;
        if (opt_node.type.find("TAG") != std::string::npos) {
            std::string dir = to_lower_str(opt_node.raw_json.get("direction").as_string());
            if (dir == "send") continue;
        }

        for (const auto& port_src : opt_node.inputs) {
            if (!port_src.second.empty()) {
                std::string actual_src = port_src.second;
                auto it_eff = tag_effective_source.find(actual_src);
                if (it_eff != tag_effective_source.end()) actual_src = it_eff->second;
                ref_counts[actual_src]++;
            }
        }
    }

    auto consume_input = [&](const std::string& src) {
        if (src.empty()) return;
        std::string actual_src = src;
        auto it_eff = tag_effective_source.find(actual_src);
        if (it_eff != tag_effective_source.end()) actual_src = it_eff->second;

        auto it_ref = ref_counts.find(actual_src);
        if (it_ref != ref_counts.end()) {
            it_ref->second--;
            if (it_ref->second <= 0) {
                auto it_tt = node_temp_type.find(actual_src);
                auto it_ti = node_temp_idx.find(actual_src);
                if (it_ti != node_temp_idx.end()) {
                    if (it_tt != node_temp_type.end()) {
                        if (it_tt->second == TempType::Bool) release_temp_bool(it_ti->second);
                        else if (it_tt->second == TempType::Float) release_temp_float(it_ti->second);
                        else if (it_tt->second == TempType::Complex) release_temp_complex(it_ti->second);
                        else if (it_tt->second == TempType::Int) release_temp_int(it_ti->second);
                    }
                    node_temp_idx.erase(it_ti);
                    if (it_tt != node_temp_type.end()) node_temp_type.erase(it_tt);
                }
            }
        }
    };

    // Step 4: Instruction Generation
    std::vector<le_instruction_t> instructions;

    /* Block Call Table: one record per LE_OP_BLOCK instruction. The record's
     * args array holds all live input addresses followed by output addresses,
     * matching le_block_desc_t's on-disk layout. */
    struct BlockCall {
        uint8_t in_count;
        uint8_t out_count;
        std::vector<uint16_t> args;
    };
    std::vector<BlockCall> block_calls;

    /* Per-kind count of stateful blocks (timers, counters, DSP filters, ...).
     * Emitted as a compact state-directive table so the loader can compute each
     * kind group's byte offset within the state image. */
    std::map<uint8_t, int> state_descs;

    /* The preconfigured state image: one materialized state struct (defaults +
     * all circuit properties baked as concrete bytes) per stateful instance,
     * grouped by kind. The loader memcpy's this image into RAM at load. */
    std::map<uint8_t, std::vector<std::vector<uint8_t> > > state_instances;
    auto append_state = [&](uint8_t kind, const void* p, size_t n) {
        const uint8_t* b = (const uint8_t*)p;
        state_instances[kind].push_back(std::vector<uint8_t>(b, b + n));
    };

    auto push_block = [&](uint8_t func_id, std::vector<uint16_t> args,
                          uint8_t in_count, uint8_t out_count) {
        BlockCall bc;
        bc.in_count = in_count;
        bc.out_count = out_count;
        bc.args = args;
        le_instruction_t inst;
        inst.opcode = LE_OP_BLOCK;
        inst.modifier = func_id;
        inst.in_a = static_cast<uint16_t>(block_calls.size());
        inst.in_b = LE_ADDR_UNUSED;
        inst.out = LE_ADDR_UNUSED;
        instructions.push_back(inst);
        block_calls.push_back(bc);
    };

    /* Resolve a named source element into its process image address (or a
     * constant default when the port is unconnected). */
    auto block_src_addr = [&](const std::string& src, uint16_t default_addr) -> uint16_t {
        if (src.empty()) return default_addr;
        auto it = element_outputs.find(src);
        if (it != element_outputs.end()) return it->second;
        return default_addr;
    };

    /* source output port observed by a given consumer input port */
    auto source_port_of = [&](const std::string& src_elem, const std::string& consumer_port) -> std::string {
        auto m = net_source_port.find(src_elem);
        if (m != net_source_port.end()) {
            auto p = m->second.find(consumer_port);
            if (p != m->second.end()) return p->second;
        }
        return "";
    };

    /* Resolve a source (element, output-port) into an address, falling back to the
     * element's canonical/only output. Enables multi-output selection. */
    auto src_address = [&](const std::string& src, const std::string& sport, uint16_t default_addr) -> uint16_t {
        if (src.empty()) return default_addr;
        if (!sport.empty()) {
            auto e = element_outputs_port.find(src);
            if (e != element_outputs_port.end()) {
                auto p = e->second.find(sport);
                if (p != e->second.end()) return p->second;
            }
        }
        auto it = element_outputs.find(src);
        return (it != element_outputs.end()) ? it->second : default_addr;
    };

    for (const std::string& name : eval_order) {
        auto it_opt = opt_nodes.find(name);
        if (it_opt == opt_nodes.end()) continue;
        const auto& opt_node = it_opt->second;

        // Skip dead nodes (DCE, constant folded, folded NOTs, CSE duplicates)
        // and direct destination eliminated outputs/registers
        if (opt_node.is_dead || opt_node.direct_dest_eliminated) {
            continue;
        }

        const auto& el = opt_node.raw_json;
        std::string type = opt_node.type;

        // Skip input nodes
        if (type == "INPUT" || type == "DIGITALINPUT" || (type == "LE_NODE_DIGITAL" && din_map.find(name) != din_map.end())) {
            continue;
        }

        // Scaled Analog Inputs
        if (type == "ANALOGINPUT" || type == "LE_ANALOG_INPUT" || type == "LE_ANALOGINPUT") {
            std::string m = to_lower_str(el.get("mode").as_string());
            if (m == "float" || m == "scaled") {
                int ch = (int)prop(el, "channel", 0);
                int s_idx = scaler_counter++;
                state_descs[LE_BLK_SCALER]++;
                if (s_idx >= 16) s_idx = 15;

                ScalerInfo sm;
                sm.index = s_idx;
                sm.name = name;
                sm.channel = ch;
                sm.raw_min = prop(el, "raw_min", 0.0f);
                sm.raw_max = prop(el, "raw_max", 4095.0f);
                if (sm.raw_max == 0.0f && sm.raw_min == 0.0f) sm.raw_max = 4095.0f;
                sm.scale_min = prop(el, "scale_min", 0.0f);
                sm.scale_max = prop(el, "scale_max", 100.0f);
                sm.units = el.get("units").as_string("%");
                sm.clamp = el.get("clamp").as_bool(true);
                scaler_list.push_back(sm);
                le_scale_state_t sc{};
                sc.raw_min = sm.raw_min; sc.raw_max = sm.raw_max;
                sc.scale_min = sm.scale_min; sc.scale_max = sm.scale_max;
                sc.clamp = sm.clamp;
                append_state(LE_BLK_SCALER, &sc, sizeof(sc));

                int temp_idx = -1;
                uint16_t out_flt = acquire_temp_float(temp_idx);
                element_outputs[name] = out_flt;
                node_temp_idx[name] = temp_idx;
                node_temp_type[name] = TempType::Float;

                le_instruction_t inst;
                inst.opcode = LE_OP_SCALE_F;
                inst.modifier = static_cast<uint8_t>(s_idx);
                inst.in_a = static_cast<uint16_t>(LE_REGION_AIN | (ch & 0x0FFF));
                inst.in_b = LE_ADDR_UNUSED;
                inst.out = out_flt;
                instructions.push_back(inst);

                if (ref_counts[name] <= 0) {
                    release_temp_float(temp_idx);
                    node_temp_idx.erase(name);
                    node_temp_type.erase(name);
                }
            }
            continue;
        }

        // Tags
        if (type.find("TAG") != std::string::npos) {
            std::string dir = to_lower_str(el.get("direction").as_string());
            if (dir == "send") {
                continue;
            } else {
                std::string tag_name = el.get("tag_name").as_string(name);
                uint16_t src_addr = LE_CONST_FALSE;
                auto it_src = tag_source_node.find(tag_name);
                if (it_src != tag_source_node.end()) {
                    auto it_out = element_outputs.find(it_src->second);
                    if (it_out != element_outputs.end()) src_addr = it_out->second;
                }
                element_outputs[name] = src_addr;
                continue;
            }
        }

        // Output nodes: emit MOVE
        if (type == "DIGITALOUTPUT" || type == "OUTPUT" || (type == "LE_NODE_DIGITAL" && dout_map.find(name) != dout_map.end())) {
            uint16_t src_addr = LE_CONST_FALSE;
            std::string src_name;
            if (!opt_node.inputs.empty()) {
                std::string consumer_port = opt_node.inputs.begin()->first;
                src_name = opt_node.inputs.begin()->second;
                src_addr = src_address(src_name, source_port_of(name, consumer_port), LE_CONST_FALSE);
            }
            uint16_t dst_addr = dout_map[name];
            le_instruction_t inst;
            inst.opcode = LE_OP_MOVE;
            inst.modifier = 0;
            inst.in_a = src_addr;
            inst.in_b = LE_ADDR_UNUSED;
            inst.out = dst_addr;
            instructions.push_back(inst);
            if (!src_name.empty()) consume_input(src_name);
            continue;
        }

        // User registers: emit MOVE
        if (type == "BOOLREGISTER" || type == "LE_BOOLREGISTER" || type == "INTREGISTER" ||
            type == "LE_INTREGISTER" || type == "FLOATREGISTER" || type == "LE_FLOATREGISTER" ||
            type == "COMPLEXREGISTER" || type == "LE_COMPLEXREGISTER") {
            if (!opt_node.inputs.empty()) {
                std::string consumer_port = opt_node.inputs.begin()->first;
                std::string in_src = opt_node.inputs.begin()->second;
                uint16_t src_addr = src_address(in_src, source_port_of(name, consumer_port), LE_ADDR_UNUSED);
                if (src_addr != LE_ADDR_UNUSED) {
                    uint16_t reg_addr = element_outputs[name];
                    le_instruction_t inst;
                    if (type.find("COMPLEX") != std::string::npos) inst.opcode = LE_OP_MOVE_C;
                    else inst.opcode = (type.find("FLOAT") != std::string::npos) ? LE_OP_MOVE_F : LE_OP_MOVE;
                    inst.modifier = 0;
                    inst.in_a = src_addr;
                    inst.in_b = LE_ADDR_UNUSED;
                    inst.out = reg_addr;
                    instructions.push_back(inst);
                    consume_input(in_src);
                }
            }
            continue;
        }

        // Standard opcodes
        uint8_t opcode = LE_OP_NOP;
        bool is_float_op = false;
        bool is_complex_op = false;
        bool is_int_op = false;
        bool is_timer_op = false;
        bool is_counter_op = false;

        if (type == "AND" || type == "LE_AND") opcode = LE_OP_AND;
        else if (type == "OR" || type == "LE_OR") opcode = LE_OP_OR;
        else if (type == "NOT" || type == "LE_NOT") opcode = LE_OP_NOT;
        else if (type == "XOR" || type == "LE_XOR") opcode = LE_OP_XOR;
        else if (type == "NAND" || type == "LE_NAND") opcode = LE_OP_NAND;
        else if (type == "NOR" || type == "LE_NOR") opcode = LE_OP_NOR;
        else if (type == "MUX" || type == "LE_MUX") opcode = LE_OP_MUX;
        else if (type == "RTRIG" || type == "LE_RTRIG") opcode = LE_OP_RTRIG;
        else if (type == "FTRIG" || type == "LE_FTRIG") opcode = LE_OP_FTRIG;
        else if (type == "LATCH" || type == "LE_LATCH") {
            std::string dom = to_lower_str(el.get("dominant").as_string("reset"));
            if (dom.find("reset") != std::string::npos) {
                opcode = LE_OP_RS;
            } else if (dom.find("set") != std::string::npos) {
                opcode = LE_OP_SR;
            } else {
                opcode = LE_OP_RS;
            }
        }
        else if (type == "SR" || type == "LE_SR") opcode = LE_OP_SR;
        else if (type == "RS" || type == "LE_RS") opcode = LE_OP_RS;
        else if (type == "TON" || type == "LE_TON") { opcode = LE_OP_TON; is_timer_op = true; }
        else if (type == "TOF" || type == "LE_TOF") { opcode = LE_OP_TOF; is_timer_op = true; }
        else if (type == "TP" || type == "LE_TP") { opcode = LE_OP_TP; is_timer_op = true; }
        else if (type == "CTU" || type == "LE_CTU") { opcode = LE_OP_CTU; is_counter_op = true; }
        else if (type == "CTD" || type == "LE_CTD") { opcode = LE_OP_CTD; is_counter_op = true; }
        else if (type == "CTUD" || type == "LE_CTUD") { opcode = LE_OP_CTUD; is_counter_op = true; }
        else if (type == "ADD" || type == "LE_ADD") { opcode = LE_OP_ADD_F; is_float_op = true; }
        else if (type == "SUB" || type == "SUBTRACT" || type == "LE_SUB") { opcode = LE_OP_SUB_F; is_float_op = true; }
        else if (type == "MUL" || type == "MULTIPLY" || type == "LE_MUL") { opcode = LE_OP_MUL_F; is_float_op = true; }
        else if (type == "DIV" || type == "DIVIDE" || type == "LE_DIV") { opcode = LE_OP_DIV_F; is_float_op = true; }
        else if (type == "ABS" || type == "LE_ABS") { opcode = LE_OP_ABS_F; is_float_op = true; }
        else if (type == "NEG" || type == "LE_NEG") { opcode = LE_OP_NEG_F; is_float_op = true; }
        else if (type == "MIN" || type == "LE_MIN") { opcode = LE_OP_MIN_F; is_float_op = true; }
        else if (type == "MAX" || type == "LE_MAX") { opcode = LE_OP_MAX_F; is_float_op = true; }
        else if (type == "CLAMP" || type == "LE_CLAMP") { opcode = LE_OP_BLOCK; } /* block func LE_FUNC_CLAMP_F */
        else if (type == "CADD" || type == "LE_CADD" || type == "C_ADD") { opcode = LE_OP_CADD_F; is_complex_op = true; }
        else if (type == "CSUB" || type == "LE_CSUB" || type == "C_SUB") { opcode = LE_OP_CSUB_F; is_complex_op = true; }
        else if (type == "CMUL" || type == "LE_CMUL" || type == "C_MUL") { opcode = LE_OP_CMUL_F; is_complex_op = true; }
        else if (type == "CDIV" || type == "LE_CDIV" || type == "C_DIV") { opcode = LE_OP_CDIV_F; is_complex_op = true; }
        else if (type == "COMPLEX2POLAR" || type == "LE_COMPLEX2POLAR") { opcode = LE_OP_RECT2POLAR; is_float_op = true; }
        else if (type == "COMPLEX2RECT" || type == "LE_COMPLEX2RECT") { opcode = LE_OP_POLAR2RECT; is_float_op = true; }
        else if (type == "RECT2COMPLEX" || type == "LE_RECT2COMPLEX") { opcode = LE_OP_RECT2POLAR; is_float_op = true; }
        else if (type == "POLAR2COMPLEX" || type == "LE_POLAR2COMPLEX") { opcode = LE_OP_POLAR2RECT; is_float_op = true; }
        else if (type == "DIFF_87" || type == "DIFF" || type == "LE_DIFF_87" || type == "LE_DIFF") { opcode = LE_OP_EXT_CALL; uses_protection = true;
            if (state_descs.find(LE_BLK_DIFF_87) == state_descs.end()) state_descs[LE_BLK_DIFF_87] = 1; }
        else if (type == "PHASE_COMP" || type == "TRANSFORM_33" || type == "TCOMP" || type == "LE_PHASE_COMP" || type == "LE_TRANSFORM_33") { opcode = LE_OP_BLOCK; uses_protection = true;
            if (state_descs.find(LE_BLK_PHASE_COMP) == state_descs.end()) state_descs[LE_BLK_PHASE_COMP] = 1; }
        else if (type == "CMP_GT" || type == "LE_CMP_GT") opcode = LE_OP_CMP_GT;
        else if (type == "CMP_LT" || type == "LE_CMP_LT") opcode = LE_OP_CMP_LT;
        else if (type == "CMP_GE" || type == "LE_CMP_GE") opcode = LE_OP_CMP_GE;
        else if (type == "CMP_LE" || type == "LE_CMP_LE") opcode = LE_OP_CMP_LE;
        else if (type == "CMP_EQ" || type == "LE_CMP_EQ") opcode = LE_OP_CMP_EQ;
        else if (type == "CMP_NE" || type == "LE_CMP_NE") opcode = LE_OP_CMP_NE;
        else if (type == "PID" || type == "LE_PID") { opcode = LE_OP_PID; uses_protection = true; is_float_op = true;
            if (state_descs.find(LE_BLK_PID) == state_descs.end()) state_descs[LE_BLK_PID] = 1; }
        else if (type == "OVERCURRENT_51" || type == "OVERCURRENT" || type == "LE_OVERCURRENT_51" || type == "LE_OVERCURRENT") { opcode = LE_OP_OVERCURRENT; uses_protection = true;
            if (state_descs.find(LE_BLK_OVERCURRENT) == state_descs.end()) state_descs[LE_BLK_OVERCURRENT] = 1; }
        else if (type == "RECT2POLAR" || type == "LE_RECT2POLAR") { opcode = LE_OP_RECT2POLAR; is_float_op = true; }
        else if (type == "POLAR2RECT" || type == "LE_POLAR2RECT") { opcode = LE_OP_POLAR2RECT; is_float_op = true; }
        else if (type == "PHASOR_SHIFT" || type == "LE_PHASOR_SHIFT") { opcode = LE_OP_PHASOR_SHIFT; is_float_op = true; }
        else if (type == "PHASOR_1P" || type == "LE_PHASOR_1P" || type == "LE_1P_WINDING") { opcode = LE_OP_PHASOR_1P; uses_protection = true; }
        else if (type == "SYM_COMP" || type == "LE_SYM_COMP") { opcode = LE_OP_SYM_COMP; uses_protection = true;
            if (state_descs.find(LE_BLK_SYMCOMP) == state_descs.end()) state_descs[LE_BLK_SYMCOMP] = 1; }
        else if (type == "DIST_21" || type == "LE_DIST_21") { opcode = LE_OP_DIST_21; uses_protection = true;
            if (state_descs.find(LE_BLK_21) == state_descs.end()) state_descs[LE_BLK_21] = 1; }
        else if (type == "I2C" || type == "LE_I2C") { opcode = LE_OP_I2C; uses_serial_bus = true; state_descs[LE_BLK_I2C] = 1; }
        else if (type == "SPI" || type == "LE_SPI") { opcode = LE_OP_SPI; uses_serial_bus = true; state_descs[LE_BLK_SPI] = 1; }
        else if (type == "LPF" || type == "LE_LPF" || type == "LPF_1P") { opcode = LE_OP_LPF_1P; uses_dsp = true; is_float_op = true; }
        else if (type == "BIQUAD" || type == "LE_BIQUAD" || type == "BIQUAD_IIR") { opcode = LE_OP_BIQUAD_IIR; uses_dsp = true; is_float_op = true; }
        else if (type == "MOVING_AVG" || type == "LE_MOVING_AVG" || type == "WINDOW_AVG") { opcode = LE_OP_MOVING_AVG; uses_dsp = true; is_float_op = true; }
        else if (type == "RATE_LIMITER" || type == "LE_RATE_LIMITER" || type == "SLEW_LIMITER") { opcode = LE_OP_RATE_LIMITER; uses_dsp = true; is_float_op = true; }
        else if (type == "DEADBAND" || type == "LE_DEADBAND") { opcode = LE_OP_DEADBAND; uses_dsp = true; is_float_op = true; }
        else if (type == "WASHOUT" || type == "LE_WASHOUT") { opcode = LE_OP_WASHOUT; uses_dsp = true; is_float_op = true; }
        else if (type == "PEAK_DETECTOR" || type == "LE_PEAK_DETECTOR" || type == "ENVELOPE") { opcode = LE_OP_PEAK_DETECTOR; uses_dsp = true; is_float_op = true; }
        else if (type == "RMS" || type == "LE_RMS") { opcode = LE_OP_RMS; uses_dsp = true; is_float_op = true; }
        else if (type == "MEDIAN" || type == "LE_MEDIAN" || type == "MEDIAN_FILTER") { opcode = LE_OP_MEDIAN; uses_dsp = true; is_float_op = true; }
        else if (type == "DERIVATIVE" || type == "LE_DERIVATIVE") { opcode = LE_OP_DERIVATIVE; uses_dsp = true; is_float_op = true; }
        else if (type == "ZERO_CROSSING" || type == "LE_ZERO_CROSSING") { opcode = LE_OP_ZERO_CROSSING; uses_dsp = true; is_float_op = true; }
        else if (type == "LUT_1D" || type == "LE_LUT_1D" || type == "LUT") { opcode = LE_OP_LUT_1D; uses_dsp = true; is_float_op = true; }
        else if (type == "TOTALIZER" || type == "LE_TOTALIZER") { opcode = LE_OP_TOTALIZER; uses_dsp = true; is_float_op = true; }
        else if (type == "MIN_MAX_HOLD" || type == "LE_MIN_MAX_HOLD") { opcode = LE_OP_MIN_MAX_HOLD; uses_dsp = true; is_float_op = true; }

        // Check for Custom Node
        const CustomNodeInfo* custom_def = nullptr;
        std::string custom_type = el.get("custom_type").as_string(el.get("type").as_string());
        if (custom_nodes.find(custom_type) != custom_nodes.end()) {
            custom_def = &custom_nodes[custom_type];
        } else if (type == "CUSTOMNODE" || type == "LE_CUSTOM" || type == "LE_NODE_CUSTOM" || type == "LE_OP_EXT_CALL" || type == "EXT_CALL") {
            // Local fallback custom definition
            static CustomNodeInfo fallback_def;
            fallback_def.function_id = static_cast<uint8_t>((int)prop(el, "function_id", 1));
            std::string out_type = to_lower_str(el.get("output_type").as_string("bool"));
            fallback_def.outputs.clear();
            CustomPinInfo pi;
            pi.type = out_type;
            fallback_def.outputs.push_back(pi);
            custom_def = &fallback_def;
        }

        uint8_t modifier = opt_node.modifier;
        if (custom_def) {
            opcode = LE_OP_EXT_CALL;
            modifier = custom_def->function_id;
            if (!custom_def->outputs.empty()) {
                std::string ot = to_lower_str(custom_def->outputs[0].type);
                if (ot == "float") is_float_op = true;
                else if (ot == "int" || ot == "integer") is_int_op = true;
            }
        } else if (opcode == LE_OP_LPF_1P) {
            modifier = static_cast<uint8_t>(lpf_counter++);
            state_descs[LE_BLK_LPF]++;
        } else if (opcode == LE_OP_BIQUAD_IIR) {
            modifier = static_cast<uint8_t>(biquad_counter++);
            state_descs[LE_BLK_BIQUAD]++;
        } else if (opcode == LE_OP_MOVING_AVG) {
            modifier = static_cast<uint8_t>(moving_avg_counter++);
            state_descs[LE_BLK_MOVING_AVG]++;
        } else if (opcode == LE_OP_RATE_LIMITER) {
            modifier = static_cast<uint8_t>(rate_limiter_counter++);
            state_descs[LE_BLK_RATE_LIMITER]++;
        } else if (opcode == LE_OP_DEADBAND) {
            modifier = static_cast<uint8_t>(deadband_counter++);
            state_descs[LE_BLK_DEADBAND]++;
        } else if (opcode == LE_OP_WASHOUT) {
            modifier = static_cast<uint8_t>(washout_counter++);
            state_descs[LE_BLK_WASHOUT]++;
        } else if (opcode == LE_OP_PEAK_DETECTOR) {
            modifier = static_cast<uint8_t>(peak_counter++);
            state_descs[LE_BLK_PEAK]++;
        } else if (opcode == LE_OP_RMS) {
            modifier = static_cast<uint8_t>(rms_counter++);
            state_descs[LE_BLK_RMS]++;
        } else if (opcode == LE_OP_MEDIAN) {
            modifier = static_cast<uint8_t>(median_counter++);
            state_descs[LE_BLK_MEDIAN]++;
        } else if (opcode == LE_OP_DERIVATIVE) {
            modifier = static_cast<uint8_t>(derivative_counter++);
            state_descs[LE_BLK_DERIVATIVE]++;
        } else if (opcode == LE_OP_ZERO_CROSSING) {
            modifier = static_cast<uint8_t>(zero_crossing_counter++);
            state_descs[LE_BLK_ZERO_CROSSING]++;
        } else if (opcode == LE_OP_LUT_1D) {
            modifier = static_cast<uint8_t>(lut_1d_counter++);
            state_descs[LE_BLK_LUT_1D]++;
        } else if (opcode == LE_OP_TOTALIZER) {
            modifier = static_cast<uint8_t>(totalizer_counter++);
            state_descs[LE_BLK_TOTALIZER]++;
        } else if (opcode == LE_OP_MIN_MAX_HOLD) {
            modifier = static_cast<uint8_t>(min_max_hold_counter++);
            state_descs[LE_BLK_MIN_MAX_HOLD]++;
        } else if (opcode == LE_OP_PHASOR_SHIFT) {
            /* Rotation angle (degrees, 0..255) is carried in the modifier byte. */
            int deg = static_cast<int>(el.get("angle_deg").as_float(prop(el, "delta_deg", 0.0f)));
            modifier = static_cast<uint8_t>(deg & 0xFF);
        }

        if (opcode == LE_OP_NOP) {
            continue;
        }

/* Materialize each stateful element\'s state struct (defaults + circuit
         * properties baked as concrete bytes) into the preconfigured state image
         * that the loader copies to RAM verbatim at load. */
        switch (opcode) {
            case LE_OP_LPF_1P: { le_lpf_state_t st{}; st.alpha = prop(el, "alpha", 0.1f); append_state(LE_BLK_LPF, &st, sizeof(st)); break; }
            case LE_OP_RATE_LIMITER: { le_rate_limiter_state_t st{}; st.rising_rate = prop(el, "rising_rate", 1.0f); st.falling_rate = prop(el, "falling_rate", 1.0f); append_state(LE_BLK_RATE_LIMITER, &st, sizeof(st)); break; }
            case LE_OP_BIQUAD_IIR: { le_biquad_state_t st{}; st.b0 = prop(el, "b0", 1.0f); st.b1 = prop(el, "b1", 0.0f); st.b2 = prop(el, "b2", 0.0f); st.a1 = prop(el, "a1", 0.0f); st.a2 = prop(el, "a2", 0.0f); append_state(LE_BLK_BIQUAD, &st, sizeof(st)); break; }
            case LE_OP_MOVING_AVG: { le_moving_avg_state_t st{}; st.window_size = (uint16_t)prop(el, "window_size", 8.0f); append_state(LE_BLK_MOVING_AVG, &st, sizeof(st)); break; }
            case LE_OP_PEAK_DETECTOR: { le_peak_state_t st{}; st.decay_rate = prop(el, "decay_rate", 0.995f); append_state(LE_BLK_PEAK, &st, sizeof(st)); break; }
            case LE_OP_RMS: { le_rms_state_t st{}; st.window_size = (uint16_t)prop(el, "window_size", 16.0f); append_state(LE_BLK_RMS, &st, sizeof(st)); break; }
            case LE_OP_MEDIAN: { le_median_state_t st{}; st.window_size = (uint16_t)prop(el, "window_size", 5.0f); append_state(LE_BLK_MEDIAN, &st, sizeof(st)); break; }
            case LE_OP_DEADBAND: { le_deadband_state_t st{}; st.threshold = prop(el, "threshold", 0.0f); st.center = prop(el, "center", 0.0f); append_state(LE_BLK_DEADBAND, &st, sizeof(st)); break; }
            case LE_OP_WASHOUT: { le_washout_state_t st{}; st.alpha = prop(el, "alpha", 0.95f); append_state(LE_BLK_WASHOUT, &st, sizeof(st)); break; }
            case LE_OP_DERIVATIVE: { le_derivative_state_t st{}; st.alpha = prop(el, "alpha", 0.8f); st.gain = prop(el, "gain", 1000.0f); append_state(LE_BLK_DERIVATIVE, &st, sizeof(st)); break; }
            case LE_OP_ZERO_CROSSING: { le_zero_crossing_state_t st{}; st.hysteresis = prop(el, "hysteresis", 0.05f); st.sample_rate_hz = prop(el, "sample_rate_hz", 1000.0f); append_state(LE_BLK_ZERO_CROSSING, &st, sizeof(st)); break; }
            case LE_OP_LUT_1D: { le_lut_1d_state_t st{}; st.num_points = (uint16_t)prop(el, "num_points", 2.0f); const auto& xarr = el.get("x").arr_val; const auto& yarr = el.get("y").arr_val; for (size_t k = 0; k < xarr.size() && k < LE_MAX_LUT_POINTS; k++) st.x[k] = (float)xarr[k].as_float(0.0f); for (size_t k = 0; k < yarr.size() && k < LE_MAX_LUT_POINTS; k++) st.y[k] = (float)yarr[k].as_float(0.0f); if (st.num_points < 2) st.num_points = 2; if (st.num_points > LE_MAX_LUT_POINTS) st.num_points = LE_MAX_LUT_POINTS; append_state(LE_BLK_LUT_1D, &st, sizeof(st)); break; }
            case LE_OP_TOTALIZER: { le_totalizer_state_t st{}; st.time_base_sec = prop(el, "time_base_sec", 60.0f); st.scale_factor = prop(el, "scale_factor", 1.0f); st.sample_time_sec = prop(el, "sample_time_sec", 0.001f); st.max_limit = prop(el, "max_limit", 0.0f); append_state(LE_BLK_TOTALIZER, &st, sizeof(st)); break; }
            case LE_OP_MIN_MAX_HOLD: { le_min_max_hold_state_t st{}; st.mode = (uint8_t)prop(el, "mode", 0.0f); append_state(LE_BLK_MIN_MAX_HOLD, &st, sizeof(st)); break; }
            case LE_OP_OVERCURRENT: { le_overcurrent_state_t st{}; st.pickup = prop(el, "pickup", 1.0f); st.time_dial = prop(el, "time_dial", 1.0f); st.curve_type = (uint8_t)prop(el, "curve_type", 0.0f); append_state(LE_BLK_OVERCURRENT, &st, sizeof(st)); break; }
            case LE_OP_DIST_21: { le_dist21_state_t st{}; st.reach_ohms = (float)prop(el, "reach", prop(el, "reach_ohms", 10.0)); st.line_angle_deg = (float)prop(el, "line_angle", prop(el, "line_angle_deg", 75.0)); st.offset_mag = (float)prop(el, "offset", prop(el, "offset_ohms", 0.0)); st.offset_angle_deg = (float)prop(el, "offset_angle", prop(el, "offset_angle_deg", 75.0)); st.prefault_v_threshold = (float)prop(el, "prefault_v_threshold", 0.5); st.prefault_duration_ms = (uint32_t)prop(el, "prefault_v_duration", prop(el, "prefault_duration_ms", 80.0)); append_state(LE_BLK_21, &st, sizeof(st)); break; }
            case LE_OP_PID: { le_pid_state_t st{}; st.kp = prop(el, "kp", 1.0f); st.ki = prop(el, "ki", 0.0f); st.kd = prop(el, "kd", 0.0f); st.out_min = prop(el, "out_min", -1e6f); st.out_max = prop(el, "out_max", 1e6f); append_state(LE_BLK_PID, &st, sizeof(st)); break; }
            case LE_OP_I2C: { le_i2c_device_state_t st{}; st.addr_7bit = (uint8_t)prop(el, "addr", 0.0f); st.poll_rate_ms = (uint32_t)prop(el, "poll_rate_ms", 0.0f); st.poll_tx_len = (uint8_t)prop(el, "poll_tx_len", 0.0f); st.poll_rx_len = (uint8_t)prop(el, "poll_rx_len", 0.0f); st.data_dest_addr = (uint16_t)prop(el, "data_dest_addr", 0.0f); append_state(LE_BLK_I2C, &st, sizeof(st)); break; }
            case LE_OP_SPI: { le_spi_device_state_t st{}; st.cs_pin = (uint8_t)prop(el, "cs_pin", 0.0f); st.poll_rate_ms = (uint32_t)prop(el, "poll_rate_ms", 0.0f); st.poll_len = (uint8_t)prop(el, "poll_len", 0.0f); st.data_dest_addr = (uint16_t)prop(el, "data_dest_addr", 0.0f); append_state(LE_BLK_SPI, &st, sizeof(st)); break; }
            default: break;
        }

        const auto& conn = opt_node.inputs;
        std::string in0_src;
        std::string in1_src;
        std::vector<std::string> used_keys;  /* consumer-port aliases matched, in order */

        auto find_port = [&](const std::vector<std::string>& port_names) -> std::string {
            for (const auto& pn : port_names) {
                auto it = conn.find(pn);
                if (it != conn.end()) { used_keys.push_back(pn); return it->second; }
            }
            return "";
        };

        /* source output port observed by a given consumer input port */
        auto source_port_of = [&](const std::string& src_elem, const std::string& consumer_port) -> std::string {
            auto m = net_source_port.find(src_elem);
            if (m != net_source_port.end()) {
                auto p = m->second.find(consumer_port);
                if (p != m->second.end()) return p->second;
            }
            return "";
        };

        /* Resolve a source (element, output-port) into an address, falling back to
         * the element's canonical/only output. Enables multi-output selection. */
        auto src_address = [&](const std::string& src, const std::string& sport, uint16_t default_addr) -> uint16_t {
            if (src.empty()) return default_addr;
            if (!sport.empty()) {
                auto e = element_outputs_port.find(src);
                if (e != element_outputs_port.end()) {
                    auto p = e->second.find(sport);
                    if (p != e->second.end()) return p->second;
                }
            }
            auto it = element_outputs.find(src);
            return (it != element_outputs.end()) ? it->second : default_addr;
        };

        /* source output port observed by a given consumer input port */
        /* (source_port_of / src_address are shared lambdas defined above the loop) */

        /* ------------------------------------------------------------------ */
        /* Variable-arity block calls (MUX, custom nodes).                     */
        /* Emitted as a single LE_OP_BLOCK instruction + a block descriptor.   */
        /* ------------------------------------------------------------------ */
        if (opcode == LE_OP_MUX) {
            std::string sel_src = find_port({"sel", "select", "selector", "s", "in_sel"});
            std::string in0_src = find_port({"in0", "input_0", "a", "in_a"});
            std::string in1_src = find_port({"in1", "input_1", "b", "in_b"});

            uint16_t sel = block_src_addr(sel_src, LE_CONST_FALSE);
            uint16_t in0 = block_src_addr(in0_src, LE_CONST_FALSE);
            uint16_t in1 = block_src_addr(in1_src, LE_CONST_FALSE);

            consume_input(sel_src);
            consume_input(in0_src);
            consume_input(in1_src);

            uint16_t out_addr = LE_ADDR_UNUSED;
            if (!opt_node.direct_dest_node.empty()) {
                if (dout_map.find(opt_node.direct_dest_node) != dout_map.end()) {
                    out_addr = dout_map[opt_node.direct_dest_node];
                } else if (element_outputs.find(opt_node.direct_dest_node) != element_outputs.end()) {
                    out_addr = element_outputs[opt_node.direct_dest_node];
                }
            }
            if (out_addr == LE_ADDR_UNUSED) {
                int temp_idx = -1;
                out_addr = acquire_temp_bool(temp_idx);
                node_temp_idx[name] = temp_idx;
                node_temp_type[name] = TempType::Bool;
            }
            element_outputs[name] = out_addr;

            push_block(LE_FUNC_MUX_SELECT, {sel, in0, in1, out_addr}, 3, 1);
            continue;
        }

if (type == "DIFF_87" || type == "DIFF" || type == "LE_DIFF_87" || type == "LE_DIFF") {
            /* N-input dual-slope differential protection (ANSI 87): N complex phasors
             * -> bool trip. input_count is designer-defined (default 2); the dual-slope
             * restraint characteristic (o87p/slp1/irs1/slp2) is baked into the state
             * image, SEL-style (O87P/SLP1/IRS1/SLP2). Supports large-bus differential
             * with up to 30 phasor inputs. */
            std::vector<uint16_t> args;
            int n_in = (int)prop(el, "input_count", 2.0);
            if (n_in < 2) n_in = 2;
            if (n_in > 30) n_in = 30;
            int n_added = 0;
            for (int k = 0; k < n_in; k++) {
                std::vector<std::string> names = { "in", "a", "c", "complex", "in_a" };
                if (k == 1) names = { "b", "in_b" };
                std::string src = find_port(names);
                args.push_back(block_src_addr(src, LE_CONST_ZERO_C));
                consume_input(src);
                n_added++;
            }

            le_diff87_state_t d87{};
            d87.o87p = (float)prop(el, "o87p", prop(el, "pickup", 0.3));
            d87.slp1 = (float)prop(el, "slp1", 0.25);
            d87.irs1 = (float)prop(el, "irs1", prop(el, "ips1", 1.5));
            d87.slp2 = (float)prop(el, "slp2", 0.60);
            append_state(LE_BLK_DIFF_87, &d87, sizeof(d87));

            int t0 = -1;
            uint16_t out0 = acquire_temp_bool(t0);
            node_temp_idx[name] = t0;
            node_temp_type[name] = TempType::Bool;
            element_outputs[name] = out0;
            args.push_back(out0);
            push_block(LE_FUNC_DIFF_87, args, static_cast<uint8_t>(n_added), 1);
            continue;
        }

if (type == "PHASE_COMP" || type == "TRANSFORM_33" || type == "TCOMP" || type == "LE_PHASE_COMP" || type == "LE_TRANSFORM_33") {
            /* 3-phase transformer phase-shift compensation (ANSI 87T): three complex
             * phasors in -> three complex phasors out, transformed by the SEL
             * compensation matrix M(k) where k=comp (odd k s=1/sqrt(3), even k
             * s=1/3). comp (1..12) is baked into le_comp33_state_t. */
            std::vector<uint16_t> args;
            int n_in = 0;
            auto add_in = [&](const std::string& port_src, uint16_t def_addr) {
                args.push_back(block_src_addr(port_src, def_addr));
                consume_input(port_src);
                n_in++;
            };
            add_in(find_port({"a", "phase_a", "ia", "in_a", "ca"}), LE_CONST_ZERO_C);
            add_in(find_port({"b", "phase_b", "ib", "in_b", "cb"}), LE_CONST_ZERO_C);
            add_in(find_port({"c", "phase_c", "ic", "in_c", "cc"}), LE_CONST_ZERO_C);

            le_comp33_state_t c33{};
            c33.comp = (uint8_t)((int)el.get("compensation").as_float(el.get("comp").as_float(prop(el, "tcomp", 6.0))) % 13);
            append_state(LE_BLK_PHASE_COMP, &c33, sizeof(c33));

            for (int k = 0; k < 3; k++) {
                int t = -1;
                uint16_t o = acquire_temp_complex(t);
                element_outputs_port[name][(k == 0) ? "a" : (k == 1) ? "b" : "c"] = o;
                if (k == 0) {
                    node_temp_idx[name] = t;
                    node_temp_type[name] = TempType::Complex;
                    element_outputs[name] = o;
                }
                args.push_back(o);
            }
            push_block(LE_FUNC_PHASE_COMP, args, static_cast<uint8_t>(n_in), 3);
            continue;
        }

if (type == "DIST_21" || type == "LE_DIST_21") {
            /* Mho distance (21): v_c, i_c complex phasors + offset_on boolean -> bool trip.
             * Prefault voltage memory uses the offset_on-independent threshold/duration
             * properties; the boolean only gates the mho-offset circle shift. */
            std::vector<uint16_t> args;
            int n_in = 0;
            auto add_in = [&](const std::string& port_src, uint16_t def_addr) {
                args.push_back(block_src_addr(port_src, def_addr));
                consume_input(port_src);
                n_in++;
            };
            add_in(find_port({"v", "voltage", "v_c", "cplx_v", "a", "in_a"}), LE_CONST_ZERO_C);
            add_in(find_port({"i", "current", "i_c", "cplx_i", "b", "in_b"}), LE_CONST_ZERO_C);
            add_in(find_port({"offset_on", "offset_en", "offset", "oe"}), LE_CONST_FALSE);

            int t0 = -1;
            uint16_t out0 = acquire_temp_bool(t0);
            node_temp_idx[name] = t0;
            node_temp_type[name] = TempType::Bool;
            element_outputs[name] = out0;
            args.push_back(out0);
            push_block(LE_FUNC_DIST_21, args, static_cast<uint8_t>(n_in), 1);
            continue;
        }
        if (type == "COMPLEX2POLAR" || type == "LE_COMPLEX2POLAR") {
            /* COMPLEX2POLAR: one complex in -> {mag, angle} floats (1-in/2-out). */
            std::vector<uint16_t> args;
            int n_in = 0;
            auto add_in = [&](const std::string& port_src, uint16_t def_addr) {
                args.push_back(block_src_addr(port_src, def_addr));
                consume_input(port_src);
                n_in++;
            };
            add_in(find_port({"in", "a", "c", "complex", "in_a"}), LE_CONST_ZERO_C);

            int t0 = -1, t1 = -1;
            uint16_t out0 = acquire_temp_float(t0);
            uint16_t out1 = acquire_temp_float(t1);
            node_temp_idx[name] = t0;
            node_temp_type[name] = TempType::Float;
            element_outputs[name] = out0;
            args.push_back(out0);
            args.push_back(out1);
            element_outputs_port[name]["magnitude"] = out0;
            element_outputs_port[name]["mag"] = out0;
            element_outputs_port[name]["angle"] = out1;
            element_outputs_port[name]["ang"] = out1;
            push_block(LE_FUNC_COMPLEX2POLAR, args, static_cast<uint8_t>(n_in), 2);
            continue;
        }

        if (type == "COMPLEX2RECT" || type == "LE_COMPLEX2RECT") {
            /* COMPLEX2RECT: one complex in -> {real, imag} floats (1-in/2-out). */
            std::vector<uint16_t> args;
            int n_in = 0;
            auto add_in = [&](const std::string& port_src, uint16_t def_addr) {
                args.push_back(block_src_addr(port_src, def_addr));
                consume_input(port_src);
                n_in++;
            };
            add_in(find_port({"in", "a", "c", "complex", "in_a"}), LE_CONST_ZERO_C);

            int t0 = -1, t1 = -1;
            uint16_t out0 = acquire_temp_float(t0);
            uint16_t out1 = acquire_temp_float(t1);
            node_temp_idx[name] = t0;
            node_temp_type[name] = TempType::Float;
            element_outputs[name] = out0;
            args.push_back(out0);
            args.push_back(out1);
            element_outputs_port[name]["real"] = out0;
            element_outputs_port[name]["imag"] = out1;
            push_block(LE_FUNC_COMPLEX2RECT, args, static_cast<uint8_t>(n_in), 2);
            continue;
        }

        if (type == "RECT2COMPLEX" || type == "LE_RECT2COMPLEX") {
            /* RECT2COMPLEX: {real, imag} floats -> one complex out (2-in/1-out). */
            std::vector<uint16_t> args;
            int n_in = 0;
            auto add_in = [&](const std::string& port_src, uint16_t def_addr) {
                args.push_back(block_src_addr(port_src, def_addr));
                consume_input(port_src);
                n_in++;
            };
            add_in(find_port({"real", "a", "in_a", "in", "x"}), LE_CONST_ZERO_F);
            add_in(find_port({"imag", "imaginary", "b", "in_b", "y"}), LE_CONST_ZERO_F);

            int t0 = -1;
            uint16_t out0 = acquire_temp_complex(t0);
            node_temp_idx[name] = t0;
            node_temp_type[name] = TempType::Complex;
            element_outputs[name] = out0;
            element_outputs_port[name]["real"] = out0;
            element_outputs_port[name]["imag"] = out0;
            element_outputs_port[name]["out"] = out0;
            args.push_back(out0);
            push_block(LE_FUNC_RECT2COMPLEX, args, static_cast<uint8_t>(n_in), 1);
            continue;
        }

        if (type == "POLAR2COMPLEX" || type == "LE_POLAR2COMPLEX") {
            /* POLAR2COMPLEX: {mag, angle} floats -> one complex out (2-in/1-out). */
            std::vector<uint16_t> args;
            int n_in = 0;
            auto add_in = [&](const std::string& port_src, uint16_t def_addr) {
                args.push_back(block_src_addr(port_src, def_addr));
                consume_input(port_src);
                n_in++;
            };
            add_in(find_port({"mag", "magnitude", "a", "in_a", "in"}), LE_CONST_ZERO_F);
            add_in(find_port({"ang", "angle", "b", "in_b"}), LE_CONST_ZERO_F);

            int t0 = -1;
            uint16_t out0 = acquire_temp_complex(t0);
            node_temp_idx[name] = t0;
            node_temp_type[name] = TempType::Complex;
            element_outputs[name] = out0;
            element_outputs_port[name]["real"] = out0;
            element_outputs_port[name]["imag"] = out0;
            element_outputs_port[name]["out"] = out0;
            args.push_back(out0);
            push_block(LE_FUNC_POLAR2COMPLEX, args, static_cast<uint8_t>(n_in), 1);
            continue;
        }

        if (type == "CLAMP" || type == "LE_CLAMP") {
            /* CLAMP: {value, min, max} floats -> one float out (3-in/1-out). */
            std::vector<uint16_t> args;
            int n_in = 0;
            auto add_in = [&](const std::string& port_src, uint16_t def_addr) {
                args.push_back(block_src_addr(port_src, def_addr));
                consume_input(port_src);
                n_in++;
            };
            add_in(find_port({"in", "value", "a", "in_a", "x", "input"}), LE_CONST_ZERO_F);
            add_in(find_port({"min", "lo", "low", "min_val", "b", "in_b"}), LE_CONST_ZERO_F);
            add_in(find_port({"max", "hi", "high", "max_val"}), LE_CONST_ZERO_F);

            int t0 = -1;
            uint16_t out0 = acquire_temp_float(t0);
            node_temp_idx[name] = t0;
            node_temp_type[name] = TempType::Float;
            element_outputs[name] = out0;
            element_outputs_port[name]["out"] = out0;
            args.push_back(out0);
            push_block(LE_FUNC_CLAMP_F, args, static_cast<uint8_t>(n_in), 1);
            continue;
        }

if (opcode == LE_OP_PHASOR_1P) {
            /* PHASOR_1P: complex output phasor (2-in/1-out). */
            std::vector<uint16_t> args;
            int n_in = 0;
            auto add_in = [&](const std::string& port_src, uint16_t def_addr) {
                args.push_back(block_src_addr(port_src, def_addr));
                consume_input(port_src);
                n_in++;
            };
            add_in(find_port({"in", "a", "sample", "x", "pv"}), LE_CONST_ZERO_F);
            add_in(find_port({"sync", "sync_complex", "ref", "b", "sync_phasor"}), LE_CONST_ZERO_C);
            state_descs[LE_BLK_PHASOR]++;
            le_phasor_state_t ph{};
            ph.samples_per_cycle = (uint16_t)prop(el, "samples_per_cycle", 16.0);
            if (ph.samples_per_cycle == 0 || ph.samples_per_cycle > LE_MAX_SAMPLES_PER_CYCLE)
                ph.samples_per_cycle = 16;
            append_state(LE_BLK_PHASOR, &ph, sizeof(ph));

            int t0 = -1;
            uint16_t out0 = acquire_temp_complex(t0);
            node_temp_idx[name] = t0;
            node_temp_type[name] = TempType::Complex;
            element_outputs[name] = out0;
            element_outputs_port[name]["phasor"] = out0;
            element_outputs_port[name]["out"] = out0;
            args.push_back(out0);
            push_block(LE_FUNC_PHASOR_1P, args, static_cast<uint8_t>(n_in), 1);
            continue;
        }
        /* Phasor conversions: 2-in (or 3-in) -> 2-out blocks (RECT2POLAR/POLAR2RECT/PHASOR_SHIFT). */
        if (opcode == LE_OP_RECT2POLAR || opcode == LE_OP_POLAR2RECT ||
            opcode == LE_OP_PHASOR_SHIFT) {
            std::vector<uint16_t> args;
            uint8_t fid = LE_FUNC_NONE;
            int n_in = 0;
            auto add_in = [&](const std::string& port_src, uint16_t def_addr) {
                args.push_back(block_src_addr(port_src, def_addr));
                consume_input(port_src);
                n_in++;
            };
            if (opcode == LE_OP_RECT2POLAR) {
                add_in(find_port({"a", "real", "in_a", "in"}), LE_CONST_ZERO_F);
                add_in(find_port({"b", "imag", "in_b", "imaginary"}), LE_CONST_ZERO_F);
                fid = LE_FUNC_RECT2POLAR;
            } else if (opcode == LE_OP_POLAR2RECT) {
                add_in(find_port({"a", "mag", "in_a", "magnitude", "in"}), LE_CONST_ZERO_F);
                add_in(find_port({"b", "angle", "in_b", "ang"}), LE_CONST_ZERO_F);
                fid = LE_FUNC_POLAR2RECT;
            } else if (opcode == LE_OP_PHASOR_SHIFT) {
                add_in(find_port({"a", "real", "in_a", "in"}), LE_CONST_ZERO_F);
                add_in(find_port({"b", "imag", "in_b"}), LE_CONST_ZERO_F);
                add_in(find_port({"delta", "delta_rad", "angle"}), LE_CONST_ZERO_F);
                fid = LE_FUNC_PHASOR_SHIFT;
            }

            /* Two float outputs: the first (magnitude / real) is the canonical
             * element output consumed by single-output connectives. */
            int t0 = -1, t1 = -1;
            uint16_t out0 = acquire_temp_float(t0);
            uint16_t out1 = acquire_temp_float(t1);
            node_temp_idx[name] = t0;
            node_temp_type[name] = TempType::Float;
            element_outputs[name] = out0;
            args.push_back(out0);
            args.push_back(out1);

            /* Expose both outputs under their schematic port names so a consumer
             * can select magnitude vs. angle (or real vs. imag) explicitly. */
            element_outputs_port[name]["magnitude"] = out0;
            element_outputs_port[name]["mag"] = out0;
            element_outputs_port[name]["angle"] = out1;
            element_outputs_port[name]["ang"] = out1;
            if (opcode == LE_OP_POLAR2RECT || opcode == LE_OP_PHASOR_SHIFT) {
                element_outputs_port[name]["real"] = out0;
                element_outputs_port[name]["imag"] = out1;
                element_outputs_port[name].erase("magnitude");
                element_outputs_port[name].erase("angle");
            }

            push_block(fid, args, static_cast<uint8_t>(n_in), 2);
            continue;
        }

        if (custom_def) {
            std::vector<uint16_t> args;
            int n_in = (int)custom_def->inputs.size();
            for (int i = 0; i < n_in; i++) {
                std::vector<std::string> names;
                names.push_back(to_lower_str(custom_def->inputs[i].name));
                names.push_back("input_" + std::to_string(i));
                names.push_back("in");
                names.push_back((i == 1) ? "b" : "a");
                std::string in_type = to_lower_str(custom_def->inputs[i].type);
                uint16_t def_addr = (in_type.find("float") != std::string::npos) ? LE_CONST_ZERO_F : LE_CONST_FALSE;
                std::string src = find_port(names);
                args.push_back(block_src_addr(src, def_addr));
                consume_input(src);
            }

            std::string out_type = (!custom_def->outputs.empty()) ? to_lower_str(custom_def->outputs[0].type) : "bool";
            uint16_t out_addr = LE_ADDR_UNUSED;
            if (!opt_node.direct_dest_node.empty()) {
                if (dout_map.find(opt_node.direct_dest_node) != dout_map.end()) {
                    out_addr = dout_map[opt_node.direct_dest_node];
                } else if (element_outputs.find(opt_node.direct_dest_node) != element_outputs.end()) {
                    out_addr = element_outputs[opt_node.direct_dest_node];
                }
            }
            if (out_addr == LE_ADDR_UNUSED) {
                int temp_idx = -1;
                if (out_type.find("float") != std::string::npos) {
                    out_addr = acquire_temp_float(temp_idx);
                    node_temp_type[name] = TempType::Float;
                } else if (out_type.find("int") != std::string::npos) {
                    out_addr = acquire_temp_int(temp_idx);
                    node_temp_type[name] = TempType::Int;
                } else {
                    out_addr = acquire_temp_bool(temp_idx);
                    node_temp_type[name] = TempType::Bool;
                }
                node_temp_idx[name] = temp_idx;
            }
            element_outputs[name] = out_addr;
            args.push_back(out_addr);

            push_block(static_cast<uint8_t>(custom_def->function_id), args,
                       static_cast<uint8_t>(n_in), 1);
            continue;
        }

        if (custom_def && !custom_def->inputs.empty()) {
            in0_src = find_port({to_lower_str(custom_def->inputs[0].name), "input_0", "in", "a"});
            if (custom_def->inputs.size() > 1) {
                in1_src = find_port({to_lower_str(custom_def->inputs[1].name), "input_1", "b"});
            }
        } else if (opcode == LE_OP_SR || opcode == LE_OP_RS) {
            in0_src = find_port({"s", "set", "input_0", "in", "a"});
            in1_src = find_port({"r", "reset", "input_1", "b"});
        } else if (opcode == LE_OP_TOTALIZER || opcode == LE_OP_MIN_MAX_HOLD || opcode == LE_OP_ZERO_CROSSING) {
            in0_src = find_port({"a", "in_a", "in", "input", "input_0", "x", "rate"});
            in1_src = find_port({"b", "in_b", "input_1", "reset", "rst"});
        } else if (is_counter_op) {
            /* count up / count down primary trigger, plus reset */
            in0_src = find_port({"cu", "cd", "count_up", "count_down", "cnt", "in", "a", "input", "input_0", "pv"});
            in1_src = find_port({"r", "rst", "reset", "b", "input_1"});
        } else if (opcode == LE_OP_RTRIG || opcode == LE_OP_FTRIG) {
            in0_src = find_port({"clk", "i", "in", "a", "input", "input_0"});
            in1_src = find_port({"h", "hist", "history", "b", "prev"});
        } else {
            in0_src = find_port({"a", "in_a", "in", "input", "input_0", "x", "pv"});
            in1_src = find_port({"b", "in_b", "input_1", "alpha"});
        }

        bool is_cmp = (opcode >= LE_OP_CMP_GT && opcode <= LE_OP_CMP_NE);
        bool has_bool_b = (opcode == LE_OP_TOTALIZER || opcode == LE_OP_MIN_MAX_HOLD || opcode == LE_OP_ZERO_CROSSING);
        uint16_t default_a = (is_float_op || is_cmp) ? LE_CONST_ZERO_F : (is_complex_op ? LE_CONST_ZERO_C : LE_CONST_FALSE);
        uint16_t default_b = (is_float_op && !has_bool_b) ? LE_CONST_ZERO_F : (is_complex_op ? LE_ADDR_UNUSED : LE_ADDR_UNUSED);

        uint16_t in_a = in0_src.empty() ? default_a : src_address(in0_src,
                      source_port_of(name, !used_keys.empty() ? used_keys[0] : ""), default_a);
        uint16_t in_b = in1_src.empty() ? default_b : src_address(in1_src,
                      source_port_of(name, used_keys.size() > 1 ? used_keys[1] : ""), default_b);

        consume_input(in0_src);
        consume_input(in1_src);

        uint16_t out_addr = LE_ADDR_UNUSED;
        if (!opt_node.direct_dest_node.empty()) {
            if (dout_map.find(opt_node.direct_dest_node) != dout_map.end()) {
                out_addr = dout_map[opt_node.direct_dest_node];
            } else if (element_outputs.find(opt_node.direct_dest_node) != element_outputs.end()) {
                out_addr = element_outputs[opt_node.direct_dest_node];
            }
        }

        if (out_addr == LE_ADDR_UNUSED) {
            if (is_timer_op) {
                out_addr = allocate_timer();
                state_descs[LE_BLK_TIMER]++;
                le_timer_state_t tst{};
                tst.preset_ms = (uint32_t)el.get("preset_ms").as_float(
                    prop(el, "preset", 0.0f));
                append_state(LE_BLK_TIMER, &tst, sizeof(tst));
            } else if (is_counter_op) {
                out_addr = allocate_counter();
                state_descs[LE_BLK_COUNTER]++;
                le_counter_state_t cst{};
                cst.preset = (int32_t)prop(el, "preset", 0.0f);
                append_state(LE_BLK_COUNTER, &cst, sizeof(cst));
            } else if (is_float_op) {
                int temp_idx = -1;
                out_addr = acquire_temp_float(temp_idx);
                node_temp_idx[name] = temp_idx;
                node_temp_type[name] = TempType::Float;
            } else if (is_complex_op) {
                int temp_idx = -1;
                out_addr = acquire_temp_complex(temp_idx);
                node_temp_idx[name] = temp_idx;
                node_temp_type[name] = TempType::Complex;
            } else if (is_int_op) {
                int temp_idx = -1;
                out_addr = acquire_temp_int(temp_idx);
                node_temp_idx[name] = temp_idx;
                node_temp_type[name] = TempType::Int;
            } else {
                int temp_idx = -1;
                out_addr = acquire_temp_bool(temp_idx);
                node_temp_idx[name] = temp_idx;
                node_temp_type[name] = TempType::Bool;
            }
        }
        element_outputs[name] = out_addr;

        le_instruction_t inst;
        inst.opcode = opcode;
        inst.modifier = modifier;
        inst.in_a = in_a;
        inst.in_b = in_b;
        inst.out = out_addr;
        instructions.push_back(inst);

        if (ref_counts[name] <= 0 && node_temp_idx.find(name) != node_temp_idx.end()) {
            if (is_float_op) release_temp_float(node_temp_idx[name]);
            else if (is_complex_op) release_temp_complex(node_temp_idx[name]);
            else if (is_int_op) release_temp_int(node_temp_idx[name]);
            else if (!is_timer_op && !is_counter_op) release_temp_bool(node_temp_idx[name]);
            node_temp_idx.erase(name);
            node_temp_type.erase(name);
        }
    }

    // Step 5: Pack Binary Payload
    size_t payload_bytes = instructions.size() * sizeof(le_instruction_t);
    std::vector<uint8_t> payload_vec(payload_bytes);
    if (payload_bytes > 0) {
        std::memcpy(payload_vec.data(), instructions.data(), payload_bytes);
    }

    // Append the variable-arity block table after the instructions, using the
    // on-disk layout expected by the loader and VM (le_block_desc_t header
    // followed by its uint16 operand addresses).
    std::vector<uint8_t> block_bytes;
    for (const BlockCall& bc : block_calls) {
        le_block_desc_t d;
        d.in_count = bc.in_count;
        d.out_count = bc.out_count;
        d.flags = 0;
        d.reserved = 0;
        const uint8_t* dh = (const uint8_t*)&d;
        for (int bi = 0; bi < (int)sizeof(d); bi++) block_bytes.push_back(dh[bi]);
        for (uint16_t a : bc.args) {
            block_bytes.push_back((uint8_t)(a & 0xFF));
            block_bytes.push_back((uint8_t)((a >> 8) & 0xFF));
        }
    }
    /* Append the state-directive table after the block table. Each group lists a
     * kind, an instance count, and the struct byte size so the loader can reserve
     * heap slices and bind pointers at load time. */
    auto state_kind_size = [&](uint8_t kind) -> uint16_t {
        switch (kind) {
            case LE_BLK_TIMER: return (uint16_t)sizeof(le_timer_state_t);
            case LE_BLK_COUNTER: return (uint16_t)sizeof(le_counter_state_t);
            case LE_BLK_LPF: return (uint16_t)sizeof(le_lpf_state_t);
            case LE_BLK_BIQUAD: return (uint16_t)sizeof(le_biquad_state_t);
            case LE_BLK_MOVING_AVG: return (uint16_t)sizeof(le_moving_avg_state_t);
            case LE_BLK_RATE_LIMITER: return (uint16_t)sizeof(le_rate_limiter_state_t);
            case LE_BLK_DEADBAND: return (uint16_t)sizeof(le_deadband_state_t);
            case LE_BLK_WASHOUT: return (uint16_t)sizeof(le_washout_state_t);
            case LE_BLK_PEAK: return (uint16_t)sizeof(le_peak_state_t);
            case LE_BLK_RMS: return (uint16_t)sizeof(le_rms_state_t);
            case LE_BLK_MEDIAN: return (uint16_t)sizeof(le_median_state_t);
            case LE_BLK_DERIVATIVE: return (uint16_t)sizeof(le_derivative_state_t);
            case LE_BLK_ZERO_CROSSING: return (uint16_t)sizeof(le_zero_crossing_state_t);
            case LE_BLK_LUT_1D: return (uint16_t)sizeof(le_lut_1d_state_t);
            case LE_BLK_TOTALIZER: return (uint16_t)sizeof(le_totalizer_state_t);
            case LE_BLK_MIN_MAX_HOLD: return (uint16_t)sizeof(le_min_max_hold_state_t);
            case LE_BLK_SCALER: return (uint16_t)sizeof(le_scale_state_t);
            case LE_BLK_PID: return (uint16_t)sizeof(le_pid_state_t);
            case LE_BLK_OVERCURRENT: return (uint16_t)sizeof(le_overcurrent_state_t);
#if LE_ENABLE_PROTECTION
            case LE_BLK_PHASOR: return (uint16_t)sizeof(le_phasor_state_t);
            case LE_BLK_SYMCOMP: return (uint16_t)sizeof(le_symcomp_state_t);
            case LE_BLK_21: return (uint16_t)sizeof(le_dist21_state_t);
            case LE_BLK_DIFF_87: return (uint16_t)sizeof(le_diff87_state_t);
            case LE_BLK_PHASE_COMP: return (uint16_t)sizeof(le_comp33_state_t);
#endif
            case LE_BLK_I2C: return (uint16_t)sizeof(le_i2c_device_state_t);
            case LE_BLK_SPI: return (uint16_t)sizeof(le_spi_device_state_t);
            default: return 0;
        }
    };
    /* Build the state-directive table and the preconfigured state image.
     * Groups are emitted in ascending kind order; each group's blocks are
     * concatenated (instance order). The loader copies the whole image to RAM
     * and uses the directives to compute per-kind byte offsets. Defaults and
     * all circuit properties are already baked into these bytes. */
    std::vector<uint8_t> state_bytes;   /* the state-desc (kind/count/size) table */
    std::vector<uint8_t> state_image;   /* the preconfigured state struct bytes */
    int state_records = 0;
    for (int k = 1; k < LE_BLK_LAST; k++) {
        uint8_t kind = (uint8_t)k;
        auto sit = state_instances.find(kind);
        if (sit == state_instances.end() || sit->second.empty()) continue;
        uint16_t sz = state_kind_size(kind);
        if (sz == 0) continue;
        int cnt = (int)sit->second.size();
        if (cnt > 255) cnt = 255;
        le_state_desc_t sd;
        sd.kind = kind; sd.count = (uint8_t)cnt; sd.size = sz;
        const uint8_t* sh = (const uint8_t*)&sd;
        for (int bi = 0; bi < (int)sizeof(sd); bi++) state_bytes.push_back(sh[bi]);
        state_records++;
        for (size_t i = 0; i < sit->second.size(); i++) {
            const std::vector<uint8_t>& blob = sit->second[i];
            if ((int)blob.size() != (int)sz) continue; /* defensive */
            for (uint8_t b : blob) state_image.push_back(b);
        }
    }
    /* Append the user-declared alias table after the state image. */
    std::vector<uint8_t> alias_bytes;
    for (const le_alias_t& a : alias_list) {
        const uint8_t* ab = (const uint8_t*)&a;
        for (size_t bi = 0; bi < sizeof(a); bi++) alias_bytes.push_back(ab[bi]);
    }

    size_t total_payload = payload_bytes + block_bytes.size() + state_bytes.size() + state_image.size() + alias_bytes.size();

    std::vector<uint8_t> payload_all(total_payload);
    if (payload_bytes > 0) {
        std::memcpy(payload_all.data(), payload_vec.data(), payload_bytes);
    }
    if (!block_bytes.empty()) {
        std::memcpy(payload_all.data() + payload_bytes, block_bytes.data(), block_bytes.size());
    }
    if (!state_bytes.empty()) {
        std::memcpy(payload_all.data() + payload_bytes + block_bytes.size(), state_bytes.data(), state_bytes.size());
    }
    if (!state_image.empty()) {
        std::memcpy(payload_all.data() + payload_bytes + block_bytes.size() + state_bytes.size(),
                    state_image.data(), state_image.size());
    }
    if (!alias_bytes.empty()) {
        std::memcpy(payload_all.data() + payload_bytes + block_bytes.size() + state_bytes.size() + state_image.size(),
                    alias_bytes.data(), alias_bytes.size());
    }

    uint32_t crc = compute_crc32(payload_all.data(), total_payload);

    int total_bool_regs = m_user_bool_count + m_peak_temp_bool;
    int total_float_regs = m_user_float_count + m_peak_temp_float;
    int total_complex_regs = m_user_complex_count + m_peak_temp_complex;
    int total_int_regs = m_user_int_count + m_peak_temp_int;

    le_header_t header;
    std::memset(&header, 0, sizeof(header));
    header.magic = LE_BIN_MAGIC;
    header.version = LE_BIN_VERSION;
    header.flags = LE_FLAG_AUTOSTART;
    header.instruction_count = static_cast<uint16_t>(instructions.size());
    header.digital_in_count = static_cast<uint16_t>(din_map.size());
    header.digital_out_count = static_cast<uint16_t>(dout_map.size());
    header.bool_reg_count = static_cast<uint16_t>(total_bool_regs);
    header.float_reg_count = static_cast<uint16_t>(total_float_regs);
    header.complex_reg_count = static_cast<uint16_t>(total_complex_regs);
    header.block_count = static_cast<uint16_t>(block_calls.size());
    header.state_desc_count = static_cast<uint16_t>(state_records);
    header.state_img_len = static_cast<uint32_t>(state_image.size());
    header.alias_count = static_cast<uint16_t>(alias_list.size());
    header.crc32 = crc;

    size_t total_bin_size = sizeof(le_header_t) + total_payload;
    std::vector<uint8_t> full_bin(total_bin_size);
    std::memcpy(full_bin.data(), &header, sizeof(le_header_t));
    if (total_payload > 0) {
        std::memcpy(full_bin.data() + sizeof(le_header_t), payload_all.data(), total_payload);
    }

    // Step 6: Validate against Board Profile Limits
    bool board_valid = true;
    std::vector<std::string> validation_errors;

    if (has_board) {
        std::string dev_name = board_doc.get("device").get("name").as_string("Target Board");
        const auto& limits = board_doc.get("limits");
        const auto& features = board_doc.get("features");

        int max_din = limits.get("digital_inputs").as_int(64);
        if (static_cast<int>(din_map.size()) > max_din) {
            board_valid = false;
            validation_errors.push_back("Board Validation Error: Circuit uses " + std::to_string(din_map.size()) +
                " digital inputs (%I), but target board '" + dev_name + "' only supports up to " + std::to_string(max_din) + ".");
        }

        int max_dout = limits.get("digital_outputs").as_int(64);
        if (static_cast<int>(dout_map.size()) > max_dout) {
            board_valid = false;
            validation_errors.push_back("Board Validation Error: Circuit uses " + std::to_string(dout_map.size()) +
                " digital outputs (%Q), but target board '" + dev_name + "' only supports up to " + std::to_string(max_dout) + ".");
        }

        int max_ain = limits.get("analog_inputs").as_int(16);
        if (static_cast<int>(ain_map.size()) > max_ain) {
            board_valid = false;
            validation_errors.push_back("Board Validation Error: Analog Inputs exceed limit: " + std::to_string(ain_map.size()) +
                " used, " + std::to_string(max_ain) + " max allowed on " + dev_name + ".");
        }

        int max_coils = limits.get("coils").as_int(limits.get("bool_registers").as_int(256));
        if (total_bool_regs > max_coils) {
            board_valid = false;
            validation_errors.push_back("Board Validation Error: Circuit uses " + std::to_string(total_bool_regs) +
                " internal boolean registers (%M), but target board '" + dev_name + "' only supports up to " + std::to_string(max_coils) + ".");
        }

        int max_floats = limits.get("floats").as_int(limits.get("float_registers").as_int(128));
        if (total_float_regs > max_floats) {
            board_valid = false;
            validation_errors.push_back("Board Validation Error: Circuit uses " + std::to_string(total_float_regs) +
                " float registers (%R), but target board '" + dev_name + "' only supports up to " + std::to_string(max_floats) + ".");
        }

        int workspace_bytes = limits.get("workspace_bytes").as_int(LE_STATE_WORKSPACE_BYTES);
        int total_state_bytes = 0;
        for (int k = 1; k < LE_BLK_LAST; k++) {
            auto sit = state_descs.find((uint8_t)k);
            if (sit == state_descs.end() || sit->second <= 0) continue;
            uint16_t sz = state_kind_size((uint8_t)k);
            if (sz == 0) continue;
            total_state_bytes += (int)sz * sit->second;
        }
        if (total_state_bytes > workspace_bytes) {
            board_valid = false;
            validation_errors.push_back("Board Validation Error: Circuit state requires " + std::to_string(total_state_bytes) +
                " bytes, but target board '" + dev_name + "' has a " + std::to_string(workspace_bytes) + "-byte state workspace.");
        }

        int slot_size = limits.get("slot_size_bytes").as_int(2048);
        if (static_cast<int>(total_bin_size) > slot_size) {
            board_valid = false;
            validation_errors.push_back("Board Validation Error: Compiled binary size (" + std::to_string(total_bin_size) +
                " bytes) exceeds slot storage capacity (" + std::to_string(slot_size) + " bytes) of target board '" + dev_name + "'.");
        }

        bool prot_enabled = features.get("protection").as_bool(true);
        if (uses_protection && !prot_enabled) {
            board_valid = false;
            validation_errors.push_back("Board Validation Error: Circuit uses Protection & Control features, but target board '" + dev_name + "' has protection disabled.");
        }

        bool serial_enabled = features.get("serial_bus").as_bool(true);
        if (uses_serial_bus && !serial_enabled) {
            board_valid = false;
            validation_errors.push_back("Board Validation Error: Circuit uses Serial Bus features, but target board '" + dev_name + "' has serial bus disabled.");
        }

        bool dsp_enabled = features.get("dsp").as_bool(true);
        if (uses_dsp && !dsp_enabled) {
            board_valid = false;
            validation_errors.push_back("Board Validation Error: Circuit uses Digital Signal Processing (DSP) features, but target board '" + dev_name + "' has DSP disabled.");
        }
    }

    // Step 7: Populate out_result
    out_result->success = board_valid ? 1 : 0;
    out_result->instruction_count = static_cast<int>(instructions.size());
    out_result->din_count = static_cast<int>(din_map.size());
    out_result->dout_count = static_cast<int>(dout_map.size());
    out_result->ain_count = static_cast<int>(ain_map.size());
    out_result->bool_reg_count = total_bool_regs;
    out_result->float_count = total_float_regs;
    out_result->complex_count = total_complex_regs;
    out_result->int_count = total_int_regs;
    out_result->timer_count = m_timer_counter;
    out_result->counter_count = m_counter_counter;
    out_result->alias_count = static_cast<int>(alias_list.size());
    out_result->crc32 = crc;
    out_result->user_bool_count = m_user_bool_count;
    out_result->temp_bool_count = m_peak_temp_bool;
    out_result->user_float_count = m_user_float_count;
    out_result->temp_float_count = m_peak_temp_float;
    out_result->user_complex_count = m_user_complex_count;
    out_result->temp_complex_count = m_peak_temp_complex;
    out_result->user_int_count = m_user_int_count;
    out_result->temp_int_count = m_peak_temp_int;
    out_result->eliminated_instructions = stats.eliminated_instructions;
    out_result->eliminated_registers = stats.eliminated_registers;

    // Allocate binary
    out_result->binary_size = total_bin_size;
    out_result->binary_data = (uint8_t*)std::malloc(total_bin_size);
    std::memcpy(out_result->binary_data, full_bin.data(), total_bin_size);

    // Generate disassembly
    std::string disasm = disassemble(full_bin.data(), total_bin_size, m_user_bool_count, m_user_float_count, m_user_int_count);
    out_result->disassembly_text = (char*)std::malloc(disasm.size() + 1);
    std::strcpy(out_result->disassembly_text, disasm.c_str());

    // Generate C header
    std::string c_hdr = export_c_header(full_bin.data(), total_bin_size, "le_default_program", custom_headers, scaler_list);
    out_result->c_header_code = (char*)std::malloc(c_hdr.size() + 1);
    std::strcpy(out_result->c_header_code, c_hdr.c_str());

    // Re-serialize circuit JSON
    std::string canon_json = circuit_doc.to_string(2);
    out_result->json_circuit = (char*)std::malloc(canon_json.size() + 1);
    std::strcpy(out_result->json_circuit, canon_json.c_str());

    // Warnings and Errors
    if (!validation_errors.empty()) {
        std::ostringstream ss;
        for (size_t i = 0; i < validation_errors.size(); ++i) {
            ss << validation_errors[i];
            if (i + 1 < validation_errors.size()) ss << "\n";
        }
        std::string err_s = ss.str();
        out_result->error_message = (char*)std::malloc(err_s.size() + 1);
        std::strcpy(out_result->error_message, err_s.c_str());
    }

    if (!warning_list.empty()) {
        std::ostringstream ss;
        for (size_t i = 0; i < warning_list.size(); ++i) {
            ss << warning_list[i];
            if (i + 1 < warning_list.size()) ss << "\n";
        }
        std::string warn_s = ss.str();
        out_result->warnings = (char*)std::malloc(warn_s.size() + 1);
        std::strcpy(out_result->warnings, warn_s.c_str());
    }

    return board_valid ? 0 : 1;
}

} // namespace LogicElements

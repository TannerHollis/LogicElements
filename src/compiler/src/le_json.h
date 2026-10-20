/**
 * @file le_json.h
 * @brief Lightweight, zero-dependency JSON parser and serializer for circuit schematics.
 *
 * This header defines a simple Document Object Model (DOM) JSON parser tailored
 * for parsing LogicElements circuit schematics, board configurations (.leconfig),
 * and exporting formatted JSON representations.
 *
 * ## Features & Guarantees
 * - **Zero External Dependencies**: Operates exclusively with standard C++17 library facilities.
 * - **Locale Independence**: Numeric parsing does not depend on host `LC_NUMERIC` settings,
 *   preventing decimal separator misinterpretations in non-US locales.
 * - **Stack Exhaustion Protection**: Imposes an internal nesting recursion limit to guard against
 *   maliciously deep payloads.
 * - **Memory Safety**: Value semantics with clear standard container ownership (`std::vector`, `std::map`).
 * - **Thread Safety**: Independent `JsonValue` instances can be parsed, read, and modified concurrently
 *   across separate threads. Const read operations on shared instances are thread-safe.
 */

#ifndef LE_JSON_H
#define LE_JSON_H

#include <cctype>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace LogicElements {

/**
 * @brief Fundamental data type tags supported by JSON (RFC 8259).
 */
enum class JsonType {
    Null,     /**< Represents JSON null literal (`null`). */
    Boolean,  /**< Represents JSON boolean literal (`true` or `false`). */
    Number,   /**< Represents 64-bit IEEE-754 floating-point or integer value. */
    String,   /**< Represents UTF-8 encoded text string. */
    Array,    /**< Ordered sequence of @ref JsonValue elements. */
    Object    /**< Key-value collection mapping string keys to @ref JsonValue elements. */
};

/**
 * @brief Represents an individual JSON node in the Document Object Model.
 *
 * `JsonValue` provides standard value semantics (copyable, movable) and holds one
 * active variant corresponding to its @ref JsonType tag.
 */
class JsonValue {
public:
    JsonType type = JsonType::Null;          /**< The active type tag of this JSON node. */
    bool bool_val = false;                   /**< Boolean payload when @ref type is @ref JsonType::Boolean. */
    double num_val = 0.0;                    /**< Numerical payload when @ref type is @ref JsonType::Number. */
    std::string str_val;                     /**< String payload when @ref type is @ref JsonType::String. */
    std::vector<JsonValue> arr_val;          /**< Array payload when @ref type is @ref JsonType::Array. */
    std::map<std::string, JsonValue> obj_val;/**< Key-value map when @ref type is @ref JsonType::Object. */

    /**
     * @brief Constructs a JSON Null value.
     */
    JsonValue() : type(JsonType::Null) {}

    /**
     * @brief Constructs a JSON Boolean value.
     * @param[in] b Boolean truth value.
     */
    JsonValue(bool b) : type(JsonType::Boolean), bool_val(b) {}

    /**
     * @brief Constructs a JSON Number value from an integer.
     * @param[in] i 32-bit signed integer.
     */
    JsonValue(int i) : type(JsonType::Number), num_val(static_cast<double>(i)) {}

    /**
     * @brief Constructs a JSON Number value from a 64-bit integer.
     * @param[in] i 64-bit signed integer.
     */
    JsonValue(int64_t i) : type(JsonType::Number), num_val(static_cast<double>(i)) {}

    /**
     * @brief Constructs a JSON Number value from a double-precision float.
     * @param[in] d Double-precision floating point number.
     */
    JsonValue(double d) : type(JsonType::Number), num_val(d) {}

    /**
     * @brief Constructs a JSON String value from a null-terminated C string.
     * @param[in] s Pointer to a null-terminated C string, or `nullptr`.
     */
    JsonValue(const char* s) : type(JsonType::String), str_val(s ? s : "") {}

    /**
     * @brief Constructs a JSON String value from a standard string.
     * @param[in] s String content.
     */
    JsonValue(const std::string& s) : type(JsonType::String), str_val(s) {}

    /**
     * @brief Constructs a JSON Array value.
     * @param[in] a Vector of @ref JsonValue elements.
     */
    JsonValue(const std::vector<JsonValue>& a) : type(JsonType::Array), arr_val(a) {}

    /**
     * @brief Constructs a JSON Object value.
     * @param[in] o Key-value map of string identifiers to @ref JsonValue elements.
     */
    JsonValue(const std::map<std::string, JsonValue>& o) : type(JsonType::Object), obj_val(o) {}

    /** @brief Checks if this value is null. @return True if null; false otherwise. */
    bool is_null() const noexcept { return type == JsonType::Null; }

    /** @brief Checks if this value is a boolean. @return True if boolean; false otherwise. */
    bool is_bool() const noexcept { return type == JsonType::Boolean; }

    /** @brief Checks if this value is a number. @return True if numeric; false otherwise. */
    bool is_number() const noexcept { return type == JsonType::Number; }

    /** @brief Checks if this value is a string. @return True if string; false otherwise. */
    bool is_string() const noexcept { return type == JsonType::String; }

    /** @brief Checks if this value is an array. @return True if array; false otherwise. */
    bool is_array() const noexcept { return type == JsonType::Array; }

    /** @brief Checks if this value is an object. @return True if object; false otherwise. */
    bool is_object() const noexcept { return type == JsonType::Object; }

    /**
     * @brief Converts or retrieves the value as a boolean.
     *
     * If the value is boolean, returns its direct value. If numeric, returns true if non-zero.
     * If string, returns true for "true", "1", or "True".
     *
     * @param[in] def Fallback default value if the node cannot be interpreted as boolean.
     * @return Interpreted boolean value.
     */
    bool as_bool(bool def = false) const noexcept {
        if (type == JsonType::Boolean) return bool_val;
        if (type == JsonType::Number) return num_val != 0.0;
        if (type == JsonType::String) {
            return (str_val == "true" || str_val == "1" || str_val == "True");
        }
        return def;
    }

    /**
     * @brief Converts or retrieves the value as a 32-bit signed integer.
     *
     * @param[in] def Fallback default value if the node cannot be converted to integer.
     * @return Interpreted integer value.
     */
    int as_int(int def = 0) const noexcept {
        if (type == JsonType::Number) return static_cast<int>(num_val);
        if (type == JsonType::String) {
            double parsed_val = 0.0;
            if (parse_locale_independent_double(str_val, parsed_val)) {
                return static_cast<int>(parsed_val);
            }
            return def;
        }
        if (type == JsonType::Boolean) return bool_val ? 1 : 0;
        return def;
    }

    /**
     * @brief Converts or retrieves the value as a double-precision float.
     *
     * @param[in] def Fallback default value if the node cannot be converted to double.
     * @return Interpreted double value.
     */
    double as_double(double def = 0.0) const noexcept {
        if (type == JsonType::Number) return num_val;
        if (type == JsonType::String) {
            double parsed_val = 0.0;
            if (parse_locale_independent_double(str_val, parsed_val)) {
                return parsed_val;
            }
            return def;
        }
        if (type == JsonType::Boolean) return bool_val ? 1.0 : 0.0;
        return def;
    }

    /**
     * @brief Converts or retrieves the value as a single-precision float.
     *
     * @param[in] def Fallback default value if the node cannot be converted to float.
     * @return Interpreted float value.
     */
    float as_float(float def = 0.0f) const noexcept {
        return static_cast<float>(as_double(static_cast<double>(def)));
    }

    /**
     * @brief Retrieves the string value.
     *
     * @param[in] def Fallback string returned if the node is not of string type.
     * @return Const reference to string payload, or @p def.
     */
    const std::string& as_string(const std::string& def = "") const noexcept {
        if (type == JsonType::String) return str_val;
        return def;
    }

    /**
     * @brief Retrieves the elements of an array.
     *
     * @return Reference to vector of array items, or a static empty vector if not an array.
     */
    const std::vector<JsonValue>& as_array() const noexcept {
        static const std::vector<JsonValue> empty_arr;
        return (type == JsonType::Array) ? arr_val : empty_arr;
    }

    /**
     * @brief Retrieves the key-value members of an object.
     *
     * @return Reference to object map, or a static empty map if not an object.
     */
    const std::map<std::string, JsonValue>& as_object() const noexcept {
        static const std::map<std::string, JsonValue> empty_obj;
        return (type == JsonType::Object) ? obj_val : empty_obj;
    }

    /**
     * @brief Determines whether an object contains a specified property key.
     *
     * @param[in] key Property name to query.
     * @return True if the node is an object and contains @p key; false otherwise.
     */
    bool contains(const std::string& key) const noexcept {
        if (type != JsonType::Object) return false;
        return obj_val.find(key) != obj_val.end();
    }

    /**
     * @brief Safely retrieves a constant reference to a member property by key.
     *
     * @param[in] key Property name identifier.
     * @return Const reference to the child node, or a static Null node if missing.
     */
    const JsonValue& get(const std::string& key) const noexcept {
        static const JsonValue null_val;
        if (type != JsonType::Object) return null_val;
        auto it = obj_val.find(key);
        return (it != obj_val.end()) ? it->second : null_val;
    }

    /**
     * @brief Read-only object subscript operator.
     *
     * @param[in] key Property name identifier.
     * @return Const reference to the child node, or a static Null node if missing.
     */
    const JsonValue& operator[](const std::string& key) const noexcept {
        return get(key);
    }

    /**
     * @brief Mutable object subscript operator.
     *
     * If this instance is not already an Object, it is reinitialized as an empty Object.
     *
     * @param[in] key Property name identifier.
     * @return Mutable reference to the child node.
     */
    JsonValue& operator[](const std::string& key) {
        if (type != JsonType::Object) {
            type = JsonType::Object;
            obj_val.clear();
        }
        return obj_val[key];
    }

    /**
     * @brief Inserts or replaces a property key-value pair in an object.
     *
     * @param[in] key Property name identifier.
     * @param[in] val Value to associate with the key.
     */
    void set(const std::string& key, const JsonValue& val) {
        if (type != JsonType::Object) {
            type = JsonType::Object;
            obj_val.clear();
        }
        obj_val[key] = val;
    }

    /**
     * @brief Appends an element to the end of an array.
     *
     * If this instance is not already an Array, it is reinitialized as an empty Array.
     *
     * @param[in] val Element to append.
     */
    void push_back(const JsonValue& val) {
        if (type != JsonType::Array) {
            type = JsonType::Array;
            arr_val.clear();
        }
        arr_val.push_back(val);
    }

    /**
     * @brief Returns the number of elements in an Array or Object.
     *
     * @return Element count, or `0` for scalar/null types.
     */
    size_t size() const noexcept {
        if (type == JsonType::Array) return arr_val.size();
        if (type == JsonType::Object) return obj_val.size();
        return 0;
    }

    /**
     * @brief Parses a UTF-8 JSON document string into a DOM tree.
     *
     * Enforces a maximum recursion depth limit (128) to prevent stack exhaustion.
     * Numeric parsing is completely locale-independent.
     *
     * @param[in] src UTF-8 JSON source string.
     * @param[out] err Optional output string receiving the error description on failure.
     * @return Parsed @ref JsonValue tree on success; or a Null @ref JsonValue on error.
     */
    static JsonValue parse(const std::string& src, std::string* err = nullptr) {
        size_t idx = 0;
        skip_ws(src, idx);
        try {
            JsonValue val = parse_val(src, idx, 0);
            skip_ws(src, idx);
            if (idx < src.size()) {
                throw std::runtime_error("Unexpected trailing characters after JSON root");
            }
            return val;
        } catch (const std::exception& e) {
            if (err) *err = e.what();
            return JsonValue();
        }
    }

    /**
     * @brief Serializes this DOM node and its descendants into formatted JSON text.
     *
     * @param[in] indent Number of spaces per indentation level (`0` for compact single-line).
     * @return Formatted JSON text string.
     */
    std::string dump(int indent = 0) const {
        std::ostringstream ss;
        serialize(ss, indent, 0);
        return ss.str();
    }

    /**
     * @brief Alias for @ref dump.
     *
     * @param[in] indent Number of spaces per indentation level.
     * @return Formatted JSON text string.
     */
    std::string to_string(int indent = 0) const {
        return dump(indent);
    }

private:
    static constexpr size_t kMaxJsonRecursionDepth = 128;

    /**
     * @brief Parses a double-precision float without depending on C runtime locale.
     */
    static bool parse_locale_independent_double(std::string_view s, double& out_val) noexcept {
        if (s.empty()) return false;
        size_t i = 0;
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
        if (i >= s.size()) return false;

        bool negative = false;
        if (s[i] == '-') {
            negative = true;
            i++;
        } else if (s[i] == '+') {
            i++;
        }

        if (i >= s.size()) return false;

        double integer_part = 0.0;
        bool has_digits = false;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
            integer_part = integer_part * 10.0 + (s[i] - '0');
            has_digits = true;
            i++;
        }

        double fraction_part = 0.0;
        double fraction_div = 1.0;
        if (i < s.size() && s[i] == '.') {
            i++;
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
                fraction_part = fraction_part * 10.0 + (s[i] - '0');
                fraction_div *= 10.0;
                has_digits = true;
                i++;
            }
        }

        if (!has_digits) return false;

        double result = integer_part + (fraction_part / fraction_div);

        // Check for optional exponent part ('e' or 'E')
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            i++;
            if (i >= s.size()) return false;
            bool exp_negative = false;
            if (s[i] == '-') {
                exp_negative = true;
                i++;
            } else if (s[i] == '+') {
                i++;
            }
            int exponent = 0;
            bool has_exp_digits = false;
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
                exponent = exponent * 10 + (s[i] - '0');
                has_exp_digits = true;
                i++;
            }
            if (!has_exp_digits) return false;
            double factor = std::pow(10.0, exp_negative ? -exponent : exponent);
            result *= factor;
        }

        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
        if (i < s.size()) return false; // Trailing unparsed content

        out_val = negative ? -result : result;
        return true;
    }

    static void skip_ws(const std::string& s, size_t& i) noexcept {
        while (i < s.size() && (std::isspace(static_cast<unsigned char>(s[i])) || s[i] == '\0')) {
            i++;
        }
    }

    static JsonValue parse_val(const std::string& s, size_t& i, size_t depth) {
        if (depth > kMaxJsonRecursionDepth) {
            throw std::runtime_error("Exceeded maximum JSON recursion depth limit (128)");
        }
        skip_ws(s, i);
        if (i >= s.size()) throw std::runtime_error("Unexpected end of JSON input");
        char c = s[i];
        if (c == '{') return parse_obj(s, i, depth + 1);
        if (c == '[') return parse_arr(s, i, depth + 1);
        if (c == '"') return parse_str(s, i);
        if (c == 't' || c == 'f') return parse_bool(s, i);
        if (c == 'n') return parse_null(s, i);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_num(s, i);
        throw std::runtime_error(std::string("Unexpected character in JSON: '") + c + "'");
    }

    static JsonValue parse_obj(const std::string& s, size_t& i, size_t depth) {
        i++; // skip '{'
        JsonValue val;
        val.type = JsonType::Object;
        while (true) {
            skip_ws(s, i);
            if (i >= s.size()) throw std::runtime_error("Unterminated object in JSON");
            if (s[i] == '}') { i++; break; }
            if (s[i] != '"') throw std::runtime_error("Expected string key in object");
            std::string key = parse_str(s, i).str_val;
            skip_ws(s, i);
            if (i >= s.size() || s[i] != ':') throw std::runtime_error("Expected ':' after object key");
            i++; // skip ':'
            val.obj_val[key] = parse_val(s, i, depth);
            skip_ws(s, i);
            if (i < s.size() && s[i] == ',') { i++; continue; }
            if (i < s.size() && s[i] == '}') { i++; break; }
            throw std::runtime_error("Expected ',' or '}' in object");
        }
        return val;
    }

    static JsonValue parse_arr(const std::string& s, size_t& i, size_t depth) {
        i++; // skip '['
        JsonValue val;
        val.type = JsonType::Array;
        while (true) {
            skip_ws(s, i);
            if (i >= s.size()) throw std::runtime_error("Unterminated array in JSON");
            if (s[i] == ']') { i++; break; }
            val.arr_val.push_back(parse_val(s, i, depth));
            skip_ws(s, i);
            if (i < s.size() && s[i] == ',') { i++; continue; }
            if (i < s.size() && s[i] == ']') { i++; break; }
            throw std::runtime_error("Expected ',' or ']' in array");
        }
        return val;
    }

    static JsonValue parse_str(const std::string& s, size_t& i) {
        i++; // skip initial quote
        std::string res;
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return JsonValue(res);
            if (c == '\\') {
                if (i >= s.size()) throw std::runtime_error("Unfinished escape sequence in string");
                char esc = s[i++];
                switch (esc) {
                    case '"':  res += '"'; break;
                    case '\\': res += '\\'; break;
                    case '/':  res += '/'; break;
                    case 'b':  res += '\b'; break;
                    case 'f':  res += '\f'; break;
                    case 'n':  res += '\n'; break;
                    case 'r':  res += '\r'; break;
                    case 't':  res += '\t'; break;
                    case 'u': {
                        if (i + 4 <= s.size()) {
                            uint32_t code_point = 0;
                            for (int k = 0; k < 4; ++k) {
                                char hc = s[i + k];
                                code_point <<= 4;
                                if (hc >= '0' && hc <= '9') code_point |= (hc - '0');
                                else if (hc >= 'a' && hc <= 'f') code_point |= (hc - 'a' + 10);
                                else if (hc >= 'A' && hc <= 'F') code_point |= (hc - 'A' + 10);
                                else code_point |= 0;
                            }
                            i += 4;
                            // Encode to UTF-8
                            if (code_point <= 0x7F) {
                                res += static_cast<char>(code_point);
                            } else if (code_point <= 0x7FF) {
                                res += static_cast<char>(0xC0 | ((code_point >> 6) & 0x1F));
                                res += static_cast<char>(0x80 | (code_point & 0x3F));
                            } else {
                                res += static_cast<char>(0xE0 | ((code_point >> 12) & 0x0F));
                                res += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
                                res += static_cast<char>(0x80 | (code_point & 0x3F));
                            }
                        } else {
                            res += '?';
                        }
                        break;
                    }
                    default: res += esc; break;
                }
            } else {
                res += c;
            }
        }
        throw std::runtime_error("Unclosed quote in string");
    }

    static JsonValue parse_num(const std::string& s, size_t& i) {
        size_t start = i;
        if (s[i] == '-') i++;
        while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) ||
                                s[i] == '.' || s[i] == 'e' || s[i] == 'E' ||
                                s[i] == '+' || s[i] == '-')) {
            i++;
        }
        std::string_view num_view(&s[start], i - start);
        double d = 0.0;
        if (!parse_locale_independent_double(num_view, d)) {
            throw std::runtime_error(std::string("Malformed numeric literal in JSON: '") + std::string(num_view) + "'");
        }
        return JsonValue(d);
    }

    static JsonValue parse_bool(const std::string& s, size_t& i) {
        if (s.compare(i, 4, "true") == 0) { i += 4; return JsonValue(true); }
        if (s.compare(i, 5, "false") == 0) { i += 5; return JsonValue(false); }
        throw std::runtime_error("Invalid boolean literal in JSON");
    }

    static JsonValue parse_null(const std::string& s, size_t& i) {
        if (s.compare(i, 4, "null") == 0) { i += 4; return JsonValue(); }
        throw std::runtime_error("Invalid null literal in JSON");
    }

    void serialize(std::ostream& os, int indent, int depth) const {
        std::string ind_str(indent > 0 ? depth * indent : 0, ' ');
        std::string ind_next(indent > 0 ? (depth + 1) * indent : 0, ' ');

        switch (type) {
            case JsonType::Null: os << "null"; break;
            case JsonType::Boolean: os << (bool_val ? "true" : "false"); break;
            case JsonType::Number: {
                if (num_val == std::floor(num_val) && !std::isinf(num_val) && std::fabs(num_val) < 1e15) {
                    os << static_cast<int64_t>(num_val);
                } else {
                    os << num_val;
                }
                break;
            }
            case JsonType::String: {
                os << '"';
                for (char c : str_val) {
                    switch (c) {
                        case '"':  os << "\\\""; break;
                        case '\\': os << "\\\\"; break;
                        case '\b': os << "\\b"; break;
                        case '\f': os << "\\f"; break;
                        case '\n': os << "\\n"; break;
                        case '\r': os << "\\r"; break;
                        case '\t': os << "\\t"; break;
                        default:   os << c; break;
                    }
                }
                os << '"';
                break;
            }
            case JsonType::Array: {
                if (arr_val.empty()) { os << "[]"; break; }
                os << '[';
                if (indent > 0) os << '\n';
                for (size_t k = 0; k < arr_val.size(); k++) {
                    if (indent > 0) os << ind_next;
                    arr_val[k].serialize(os, indent, depth + 1);
                    if (k + 1 < arr_val.size()) os << ',';
                    if (indent > 0) os << '\n';
                }
                if (indent > 0) os << ind_str;
                os << ']';
                break;
            }
            case JsonType::Object: {
                if (obj_val.empty()) { os << "{}"; break; }
                os << '{';
                if (indent > 0) os << '\n';
                size_t k = 0;
                for (auto it = obj_val.begin(); it != obj_val.end(); ++it, ++k) {
                    if (indent > 0) os << ind_next;
                    os << '"' << it->first << "\": ";
                    it->second.serialize(os, indent, depth + 1);
                    if (k + 1 < obj_val.size()) os << ',';
                    if (indent > 0) os << '\n';
                }
                if (indent > 0) os << ind_str;
                os << '}';
                break;
            }
        }
    }
};

} // namespace LogicElements

#endif /* LE_JSON_H */

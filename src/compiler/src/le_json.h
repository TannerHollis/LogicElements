#ifndef LE_JSON_H
#define LE_JSON_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <cmath>

namespace LogicElements {

enum class JsonType {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class JsonValue {
public:
    JsonType type = JsonType::Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::vector<JsonValue> arr_val;
    std::map<std::string, JsonValue> obj_val;

    JsonValue() : type(JsonType::Null) {}
    JsonValue(bool b) : type(JsonType::Boolean), bool_val(b) {}
    JsonValue(int i) : type(JsonType::Number), num_val(static_cast<double>(i)) {}
    JsonValue(int64_t i) : type(JsonType::Number), num_val(static_cast<double>(i)) {}
    JsonValue(double d) : type(JsonType::Number), num_val(d) {}
    JsonValue(const char* s) : type(JsonType::String), str_val(s ? s : "") {}
    JsonValue(const std::string& s) : type(JsonType::String), str_val(s) {}
    JsonValue(const std::vector<JsonValue>& a) : type(JsonType::Array), arr_val(a) {}
    JsonValue(const std::map<std::string, JsonValue>& o) : type(JsonType::Object), obj_val(o) {}

    bool is_null() const { return type == JsonType::Null; }
    bool is_bool() const { return type == JsonType::Boolean; }
    bool is_number() const { return type == JsonType::Number; }
    bool is_string() const { return type == JsonType::String; }
    bool is_array() const { return type == JsonType::Array; }
    bool is_object() const { return type == JsonType::Object; }

    bool as_bool(bool def = false) const {
        if (type == JsonType::Boolean) return bool_val;
        if (type == JsonType::Number) return num_val != 0.0;
        if (type == JsonType::String) return (str_val == "true" || str_val == "1" || str_val == "True");
        return def;
    }

    int as_int(int def = 0) const {
        if (type == JsonType::Number) return static_cast<int>(num_val);
        if (type == JsonType::String) {
            try { return std::stoi(str_val); } catch (...) { return def; }
        }
        if (type == JsonType::Boolean) return bool_val ? 1 : 0;
        return def;
    }

    double as_double(double def = 0.0) const {
        if (type == JsonType::Number) return num_val;
        if (type == JsonType::String) {
            try { return std::stod(str_val); } catch (...) { return def; }
        }
        if (type == JsonType::Boolean) return bool_val ? 1.0 : 0.0;
        return def;
    }

    float as_float(float def = 0.0f) const {
        return static_cast<float>(as_double(def));
    }

    const std::string& as_string(const std::string& def = "") const {
        if (type == JsonType::String) return str_val;
        return def;
    }

    const std::vector<JsonValue>& as_array() const {
        static const std::vector<JsonValue> empty_arr;
        return (type == JsonType::Array) ? arr_val : empty_arr;
    }

    const std::map<std::string, JsonValue>& as_object() const {
        static const std::map<std::string, JsonValue> empty_obj;
        return (type == JsonType::Object) ? obj_val : empty_obj;
    }

    bool contains(const std::string& key) const {
        if (type != JsonType::Object) return false;
        return obj_val.find(key) != obj_val.end();
    }

    const JsonValue& get(const std::string& key) const {
        static const JsonValue null_val;
        if (type != JsonType::Object) return null_val;
        auto it = obj_val.find(key);
        return (it != obj_val.end()) ? it->second : null_val;
    }

    const JsonValue& operator[](const std::string& key) const {
        return get(key);
    }

    JsonValue& operator[](const std::string& key) {
        if (type != JsonType::Object) {
            type = JsonType::Object;
            obj_val.clear();
        }
        return obj_val[key];
    }

    size_t size() const {
        if (type == JsonType::Array) return arr_val.size();
        if (type == JsonType::Object) return obj_val.size();
        return 0;
    }

    static JsonValue parse(const std::string& src, std::string* err = nullptr) {
        size_t idx = 0;
        skip_ws(src, idx);
        try {
            JsonValue val = parse_val(src, idx);
            skip_ws(src, idx);
            return val;
        } catch (const std::exception& e) {
            if (err) *err = e.what();
            return JsonValue();
        }
    }

    std::string dump(int indent = 0) const {
        std::ostringstream ss;
        serialize(ss, indent, 0);
        return ss.str();
    }

    std::string to_string(int indent = 0) const {
        return dump(indent);
    }

private:
    static void skip_ws(const std::string& s, size_t& i) {
        while (i < s.size() && (std::isspace(static_cast<unsigned char>(s[i])) || s[i] == '\0')) {
            i++;
        }
    }

    static JsonValue parse_val(const std::string& s, size_t& i) {
        skip_ws(s, i);
        if (i >= s.size()) throw std::runtime_error("Unexpected end of JSON input");
        char c = s[i];
        if (c == '{') return parse_obj(s, i);
        if (c == '[') return parse_arr(s, i);
        if (c == '"') return parse_str(s, i);
        if (c == 't' || c == 'f') return parse_bool(s, i);
        if (c == 'n') return parse_null(s, i);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_num(s, i);
        throw std::runtime_error(std::string("Unexpected character in JSON: '") + c + "'");
    }

    static JsonValue parse_obj(const std::string& s, size_t& i) {
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
            val.obj_val[key] = parse_val(s, i);
            skip_ws(s, i);
            if (i < s.size() && s[i] == ',') { i++; continue; }
            if (i < s.size() && s[i] == '}') { i++; break; }
            throw std::runtime_error("Expected ',' or '}' in object");
        }
        return val;
    }

    static JsonValue parse_arr(const std::string& s, size_t& i) {
        i++; // skip '['
        JsonValue val;
        val.type = JsonType::Array;
        while (true) {
            skip_ws(s, i);
            if (i >= s.size()) throw std::runtime_error("Unterminated array in JSON");
            if (s[i] == ']') { i++; break; }
            val.arr_val.push_back(parse_val(s, i));
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
                        // skip 4 hex digits for basic unicode
                        if (i + 4 <= s.size()) i += 4;
                        res += '?';
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
        while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) {
            i++;
        }
        std::string num_str = s.substr(start, i - start);
        double d = std::stod(num_str);
        JsonValue val(d);
        return val;
    }

    static JsonValue parse_bool(const std::string& s, size_t& i) {
        if (s.compare(i, 4, "true") == 0) { i += 4; return JsonValue(true); }
        if (s.compare(i, 5, "false") == 0) { i += 5; return JsonValue(false); }
        throw std::runtime_error("Invalid boolean literal");
    }

    static JsonValue parse_null(const std::string& s, size_t& i) {
        if (s.compare(i, 4, "null") == 0) { i += 4; return JsonValue(); }
        throw std::runtime_error("Invalid null literal");
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

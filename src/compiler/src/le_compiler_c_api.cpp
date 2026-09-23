#include "le_compiler.h"
#include "le_compiler_core.h"
#include "le_disasm_core.h"
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {

LE_COMPILER_API int le_compile_json_ex(
    const char* circuit_json,
    const char* board_profile_json,
    const le_compiler_options_t* options,
    le_compile_result_t* out_result)
{
    if (!out_result) return -1;
    if (!circuit_json) {
        std::memset(out_result, 0, sizeof(le_compile_result_t));
        out_result->success = 0;
        const char* err = "circuit_json parameter is null";
        out_result->error_message = (char*)std::malloc(std::strlen(err) + 1);
        std::strcpy(out_result->error_message, err);
        return -1;
    }

    std::string c_str = circuit_json;
    std::string b_str = board_profile_json ? board_profile_json : "";

    le_compiler_options_t opts = options ? *options : le_compiler_options_init_default();

    LogicElements::CompilerCore compiler;
    return compiler.compile_ex(c_str, b_str, opts, out_result);
}

LE_COMPILER_API int le_compile_json(
    const char* circuit_json,
    const char* board_profile_json,
    le_compile_result_t* out_result)
{
    le_compiler_options_t opts = le_compiler_options_init_default();
    return le_compile_json_ex(circuit_json, board_profile_json, &opts, out_result);
}

LE_COMPILER_API int le_disassemble_bin(
    const uint8_t* bin_data,
    size_t bin_len,
    int user_bool_count,
    int user_float_count,
    int user_int_count,
    const char* board_profile_json,
    char** out_disassembly,
    char** out_error_msg)
{
    if (out_error_msg) *out_error_msg = nullptr;
    if (!out_disassembly) return -1;
    *out_disassembly = nullptr;

    if (!bin_data || bin_len == 0) {
        if (out_error_msg) {
            const char* err = "Binary data is null or empty";
            *out_error_msg = (char*)std::malloc(std::strlen(err) + 1);
            std::strcpy(*out_error_msg, err);
        }
        return -1;
    }

    try {
        std::string board = board_profile_json ? board_profile_json : "";
        std::string dis = LogicElements::disassemble_binary(bin_data, bin_len, user_bool_count, user_float_count, user_int_count, board);
        *out_disassembly = (char*)std::malloc(dis.size() + 1);
        std::strcpy(*out_disassembly, dis.c_str());
        return 0;
    } catch (const std::exception& e) {
        if (out_error_msg) {
            std::string err = std::string("Disassembly error: ") + e.what();
            *out_error_msg = (char*)std::malloc(err.size() + 1);
            std::strcpy(*out_error_msg, err.c_str());
        }
        return -1;
    }
}

LE_COMPILER_API int le_export_c_header(
    const uint8_t* bin_data,
    size_t bin_len,
    const char* array_name,
    char** out_c_header)
{
    if (!out_c_header) return -1;
    *out_c_header = nullptr;

    if (!bin_data || bin_len == 0) return -1;

    std::string name = (array_name && *array_name) ? array_name : "le_default_program";
    std::string hdr = LogicElements::CompilerCore::export_c_header(bin_data, bin_len, name);

    *out_c_header = (char*)std::malloc(hdr.size() + 1);
    std::strcpy(*out_c_header, hdr.c_str());
    return 0;
}

LE_COMPILER_API void le_compile_result_free(le_compile_result_t* result)
{
    if (!result) return;

    if (result->error_message) {
        std::free(result->error_message);
        result->error_message = nullptr;
    }
    if (result->warnings) {
        std::free(result->warnings);
        result->warnings = nullptr;
    }
    if (result->binary_data) {
        std::free(result->binary_data);
        result->binary_data = nullptr;
    }
    if (result->c_header_code) {
        std::free(result->c_header_code);
        result->c_header_code = nullptr;
    }
    if (result->disassembly_text) {
        std::free(result->disassembly_text);
        result->disassembly_text = nullptr;
    }
    if (result->json_circuit) {
        std::free(result->json_circuit);
        result->json_circuit = nullptr;
    }

    std::memset(result, 0, sizeof(le_compile_result_t));
}

LE_COMPILER_API void le_compiler_free_buffer(void* ptr)
{
    if (ptr) {
        std::free(ptr);
    }
}

LE_COMPILER_API const char* le_compiler_get_version(void)
{
    return "2.0.0";
}

} // extern "C"

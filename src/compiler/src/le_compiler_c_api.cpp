/**
 * @file le_compiler_c_api.cpp
 * @brief Implementation of the public C API bridge for the LogicElements compiler.
 *
 * Provides a clean C ABI boundary around internal C++ compiler classes (`CompilerCore`,
 * `disassemble_binary`). Guarantees that C++ exceptions never leak across the C linkage
 * boundary into external callers.
 */

#include "le_compiler.h"
#include "le_compiler_core.h"
#include "le_disasm_core.h"
#include "le_loader.h"
#include "le_crypto.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

namespace {

/**
 * @brief Helper that allocates a null-terminated heap copy of a string safely.
 * @param[in] s String content to duplicate.
 * @return Heap-allocated char pointer, or nullptr on allocation failure.
 */
char* duplicate_string(const std::string& s) noexcept {
    char* buf = static_cast<char*>(std::malloc(s.size() + 1));
    if (buf) {
        std::memcpy(buf, s.data(), s.size());
        buf[s.size()] = '\0';
    }
    return buf;
}

/**
 * @brief Helper that allocates a null-terminated heap copy of a C-string safely.
 * @param[in] s Null-terminated C string.
 * @return Heap-allocated char pointer, or nullptr on allocation failure.
 */
char* duplicate_cstring(const char* s) noexcept {
    if (!s) return nullptr;
    size_t len = std::strlen(s);
    char* buf = static_cast<char*>(std::malloc(len + 1));
    if (buf) {
        std::memcpy(buf, s, len + 1);
    }
    return buf;
}

} // namespace

extern "C" {

LE_COMPILER_API int le_compile_json_ex(
    const char* circuit_json,
    const char* board_profile_json,
    const le_compiler_options_t* options,
    le_compile_result_t* out_result)
{
    if (!out_result) {
        return -1;
    }
    std::memset(out_result, 0, sizeof(le_compile_result_t));

    if (!circuit_json) {
        out_result->success = 0;
        out_result->error_message = duplicate_cstring("circuit_json parameter is null");
        return -1;
    }

    try {
        std::string c_str = circuit_json;
        std::string b_str = board_profile_json ? board_profile_json : "";
        le_compiler_options_t opts = options ? *options : le_compiler_options_init_default();

        LogicElements::CompilerCore compiler;
        return compiler.compile_ex(c_str, b_str, opts, out_result);
    } catch (const std::exception& e) {
        out_result->success = 0;
        std::string err = std::string("Unhandled compiler exception: ") + e.what();
        out_result->error_message = duplicate_string(err);
        return 1;
    } catch (...) {
        out_result->success = 0;
        out_result->error_message = duplicate_cstring("Unknown critical exception during compilation");
        return 1;
    }
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
            *out_error_msg = duplicate_cstring("Binary data is null or empty");
        }
        return -1;
    }

    try {
        std::string board = board_profile_json ? board_profile_json : "";
        std::string dis = LogicElements::disassemble_binary(
            bin_data, bin_len, user_bool_count, user_float_count, user_int_count, board);
        *out_disassembly = duplicate_string(dis);
        return (*out_disassembly != nullptr) ? 0 : -1;
    } catch (const std::exception& e) {
        if (out_error_msg) {
            std::string err = std::string("Disassembly error: ") + e.what();
            *out_error_msg = duplicate_string(err);
        }
        return -1;
    } catch (...) {
        if (out_error_msg) {
            *out_error_msg = duplicate_cstring("Unknown critical exception during disassembly");
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

    try {
        std::string name = (array_name && *array_name) ? array_name : "le_default_program";
        std::string hdr = LogicElements::CompilerCore::export_c_header(bin_data, bin_len, name);

        *out_c_header = duplicate_string(hdr);
        return (*out_c_header != nullptr) ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

LE_COMPILER_API int le_compiler_sign_container(
    const uint8_t* in_container,
    size_t in_size,
    const uint8_t secret_key[64],
    uint8_t* out_container,
    size_t max_out_size,
    size_t* out_size)
{
    return static_cast<int>(le_loader_sign_container(
        in_container, in_size, secret_key, out_container, max_out_size, out_size));
}

LE_COMPILER_API int le_compiler_keygen(
    uint8_t out_pk[32],
    uint8_t out_sk[64],
    const uint8_t seed[32])
{
    return static_cast<int>(le_crypto_ed25519_keypair_from_seed(out_pk, out_sk, seed));
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
    return "LogicElements Compiler v13.0.0 (Container: v13 LEB2)";
}

} // extern "C"

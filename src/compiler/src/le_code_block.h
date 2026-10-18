#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "le_json.h"

namespace LogicElements {

struct CodeBlockResult {
    bool success{true};
    std::string error_message;
    std::vector<std::string> warnings;
};

class CodeBlockExpander {
public:
    /**
     * @brief Pre-compilation pass that expands all "CODE_BLOCK" elements in circuit_doc
     * into deterministic primitive hardware elements (MUX, arithmetic, registers, logic)
     * and synthesized nets.
     *
     * @param circuit_doc In/Out JSON document containing "elements" and "nets".
     * @param out_error Output error message if expansion fails.
     * @param out_warnings Output non-fatal compiler warnings (e.g. inferred latches).
     * @return true if all code blocks were successfully lowered; false on compilation error.
     */
    static bool expand_code_blocks(JsonValue& circuit_doc, std::string& out_error, std::vector<std::string>& out_warnings);
};

} // namespace LogicElements

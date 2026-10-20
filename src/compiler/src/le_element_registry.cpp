/**
 * @file le_element_registry.cpp
 * @brief Implementation of the central element metadata registry and lookup tables.
 */

#include "le_element_registry.h"
#include <algorithm>
#include <cctype>
#include <deque>

namespace LogicElements {

namespace {

struct RegistryData {
    std::deque<ElementTraits> traits_list;
    std::unordered_map<std::string, const ElementTraits*> type_map;

    RegistryData() {
        auto add = [this](const ElementTraits& t) {
            traits_list.push_back(t);
            const ElementTraits* ptr = &traits_list.back();
            type_map[ptr->canonical_type] = ptr;
            for (const auto& a : ptr->aliases) {
                type_map[a] = ptr;
            }
        };

        // Inputs & Outputs
        add({"DIGITALINPUT", {"INPUT", "LE_DIGITALINPUT"}, LE_OP_NOP, ElemDataType::Bool, ElemCategory::Input, false, LE_BLK_NONE, 0});
        add({"DIGITALOUTPUT", {"OUTPUT", "LE_DIGITALOUTPUT"}, LE_OP_NOP, ElemDataType::Bool, ElemCategory::Output, false, LE_BLK_NONE, 1});
        add({"ANALOGINPUT", {"LE_ANALOG_INPUT", "LE_ANALOGINPUT"}, LE_OP_NOP, ElemDataType::Float, ElemCategory::Input, false, LE_BLK_NONE, 0});
        add({"ANALOGOUTPUT", {"LE_ANALOG_OUTPUT", "LE_ANALOGOUTPUT", "AOUT"}, LE_OP_NOP, ElemDataType::Float, ElemCategory::Output, false, LE_BLK_NONE, 1});

        // Memory Registers (Roots in DCE, but not stateful blocks; opcodes managed by lowering)
        add({"BOOLREGISTER", {"LE_BOOLREGISTER"}, LE_OP_NOP, ElemDataType::Bool, ElemCategory::Register, false, LE_BLK_NONE, 1});
        add({"INTREGISTER", {"LE_INTREGISTER"}, LE_OP_NOP, ElemDataType::Int, ElemCategory::Register, false, LE_BLK_NONE, 1});
        add({"FLOATREGISTER", {"LE_FLOATREGISTER"}, LE_OP_NOP, ElemDataType::Float, ElemCategory::Register, false, LE_BLK_NONE, 1});
        add({"COMPLEXREGISTER", {"LE_COMPLEXREGISTER"}, LE_OP_NOP, ElemDataType::Complex, ElemCategory::Register, false, LE_BLK_NONE, 1});

        // Buffer Gates
        add({"MOVE", {"LE_MOVE"}, LE_OP_MOVE, ElemDataType::Bool, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"MOVE_I", {"LE_MOVE_I"}, LE_OP_MOVE_I, ElemDataType::Int, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"MOVE_F", {"LE_MOVE_F"}, LE_OP_MOVE_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"MOVE_C", {"LE_MOVE_C"}, LE_OP_MOVE_C, ElemDataType::Complex, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});

        // Boolean Logic Gates
        add({"AND", {"LE_AND"}, LE_OP_AND, ElemDataType::Bool, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"OR", {"LE_OR"}, LE_OP_OR, ElemDataType::Bool, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"XOR", {"LE_XOR"}, LE_OP_XOR, ElemDataType::Bool, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"NAND", {"LE_NAND"}, LE_OP_NAND, ElemDataType::Bool, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"NOR", {"LE_NOR"}, LE_OP_NOR, ElemDataType::Bool, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"NOT", {"LE_NOT"}, LE_OP_NOT, ElemDataType::Bool, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"MUX", {"LE_MUX"}, LE_OP_MUX, ElemDataType::Bool, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});

        // Integer Math & Word Logic
        add({"ADD_I", {"LE_ADD_I"}, LE_OP_ADD_I, ElemDataType::Int, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"SUB_I", {"LE_SUB_I"}, LE_OP_SUB_I, ElemDataType::Int, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"MUL_I", {"LE_MUL_I"}, LE_OP_MUL_I, ElemDataType::Int, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"DIV_I", {"LE_DIV_I"}, LE_OP_DIV_I, ElemDataType::Int, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"NEG_I", {"LE_NEG_I"}, LE_OP_NEG_I, ElemDataType::Int, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"WORD_AND", {"LE_WORD_AND"}, LE_OP_WORD_AND, ElemDataType::Int, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"WORD_OR", {"LE_WORD_OR"}, LE_OP_WORD_OR, ElemDataType::Int, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"WORD_XOR", {"LE_WORD_XOR"}, LE_OP_WORD_XOR, ElemDataType::Int, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"WORD_NOT", {"LE_WORD_NOT"}, LE_OP_WORD_NOT, ElemDataType::Int, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});

        // Float Arithmetic
        add({"ADD", {"LE_ADD", "ADD_F"}, LE_OP_ADD_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"SUB", {"SUBTRACT", "LE_SUB", "SUB_F"}, LE_OP_SUB_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"MUL", {"MULTIPLY", "LE_MUL", "MUL_F"}, LE_OP_MUL_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"DIV", {"DIVIDE", "LE_DIV", "DIV_F"}, LE_OP_DIV_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"ABS", {"LE_ABS"}, LE_OP_ABS_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"NEG", {"LE_NEG", "NEG_F"}, LE_OP_NEG_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"MIN", {"LE_MIN"}, LE_OP_MIN_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"MAX", {"LE_MAX"}, LE_OP_MAX_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"CLAMP", {"LE_CLAMP"}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 2});
        add({"SQRT", {"LE_SQRT"}, LE_OP_SQRT_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 4});
        add({"SIN", {"LE_SIN"}, LE_OP_SIN_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 4});
        add({"COS", {"LE_COS"}, LE_OP_COS_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 4});
        add({"TAN", {"LE_TAN"}, LE_OP_TAN_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 4});
        add({"ASIN", {"LE_ASIN"}, LE_OP_ASIN_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 4});
        add({"ACOS", {"LE_ACOS"}, LE_OP_ACOS_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 4});
        add({"ATAN", {"LE_ATAN"}, LE_OP_ATAN_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 4});
        add({"ATAN2", {"LE_ATAN2"}, LE_OP_ATAN2_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 6});
        add({"EXP", {"LE_EXP"}, LE_OP_EXP_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 4});
        add({"LOG", {"LN", "LE_LOG", "LE_LN"}, LE_OP_LOG_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 4});
        add({"LOG10", {"LE_LOG10"}, LE_OP_LOG10_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 4});
        add({"POW", {"POWER", "LE_POW", "LE_POWER"}, LE_OP_POW_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 6});
        add({"FMA", {"LE_FMA", "MAC", "LE_MAC"}, LE_OP_FMA_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"FMS", {"LE_FMS"}, LE_OP_FMS_F, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});

        // Comparisons
        add({"CMP_EQ", {"LE_CMP_EQ"}, LE_OP_CMP_EQ, ElemDataType::Bool, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"CMP_NE", {"LE_CMP_NE"}, LE_OP_CMP_NE, ElemDataType::Bool, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"CMP_GT", {"LE_CMP_GT"}, LE_OP_CMP_GT, ElemDataType::Bool, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"CMP_GE", {"LE_CMP_GE"}, LE_OP_CMP_GE, ElemDataType::Bool, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"CMP_LT", {"LE_CMP_LT"}, LE_OP_CMP_LT, ElemDataType::Bool, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});
        add({"CMP_LE", {"LE_CMP_LE"}, LE_OP_CMP_LE, ElemDataType::Bool, ElemCategory::Combinatorial, false, LE_BLK_NONE, 1});

        // Latches & Edge Triggers
        add({"SR", {"LE_SR"}, LE_OP_SR, ElemDataType::Bool, ElemCategory::StatefulBlock, true, LE_BLK_NONE, 1});
        add({"RS", {"LE_RS"}, LE_OP_RS, ElemDataType::Bool, ElemCategory::StatefulBlock, true, LE_BLK_NONE, 1});
        add({"LATCH", {"LE_LATCH"}, LE_OP_SR, ElemDataType::Bool, ElemCategory::StatefulBlock, true, LE_BLK_NONE, 1});
        add({"RTRIG", {"LE_RTRIG"}, LE_OP_RTRIG, ElemDataType::Bool, ElemCategory::StatefulBlock, true, LE_BLK_NONE, 1});
        add({"FTRIG", {"LE_FTRIG"}, LE_OP_FTRIG, ElemDataType::Bool, ElemCategory::StatefulBlock, true, LE_BLK_NONE, 1});

        // Timers & Counters
        add({"TON", {"LE_TON"}, LE_OP_TON, ElemDataType::Bool, ElemCategory::StatefulBlock, true, LE_BLK_TIMER, 2});
        add({"TOF", {"LE_TOF"}, LE_OP_TOF, ElemDataType::Bool, ElemCategory::StatefulBlock, true, LE_BLK_TIMER, 2});
        add({"TP", {"LE_TP"}, LE_OP_TP, ElemDataType::Bool, ElemCategory::StatefulBlock, true, LE_BLK_TIMER, 2});
        add({"CTU", {"LE_CTU"}, LE_OP_CTU, ElemDataType::Int, ElemCategory::StatefulBlock, true, LE_BLK_COUNTER, 2});
        add({"CTD", {"LE_CTD"}, LE_OP_CTD, ElemDataType::Int, ElemCategory::StatefulBlock, true, LE_BLK_COUNTER, 2});
        add({"CTUD", {"LE_CTUD"}, LE_OP_CTUD, ElemDataType::Int, ElemCategory::StatefulBlock, true, LE_BLK_COUNTER, 3});

        // DSP Blocks
        add({"PID", {"LE_PID"}, LE_OP_PID, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_PID, 8});
        add({"LPF", {"LE_LPF", "LPF_1P"}, LE_OP_LPF_1P, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_LPF, 4});
        add({"BIQUAD", {"LE_BIQUAD", "BIQUAD_IIR"}, LE_OP_BIQUAD_IIR, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_BIQUAD, 6});
        add({"MOVING_AVG", {"LE_MOVING_AVG", "WINDOW_AVG"}, LE_OP_MOVING_AVG, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_MOVING_AVG, 4});
        add({"RATE_LIMITER", {"LE_RATE_LIMITER", "SLEW_LIMITER"}, LE_OP_RATE_LIMITER, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_RATE_LIMITER, 3});
        add({"DEADBAND", {"LE_DEADBAND"}, LE_OP_DEADBAND, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_DEADBAND, 2});
        add({"WASHOUT", {"LE_WASHOUT"}, LE_OP_WASHOUT, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_WASHOUT, 4});
        add({"PEAK_DETECTOR", {"LE_PEAK_DETECTOR", "ENVELOPE"}, LE_OP_PEAK_DETECTOR, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_PEAK, 3});
        add({"RMS", {"LE_RMS"}, LE_OP_RMS, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_RMS, 6});
        add({"MEDIAN", {"LE_MEDIAN", "MEDIAN_FILTER"}, LE_OP_MEDIAN, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_MEDIAN, 8});
        add({"DERIVATIVE", {"LE_DERIVATIVE"}, LE_OP_DERIVATIVE, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_DERIVATIVE, 3});
        add({"ZERO_CROSSING", {"LE_ZERO_CROSSING"}, LE_OP_ZERO_CROSSING, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_ZERO_CROSSING, 3});
        add({"LUT_1D", {"LE_LUT_1D", "LUT"}, LE_OP_LUT_1D, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_LUT_1D, 5});
        add({"TOTALIZER", {"LE_TOTALIZER"}, LE_OP_TOTALIZER, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_TOTALIZER, 3});
        add({"MIN_MAX_HOLD", {"LE_MIN_MAX_HOLD"}, LE_OP_MIN_MAX_HOLD, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_MIN_MAX_HOLD, 3});

        // Power Protection
        add({"OVERCURRENT_51", {"OVERCURRENT", "LE_OVERCURRENT_51", "LE_OVERCURRENT"}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_OVERCURRENT, 8});
        add({"PHASOR_1P", {"LE_PHASOR_1P", "LE_1P_WINDING"}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_PHASOR, 12});
        add({"PHASOR_1P_SELF_SYNC", {}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_PHASOR, 14});
        add({"PHASOR_3P", {"LE_PHASOR_3P"}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_PHASOR3, 24});
        add({"FREQ_EST", {"LE_FREQ_EST", "FREQ_TRACKER", "ANSI_81"}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_FREQ_EST, 10});
        add({"SYM_COMP", {"LE_SYM_COMP"}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_SYMCOMP, 10});
        add({"DIFF_87", {"DIFF", "LE_DIFF_87", "LE_DIFF"}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_DIFF_87, 8});
        add({"PHASE_COMP", {"TRANSFORM_33", "TCOMP", "LE_PHASE_COMP"}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_PHASE_COMP, 6});
        add({"DIST_21", {"LE_DIST_21"}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::StatefulBlock, true, LE_BLK_21, 12});

        // Complex Math & Conversions
        add({"CADD", {"LE_CADD", "C_ADD"}, LE_OP_CADD_F, ElemDataType::Complex, ElemCategory::Combinatorial, false, LE_BLK_NONE, 2});
        add({"CSUB", {"LE_CSUB", "C_SUB"}, LE_OP_CSUB_F, ElemDataType::Complex, ElemCategory::Combinatorial, false, LE_BLK_NONE, 2});
        add({"CMUL", {"LE_CMUL", "C_MUL"}, LE_OP_CMUL_F, ElemDataType::Complex, ElemCategory::Combinatorial, false, LE_BLK_NONE, 4});
        add({"CDIV", {"LE_CDIV", "C_DIV"}, LE_OP_CDIV_F, ElemDataType::Complex, ElemCategory::Combinatorial, false, LE_BLK_NONE, 8});
        add({"RECT2POLAR", {"LE_RECT2POLAR"}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 6});
        add({"POLAR2RECT", {"LE_POLAR2RECT"}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 6});
        add({"PHASOR_SHIFT", {"LE_PHASOR_SHIFT"}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 4});
        add({"COMPLEX2POLAR", {"LE_COMPLEX2POLAR"}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 6});
        add({"COMPLEX2RECT", {"LE_COMPLEX2RECT"}, LE_OP_BLOCK, ElemDataType::Float, ElemCategory::Combinatorial, false, LE_BLK_NONE, 2});
        add({"RECT2COMPLEX", {"LE_RECT2COMPLEX"}, LE_OP_BLOCK, ElemDataType::Complex, ElemCategory::Combinatorial, false, LE_BLK_NONE, 2});
        add({"POLAR2COMPLEX", {"LE_POLAR2COMPLEX"}, LE_OP_BLOCK, ElemDataType::Complex, ElemCategory::Combinatorial, false, LE_BLK_NONE, 6});

        // Serial Bus
        add({"I2C", {"LE_I2C"}, LE_OP_BLOCK, ElemDataType::Int, ElemCategory::StatefulBlock, true, LE_BLK_I2C, 10});
        add({"SPI", {"LE_SPI"}, LE_OP_BLOCK, ElemDataType::Int, ElemCategory::StatefulBlock, true, LE_BLK_SPI, 10});

        // Tags
        add({"TAG", {"LE_TAG"}, LE_OP_NOP, ElemDataType::Void, ElemCategory::Tag, false, LE_BLK_NONE, 0});
    }
};

const RegistryData& get_registry() {
    static const RegistryData instance;
    return instance;
}

} // namespace

std::string ElementRegistry::normalize_type(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;

    std::string out;
    out.reserve(end - start);
    for (size_t i = start; i < end; ++i) {
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(s[i]))));
    }
    return out;
}

const ElementTraits* ElementRegistry::lookup(std::string_view type_name) noexcept {
    std::string norm = normalize_type(type_name);
    const auto& reg = get_registry();
    auto it = reg.type_map.find(norm);
    return (it != reg.type_map.end()) ? it->second : nullptr;
}

ElemDataType ElementRegistry::get_data_type(std::string_view type_name) noexcept {
    const ElementTraits* t = lookup(type_name);
    return t ? t->data_type : ElemDataType::Bool;
}

bool ElementRegistry::is_stateful(std::string_view type_name) noexcept {
    const ElementTraits* t = lookup(type_name);
    return t ? t->is_stateful : false;
}

bool ElementRegistry::is_root(std::string_view type_name, std::string_view tag_direction) noexcept {
    const ElementTraits* t = lookup(type_name);
    if (!t) return false;
    if (t->category == ElemCategory::Output ||
        t->category == ElemCategory::Register ||
        t->category == ElemCategory::StatefulBlock) {
        return true;
    }
    if (t->category == ElemCategory::Tag) {
        std::string dir;
        for (char c : tag_direction) dir += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return (dir == "send");
    }
    return false;
}

uint8_t ElementRegistry::get_opcode(std::string_view type_name) noexcept {
    const ElementTraits* t = lookup(type_name);
    return t ? t->opcode : LE_OP_NOP;
}

uint8_t ElementRegistry::get_optimizer_opcode(std::string_view type_name) noexcept {
    const ElementTraits* t = lookup(type_name);
    if (!t) return LE_OP_NOP;
    if (t->category == ElemCategory::Combinatorial) {
        return t->opcode;
    }
    // Only single-output DSP blocks that are lowered as standalone math instructions
    switch (t->opcode) {
        case LE_OP_LPF_1P:
        case LE_OP_BIQUAD_IIR:
        case LE_OP_MOVING_AVG:
        case LE_OP_RATE_LIMITER:
        case LE_OP_DEADBAND:
        case LE_OP_WASHOUT:
        case LE_OP_PEAK_DETECTOR:
        case LE_OP_RMS:
        case LE_OP_MEDIAN:
        case LE_OP_DERIVATIVE:
        case LE_OP_ZERO_CROSSING:
        case LE_OP_LUT_1D:
        case LE_OP_TOTALIZER:
        case LE_OP_MIN_MAX_HOLD:
            return t->opcode;
        default:
            return LE_OP_NOP;
    }
}

uint8_t ElementRegistry::get_state_kind(std::string_view type_name) noexcept {
    const ElementTraits* t = lookup(type_name);
    return t ? t->state_kind : LE_BLK_NONE;
}

} // namespace LogicElements

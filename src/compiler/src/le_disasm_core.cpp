#include "le_disasm_core.h"
#include "le_compiler_core.h"
#include "le_types.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>

namespace LogicElements {

std::string format_address(uint16_t addr, int user_bool_count, int user_float_count, int user_int_count)
{
    if (addr == LE_ADDR_UNUSED) return "-";
    if (addr == LE_CONST_FALSE) return "FALSE";
    if (addr == LE_CONST_TRUE) return "TRUE";
    if (addr == LE_CONST_ZERO_F) return "0.0f";
    if (addr == LE_CONST_ONE_F) return "1.0f";

    uint16_t region = addr & LE_ADDR_REGION_MASK;
    uint16_t idx = addr & LE_ADDR_INDEX_MASK;

    if (region == LE_REGION_DIN) return "DIN[" + std::to_string(idx) + "]";
    if (region == LE_REGION_DOUT) return "DOUT[" + std::to_string(idx) + "]";
    if (region == LE_REGION_BOOL_REG) {
        int b_idx = addr & 0x1FFF;
        if (user_bool_count >= 0 && b_idx >= user_bool_count) {
            return "T_BOOL[" + std::to_string(b_idx - user_bool_count) + "]";
        }
        return "BOOL[" + std::to_string(b_idx) + "]";
    }
    if (region == LE_REGION_FLOAT || region == LE_REGION_FLOAT_EXT1 ||
        region == LE_REGION_FLOAT_EXT2 || region == LE_REGION_FLOAT_EXT3) {
        int f_idx = addr & 0x3FFF;
        if (user_float_count >= 0 && f_idx >= user_float_count) {
            return "T_FLOAT[" + std::to_string(f_idx - user_float_count) + "]";
        }
        return "FLOAT[" + std::to_string(f_idx) + "]";
    }
    if (region == LE_REGION_TIMER) return "TIMER[" + std::to_string(idx) + "]";
    if (region == LE_REGION_COUNTER) return "COUNTER[" + std::to_string(idx) + "]";
    if (region == LE_REGION_INT_REG) {
        if (user_int_count >= 0 && idx >= static_cast<uint16_t>(user_int_count)) {
            return "T_INT[" + std::to_string(idx - user_int_count) + "]";
        }
        return "INT[" + std::to_string(idx) + "]";
    }
    if (region == LE_REGION_AIN) return "AIN[" + std::to_string(idx) + "]";

    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << addr;
    return ss.str();
}

std::string format_opcode(uint8_t op, uint8_t mod)
{
    switch (op) {
        case LE_OP_NOP: return "NOP";
        case LE_OP_MOVE: return "MOVE";
        case LE_OP_NOT: return "NOT";
        case LE_OP_AND: return "AND";
        case LE_OP_OR: return "OR";
        case LE_OP_XOR: return "XOR";
        case LE_OP_NAND: return "NAND";
        case LE_OP_NOR: return "NOR";
        case LE_OP_MUX: return "MUX";

        case LE_OP_RTRIG: return "RTRIG";
        case LE_OP_FTRIG: return "FTRIG";
        case LE_OP_SR: return "SR";
        case LE_OP_RS: return "RS";

        case LE_OP_TON: return "TON";
        case LE_OP_TOF: return "TOF";
        case LE_OP_TP: return "TP";
        case LE_OP_CTU: return "CTU";
        case LE_OP_CTD: return "CTD";
        case LE_OP_CTUD: return "CTUD";

        case LE_OP_MOVE_F: return "MOVE_F";
        case LE_OP_ADD_F: return "ADD_F";
        case LE_OP_SUB_F: return "SUB_F";
        case LE_OP_MUL_F: return "MUL_F";
        case LE_OP_DIV_F: return "DIV_F";
        case LE_OP_NEG_F: return "NEG_F";
        case LE_OP_ABS_F: return "ABS_F";
        case LE_OP_MIN_F: return "MIN_F";
        case LE_OP_MAX_F: return "MAX_F";
        case LE_OP_CLAMP_F: return "CLAMP_F";
        case LE_OP_SCALE_F: return "SCALE_F";

        case LE_OP_CMP_GT: return "CMP_GT";
        case LE_OP_CMP_LT: return "CMP_LT";
        case LE_OP_CMP_GE: return "CMP_GE";
        case LE_OP_CMP_LE: return "CMP_LE";
        case LE_OP_CMP_EQ: return "CMP_EQ";
        case LE_OP_CMP_NE: return "CMP_NE";

        case LE_OP_PID: return "PID";
        case LE_OP_OVERCURRENT: return "OVERCURRENT";
        case LE_OP_RECT2POLAR: return "RECT2POLAR";
        case LE_OP_POLAR2RECT: return "POLAR2RECT";
        case LE_OP_PHASOR_SHIFT: return "PHASOR_SHIFT";
#if LE_ENABLE_PROTECTION
        case LE_OP_PHASOR_1P: return "PHASOR_1P";
        case LE_OP_SYM_COMP: return "SYM_COMP";
        case LE_OP_DIFF_87: return "DIFF_87";
        case LE_OP_DIST_21: return "DIST_21";
#endif
#if LE_ENABLE_SERIAL_BUS
        case LE_OP_I2C: return "I2C";
        case LE_OP_SPI: return "SPI";
#endif
#if LE_ENABLE_DSP
        case LE_OP_LPF_1P: return "LPF_1P";
        case LE_OP_BIQUAD_IIR: return "BIQUAD_IIR";
        case LE_OP_MOVING_AVG: return "MOVING_AVG";
        case LE_OP_RATE_LIMITER: return "RATE_LIMITER";
        case LE_OP_DEADBAND: return "DEADBAND";
        case LE_OP_WASHOUT: return "WASHOUT";
        case LE_OP_PEAK_DETECTOR: return "PEAK_DETECTOR";
        case LE_OP_RMS: return "RMS";
        case LE_OP_MEDIAN: return "MEDIAN";
        case LE_OP_DERIVATIVE: return "DERIVATIVE";
        case LE_OP_ZERO_CROSSING: return "ZERO_CROSSING";
        case LE_OP_LUT_1D: return "LUT_1D";
        case LE_OP_TOTALIZER: return "TOTALIZER";
        case LE_OP_MIN_MAX_HOLD: return "MIN_MAX_HOLD";
#endif
        case LE_OP_EXT_CALL: return "EXT_CALL";
        case LE_OP_BLOCK:
            switch (mod) {
                case LE_FUNC_MUX_SELECT: return "MUX";
                case LE_FUNC_RECT2POLAR: return "RECT2POLAR";
                case LE_FUNC_POLAR2RECT: return "POLAR2RECT";
                case LE_FUNC_PHASOR_SHIFT: return "PHASOR_SHIFT";
                case LE_FUNC_PHASOR_1P: return "PHASOR_1P";
                default: return "BLOCK";
            }

        default: {
            std::ostringstream ss;
            ss << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(op);
            return ss.str();
        }
    }
}

std::string disassemble_binary(const uint8_t* bin_data, size_t bin_len, int user_bool_count, int user_float_count, int user_int_count)
{
    if (!bin_data || bin_len < sizeof(le_header_t)) {
        return "Error: Data size smaller than the .lebin header";
    }

    le_header_t h;
    std::memcpy(&h, bin_data, sizeof(le_header_t));

    if (h.magic != LE_BIN_MAGIC) {
        std::ostringstream ss;
        ss << "Error: Invalid binary magic: 0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
           << h.magic << " (expected 'LEB1')";
        return ss.str();
    }

    std::ostringstream out;
    out << "============================================================\n";
    out << " LOGICELEMENTS BINARY DISASSEMBLY (.lebin)\n";
    out << "============================================================\n";
    out << "Magic:               0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << h.magic << " ('LEB1')\n";
    out << std::dec;
    out << "Version:             " << h.version << "\n";
    out << "Flags:               0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << h.flags
        << " (Autostart: " << ((h.flags & LE_FLAG_AUTOSTART) ? "True" : "False") << ")\n";
    out << std::dec;
    out << "Instruction Count:   " << h.instruction_count << "\n";

    std::string bool_summary = "BOOL:" + std::to_string(h.bool_reg_count);
    if (user_bool_count >= 0) {
        int temp_bools = h.bool_reg_count - user_bool_count;
        if (temp_bools < 0) temp_bools = 0;
        bool_summary += " (User:" + std::to_string(user_bool_count) + ", Temp:" + std::to_string(temp_bools) + ")";
    }

    std::string float_summary = "FLOAT:" + std::to_string(h.float_reg_count);
    if (user_float_count >= 0) {
        int temp_floats = h.float_reg_count - user_float_count;
        if (temp_floats < 0) temp_floats = 0;
        float_summary += " (User:" + std::to_string(user_float_count) + ", Temp:" + std::to_string(temp_floats) + ")";
    }

    out << "I/O Allocation:      DIN:" << h.digital_in_count << "  DOUT:" << h.digital_out_count
        << "  " << bool_summary << "  " << float_summary << "  Timers:" << h.timer_count
        << "  Counters:" << h.counter_count << "\n";

    out << "Header CRC32:        0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << h.crc32 << "\n";

    size_t payload_size = bin_len - sizeof(le_header_t);
    const uint8_t* payload_ptr = bin_data + sizeof(le_header_t);
    uint32_t calc_crc = CompilerCore::compute_crc32(payload_ptr, payload_size);
    out << "Payload Integrity:   " << (calc_crc == h.crc32 ? "MATCH (OK)" : "CRC MISMATCH (CORRUPTED)") << "\n";
    out << "------------------------------------------------------------\n";
    out << "INDEX   OPCODE         IN_A         IN_B         OUT         \n";
    out << "------------------------------------------------------------\n";

    size_t inst_count = h.instruction_count;
    std::vector<size_t> block_offsets;
    {
        size_t boff = sizeof(le_header_t) + inst_count * sizeof(le_instruction_t);
        for (uint16_t b = 0; b < h.block_count; b++) {
            if (boff + (size_t)LE_BLOCK_DESC_HEADER_BYTES > bin_len) break;
            const le_block_desc_t* d = (const le_block_desc_t*)(bin_data + boff);
            size_t dsize = (size_t)LE_BLOCK_DESC_HEADER_BYTES +
                           ((size_t)d->in_count + (size_t)d->out_count) * sizeof(uint16_t);
            if (boff + dsize > bin_len) break;
            block_offsets.push_back(boff);
            boff += dsize;
        }
    }
    for (size_t i = 0; i < inst_count; i++) {
        size_t offset = sizeof(le_header_t) + (i * sizeof(le_instruction_t));
        if (offset + sizeof(le_instruction_t) > bin_len) {
            out << "[" << std::setw(4) << std::setfill('0') << i << "] Unexpected end of binary payload\n";
            break;
        }

        le_instruction_t inst;
        std::memcpy(&inst, bin_data + offset, sizeof(le_instruction_t));

        std::string block_out;
        std::string op_str = format_opcode(inst.opcode, inst.modifier);
        std::string in_a_str = format_address(inst.in_a, user_bool_count, user_float_count, user_int_count);
        std::string in_b_str;
        if (inst.opcode == LE_OP_SCALE_F) {
            in_b_str = "SCL[" + std::to_string(inst.modifier) + "]";
        } else if (inst.opcode == LE_OP_EXT_CALL) {
            std::ostringstream ss;
            ss << "fn:0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(inst.modifier);
            in_b_str = ss.str();
        } else {
            if (inst.opcode == LE_OP_BLOCK) {
            /* Block call: in_a is the block-table index, modifier is the
             * function id. Operand addresses live in the descriptor. */
            in_a_str = "BLK[" + std::to_string(inst.in_a) + "]";
            if (inst.in_a < block_offsets.size()) {
                const le_block_desc_t* d = (const le_block_desc_t*)(bin_data + block_offsets[inst.in_a]);
                const uint16_t* argsA = (const uint16_t*)(d + 1);
                std::ostringstream a2;
                a2 << (int)d->in_count << "->" << (int)d->out_count << " [";
                for (int k = 0; k < d->in_count; k++) { if (k) a2 << " "; a2 << format_address(argsA[k], user_bool_count, user_float_count, user_int_count); }
                a2 << " | ";
                for (int k = 0; k < d->out_count; k++) { if (k) a2 << " "; a2 << format_address(argsA[d->in_count + k], user_bool_count, user_float_count, user_int_count); }
                a2 << "]";
                block_out = a2.str();
            }
            std::ostringstream ss;
            ss << "fn:0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(inst.modifier);
            in_b_str = ss.str();
        } else {
            in_b_str = format_address(inst.in_b, user_bool_count, user_float_count, user_int_count);
        }
        }
        std::string out_str = format_address(inst.out, user_bool_count, user_float_count, user_int_count);
            if (inst.opcode == LE_OP_BLOCK) out_str = block_out;

        out << "[" << std::dec << std::right << std::setw(4) << std::setfill('0') << i << "]  "
            << std::left << std::setw(14) << std::setfill(' ') << op_str << " "
            << std::setw(12) << in_a_str << " "
            << std::setw(12) << in_b_str << " -> "
            << std::setw(12) << out_str << "\n";
    }

    out << "============================================================\n";
    return out.str();
}

} // namespace LogicElements

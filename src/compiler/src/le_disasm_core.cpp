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
    if (addr == LE_CONST_ZERO_C) return "0+0j";

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
    if (region == LE_REGION_CMPLX) return "CMPLX[" + std::to_string(idx) + "]";

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
        case LE_OP_CADD_F: return "CADD_F";
        case LE_OP_CSUB_F: return "CSUB_F";
        case LE_OP_CMUL_F: return "CMUL_F";
        case LE_OP_CDIV_F: return "CDIV_F";
        case LE_OP_MOVE_C: return "MOVE_C";

        case LE_OP_CMP_GT: return "CMP_GT";
        case LE_OP_CMP_LT: return "CMP_LT";
        case LE_OP_CMP_GE: return "CMP_GE";
        case LE_OP_CMP_LE: return "CMP_LE";
        case LE_OP_CMP_EQ: return "CMP_EQ";
        case LE_OP_CMP_NE: return "CMP_NE";

        #if LE_ENABLE_PROTECTION
        case LE_OP_PID: return "PID";
        case LE_OP_OVERCURRENT: return "OVERCURRENT_51";
        case LE_OP_RECT2POLAR: return "RECT2POLAR";
        case LE_OP_POLAR2RECT: return "POLAR2RECT";
        case LE_OP_PHASOR_SHIFT: return "PHASOR_SHIFT";
        case LE_OP_PHASOR_1P: return "PHASOR_1P";
        case LE_OP_SYM_COMP: return "SYM_COMP";
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
                case LE_FUNC_COMPLEX2POLAR: return "COMPLEX2POLAR";
                case LE_FUNC_COMPLEX2RECT: return "COMPLEX2RECT";
                case LE_FUNC_RECT2COMPLEX: return "RECT2COMPLEX";
                case LE_FUNC_POLAR2COMPLEX: return "POLAR2COMPLEX";
                case LE_FUNC_CLAMP_F: return "CLAMP_F";
                case LE_FUNC_PHASE_COMP: return "PHASE_COMP";
                case LE_FUNC_DIFF_87: return "DIFF_87";
                case LE_FUNC_DIST_21: return "DIST_21";
                default: return "BLOCK";
            }

        default: {
            std::ostringstream ss;
            ss << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(op);
            return ss.str();
        }
    }
}

std::string block_kind_name(uint8_t kind)
{
    switch (kind) {
        case LE_BLK_TIMER: return "TIMER";
        case LE_BLK_COUNTER: return "COUNTER";
        case LE_BLK_PID: return "PID";
        case LE_BLK_OVERCURRENT: return "OVERCURRENT";
        case LE_BLK_SCALER: return "SCALE_F";
        case LE_BLK_PHASOR: return "PHASOR_1P";
        case LE_BLK_SYMCOMP: return "SYM_COMP";
        case LE_BLK_21: return "DIST_21";
        case LE_BLK_DIFF_87: return "DIFF_87";
        case LE_BLK_PHASE_COMP: return "PHASE_COMP";
        case LE_BLK_I2C: return "I2C";
        case LE_BLK_SPI: return "SPI";
        case LE_BLK_LPF: return "LPF_1P";
        case LE_BLK_BIQUAD: return "BIQUAD";
        case LE_BLK_MOVING_AVG: return "MOVING_AVG";
        case LE_BLK_RATE_LIMITER: return "RATE_LIMITER";
        case LE_BLK_DEADBAND: return "DEADBAND";
        case LE_BLK_WASHOUT: return "WASHOUT";
        case LE_BLK_PEAK: return "PEAK_DETECTOR";
        case LE_BLK_RMS: return "RMS";
        case LE_BLK_MEDIAN: return "MEDIAN";
        case LE_BLK_DERIVATIVE: return "DERIVATIVE";
        case LE_BLK_ZERO_CROSSING: return "ZERO_CROSSING";
        case LE_BLK_LUT_1D: return "LUT_1D";
        case LE_BLK_TOTALIZER: return "TOTALIZER";
        case LE_BLK_MIN_MAX_HOLD: return "MIN_MAX_HOLD";
        default: return "UNKNOWN";
    }
}

static std::string fmt_cx(le_complex_t c)
{
    std::ostringstream ss;
    ss << "(" << c.r << ", " << c.i << ")";
    return ss.str();
}

/* Emits every field of a decoded state struct (the properties baked into the
 * preconfigured state image) as `    <field>  =  <value>`. */
static void emit_state_props(std::ostringstream& out, uint8_t kind, const uint8_t* p)
{
    auto row = [&](const char* name, std::string val) {
        out << "    " << std::left << std::setw(22) << std::setfill(' ') << name << "= " << val << "\n";
    };
    auto rows = [&](const char* name, float v) { row(name, std::to_string(v)); };
    auto rowd = [&](const char* name, double v) { row(name, std::to_string(v)); };
    switch (kind) {
        case LE_BLK_TIMER: {
            const le_timer_state_t* s = (const le_timer_state_t*)p;
            row("start_time_ms", std::to_string(s->start_time_ms));
            row("preset_ms", std::to_string(s->preset_ms));
            row("prev_in", s->prev_in ? "true" : "false");
            row("q", s->q ? "true" : "false");
            break;
        }
        case LE_BLK_COUNTER: {
            const le_counter_state_t* s = (const le_counter_state_t*)p;
            row("count", std::to_string(s->count));
            row("preset", std::to_string(s->preset));
            row("qu", s->qu ? "true" : "false");
            row("qd", s->qd ? "true" : "false");
            break;
        }
        case LE_BLK_PID: {
            const le_pid_state_t* s = (const le_pid_state_t*)p;
            rows("kp", s->kp); rows("ki", s->ki); rows("kd", s->kd);
            rows("integrator_min", s->out_min); rows("integrator_max", s->out_max);
            break;
        }
        case LE_BLK_SCALER: {
            const le_scale_state_t* s = (const le_scale_state_t*)p;
            rows("raw_min", s->raw_min); rows("raw_max", s->raw_max);
            rows("scale_min", s->scale_min); rows("scale_max", s->scale_max);
            row("clamp", s->clamp ? "true" : "false");
            break;
        }
        case LE_BLK_OVERCURRENT: {
            const le_overcurrent_state_t* s = (const le_overcurrent_state_t*)p;
            rows("pickup", s->pickup); rows("time_dial", s->time_dial);
            row("curve_type", std::to_string((int)s->curve_type));
            break;
        }
        case LE_BLK_PHASOR: {
            const le_phasor_state_t* s = (const le_phasor_state_t*)p;
            row("samples_per_cycle", std::to_string(s->samples_per_cycle));
            row("phasor", fmt_cx(s->phasor));
            rows("magnitude", s->magnitude); rows("angle_rad", s->angle_rad);
            break;
        }
        case LE_BLK_SYMCOMP: {
            const le_symcomp_state_t* s = (const le_symcomp_state_t*)p;
            row("phase_a", fmt_cx(s->phase_a)); row("phase_b", fmt_cx(s->phase_b));
            row("phase_c", fmt_cx(s->phase_c));
            break;
        }
        case LE_BLK_21: {
            const le_dist21_state_t* s = (const le_dist21_state_t*)p;
            rows("reach_ohms", s->reach_ohms); rows("line_angle_deg", s->line_angle_deg);
            rows("offset", s->offset_mag); rows("offset_angle_deg", s->offset_angle_deg);
            rows("prefault_v_threshold", s->prefault_v_threshold);
            row("prefault_v_duration_ms", std::to_string(s->prefault_duration_ms));
            break;
        }
        case LE_BLK_DIFF_87: {
            const le_diff87_state_t* s = (const le_diff87_state_t*)p;
            rows("o87p (pickup)", s->o87p);
            rows("slp1", s->slp1);
            rows("irs1 (knee)", s->irs1);
            rows("slp2", s->slp2);
            rows("operate", s->operate);
            rows("restraint", s->restraint);
            row("tripped", s->tripped ? "true" : "false");
            break;
        }
        case LE_BLK_PHASE_COMP: {
            const le_comp33_state_t* s = (const le_comp33_state_t*)p;
            row("comp (SEL k 1-12)", std::to_string((int)s->comp));
            break;
        }
        case LE_BLK_I2C: {
            const le_i2c_device_state_t* s = (const le_i2c_device_state_t*)p;
            row("addr_7bit", std::to_string((int)s->addr_7bit));
            row("poll_rate_ms", std::to_string(s->poll_rate_ms));
            row("poll_tx_len", std::to_string((int)s->poll_tx_len));
            row("poll_rx_len", std::to_string((int)s->poll_rx_len));
            row("data_dest_addr", std::to_string(s->data_dest_addr));
            break;
        }
        case LE_BLK_SPI: {
            const le_spi_device_state_t* s = (const le_spi_device_state_t*)p;
            row("cs_pin", std::to_string((int)s->cs_pin));
            row("poll_rate_ms", std::to_string(s->poll_rate_ms));
            row("poll_len", std::to_string((int)s->poll_len));
            row("data_dest_addr", std::to_string(s->data_dest_addr));
            break;
        }
        case LE_BLK_LPF: {
            const le_lpf_state_t* s = (const le_lpf_state_t*)p;
            rows("alpha", s->alpha);
            break;
        }
        case LE_BLK_BIQUAD: {
            const le_biquad_state_t* s = (const le_biquad_state_t*)p;
            rows("b0", s->b0); rows("b1", s->b1); rows("b2", s->b2);
            rows("a1", s->a1); rows("a2", s->a2);
            break;
        }
        case LE_BLK_MOVING_AVG: {
            const le_moving_avg_state_t* s = (const le_moving_avg_state_t*)p;
            row("window_size", std::to_string(s->window_size));
            break;
        }
        case LE_BLK_RATE_LIMITER: {
            const le_rate_limiter_state_t* s = (const le_rate_limiter_state_t*)p;
            rows("rising_rate", s->rising_rate); rows("falling_rate", s->falling_rate);
            break;
        }
        case LE_BLK_DEADBAND: {
            const le_deadband_state_t* s = (const le_deadband_state_t*)p;
            rows("threshold", s->threshold); rows("center", s->center);
            break;
        }
        case LE_BLK_WASHOUT: {
            const le_washout_state_t* s = (const le_washout_state_t*)p;
            rows("alpha", s->alpha);
            break;
        }
        case LE_BLK_PEAK: {
            const le_peak_state_t* s = (const le_peak_state_t*)p;
            rows("decay_rate", s->decay_rate);
            break;
        }
        case LE_BLK_RMS: {
            const le_rms_state_t* s = (const le_rms_state_t*)p;
            row("window_size", std::to_string(s->window_size));
            break;
        }
        case LE_BLK_MEDIAN: {
            const le_median_state_t* s = (const le_median_state_t*)p;
            row("window_size", std::to_string(s->window_size));
            break;
        }
        case LE_BLK_DERIVATIVE: {
            const le_derivative_state_t* s = (const le_derivative_state_t*)p;
            rows("alpha", s->alpha); rows("gain", s->gain);
            break;
        }
        case LE_BLK_ZERO_CROSSING: {
            const le_zero_crossing_state_t* s = (const le_zero_crossing_state_t*)p;
            rows("hysteresis", s->hysteresis); rows("sample_rate_hz", s->sample_rate_hz);
            break;
        }
        case LE_BLK_LUT_1D: {
            const le_lut_1d_state_t* s = (const le_lut_1d_state_t*)p;
            row("num_points", std::to_string(s->num_points));
            std::ostringstream xs, ys;
            for (int k = 0; k < LE_MAX_LUT_POINTS; k++) { if (k) { xs << " "; ys << " "; } xs << std::to_string(s->x[k]); ys << std::to_string(s->y[k]); }
            row("x[]", xs.str()); row("y[]", ys.str());
            break;
        }
        case LE_BLK_TOTALIZER: {
            const le_totalizer_state_t* s = (const le_totalizer_state_t*)p;
            rowd("accumulator", s->accumulator);
            rows("time_base_sec", s->time_base_sec); rows("scale_factor", s->scale_factor);
            rows("sample_time_sec", s->sample_time_sec); rows("max_limit", s->max_limit);
            break;
        }
        case LE_BLK_MIN_MAX_HOLD: {
            const le_min_max_hold_state_t* s = (const le_min_max_hold_state_t*)p;
            row("mode", std::to_string((int)s->mode));
            break;
        }
        default: break;
    }
}

/* Appends the preconfigured state image (the RAM block) as a hex dump plus a
 * per-element decoding of every state struct's properties. */
static void emit_state_section(std::ostringstream& out, const uint8_t* bin_data, size_t bin_len, const le_header_t& h)
{
    if (h.state_desc_count == 0 && h.state_img_len == 0) return;

    /* Locate the state-desc table and the state image (same layout as the loader). */
    size_t inst_end = sizeof(le_header_t) + (size_t)h.instruction_count * sizeof(le_instruction_t);
    size_t bt_size = 0;
    {
        size_t off = inst_end;
        for (uint16_t b = 0; b < h.block_count; b++) {
            if (off + (size_t)LE_BLOCK_DESC_HEADER_BYTES > bin_len) break;
            const le_block_desc_t* d = (const le_block_desc_t*)(bin_data + off);
            size_t dsize = (size_t)LE_BLOCK_DESC_HEADER_BYTES +
                           ((size_t)d->in_count + (size_t)d->out_count) * sizeof(uint16_t);
            if (off + dsize > bin_len) break;
            off += dsize; bt_size = off - inst_end;
        }
    }
    const uint8_t* st_desc = bin_data + inst_end + bt_size;
    const uint8_t* img = st_desc + (size_t)h.state_desc_count * LE_STATE_DESC_BYTES;

    out << "\nState RAM (preconfigured state image, " << h.state_img_len << " bytes):\n";
    out << "------------------------------------------------------------\n";
    for (size_t off = 0; off < h.state_img_len; off += 16) {
        out << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << off << ":  ";
        for (int k = 0; k < 16; k++) {
            if (off + (size_t)k < h.state_img_len) {
                out << std::setw(2) << std::setfill('0') << (int)img[off + k] << " ";
            } else {
                out << "   ";
            }
        }
        out << " | ";
        for (int k = 0; k < 16 && off + (size_t)k < h.state_img_len; k++) {
            uint8_t c = img[off + k];
            out << (char)((c >= 32 && c < 127) ? c : '.');
        }
        out << "\n";
    }
    out << std::dec;

    if (h.state_desc_count > 0) {
        out << "\nState properties per element:\n";
        out << "------------------------------------------------------------\n";
        size_t base = 0;
        for (uint16_t s = 0; s < h.state_desc_count; s++) {
            const le_state_desc_t* sd = (const le_state_desc_t*)(st_desc + (size_t)s * LE_STATE_DESC_BYTES);
            if (sd->kind == LE_BLK_NONE || sd->count == 0) continue;
            for (uint16_t i = 0; i < sd->count; i++) {
                size_t off = base + (size_t)i * sd->size;
                if (off + sd->size > h.state_img_len) break;
                out << block_kind_name(sd->kind) << "[" << i << "]  (LE_BLK_" << (int)sd->kind
                    << ", off=" << off << ", size=" << sd->size << ")\n";
                emit_state_props(out, sd->kind, img + off);
            }
            base += (size_t)sd->count * sd->size;
        }
    }
    out << "\n";
}

/* Prints the custom board nodes declared in a board profile (their function
 * ids, names, and declared I/O). Custom nodes are not stateful, so they are
 * not part of the state image; their config lives in the board profile. */
static void emit_custom_node_section(std::ostringstream& out, const std::string& board_profile_json)
{
    if (board_profile_json.empty()) return;
    try {
        JsonValue board = JsonValue::parse(board_profile_json);
        if (!board.is_object() || !board.contains("custom_nodes") || !board.get("custom_nodes").is_array())
            return;
        auto arr = board.get("custom_nodes").as_array();
        if (arr.empty()) return;

        out << "Custom board nodes (from board profile):\n";
        out << "------------------------------------------------------------\n";
        for (const auto& cn_val : arr) {
            if (!cn_val.is_object()) continue;
            std::string type_id = cn_val.get("type_id").as_string(cn_val.get("type").as_string(cn_val.get("name").as_string()));
            std::string name = cn_val.get("display_name").as_string(type_id);
            int fn_id = cn_val.get("function_id").as_int(1);
            std::string cat = cn_val.get("category").as_string("Custom");

            out << name << "  fn:0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                << fn_id << std::dec << "  (function_id=" << fn_id << ", type_id=\"" << type_id << "\", category=" << cat << ")\n";

            std::vector<std::string> ins, outs;
            if (cn_val.contains("inputs") && cn_val.get("inputs").is_array()) {
                for (const auto& p : cn_val.get("inputs").as_array()) {
                    if (!p.is_object()) continue;
                    std::string pn = p.get("name").as_string();
                    std::string pt = p.get("type").as_string("bool");
                    ins.push_back(pn + ":" + pt);
                }
            }
            if (cn_val.contains("outputs") && cn_val.get("outputs").is_array()) {
                for (const auto& p : cn_val.get("outputs").as_array()) {
                    if (!p.is_object()) continue;
                    std::string pn = p.get("name").as_string();
                    std::string pt = p.get("type").as_string("bool");
                    outs.push_back(pn + ":" + pt);
                }
            }
            out << "    inputs:  ";
            if (ins.empty()) out << "(none)";
            else for (size_t k = 0; k < ins.size(); k++) { if (k) out << ", "; out << ins[k]; }
            out << "\n";
            out << "    outputs: ";
            if (outs.empty()) out << "(none)";
            else for (size_t k = 0; k < outs.size(); k++) { if (k) out << ", "; out << outs[k]; }
            out << "\n";
        }
        out << "\n";
    } catch (...) {
        out << "Custom board nodes: (unable to parse board profile)\n";
    }
}

std::string disassemble_binary(const uint8_t* bin_data, size_t bin_len, int user_bool_count, int user_float_count, int user_int_count, const std::string& board_profile_json)
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
        << "  " << bool_summary << "  " << float_summary
        << "  StateGroups:" << h.state_desc_count
        << "  StateImage:" << h.state_img_len << " bytes\n";

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
    emit_state_section(out, bin_data, bin_len, h);
    if (!board_profile_json.empty()) {
        emit_custom_node_section(out, board_profile_json);
    }
    return out.str();
}

} // namespace LogicElements

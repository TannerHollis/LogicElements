/**
 * @file le_optimizer.cpp
 * @brief Implementation of the multi-pass optimizer for the LogicElements compiler.
 */

#include "le_optimizer.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace LogicElements {

static std::string to_upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static std::string to_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

Optimizer::Optimizer(const le_compiler_options_t& options)
    : m_options(options)
{
}

void Optimizer::build_ir(const std::vector<JsonValue>& elements,
                         const std::vector<JsonValue>& nets,
                         const std::map<std::string, std::string>& alias_to_addr,
                         const std::map<std::string, CustomNodeInfo>& custom_nodes)
{
    m_nodes.clear();
    m_execution_order.clear();
    m_stats = OptimizationStats();

    // 1. Create OptNodes for each element
    for (const auto& el : elements) {
        if (!el.is_object()) continue;
        std::string name = el.get("name").as_string();
        std::string raw_type = el.get("type").as_string();
        std::string type = to_upper(raw_type);

        OptNode node;
        node.name = name;
        node.type = type;
        node.raw_json = el;

        // Classify roots and stateful elements
        if (type == "DIGITALOUTPUT" || type == "OUTPUT" || (type == "LE_NODE_DIGITAL" && (name.rfind("OUT", 0) == 0 || name.rfind("DO", 0) == 0))) {
            node.is_root = true;
        } else if (type == "BOOLREGISTER" || type == "LE_BOOLREGISTER" ||
                   type == "INTREGISTER" || type == "LE_INTREGISTER" ||
                   type == "FLOATREGISTER" || type == "LE_FLOATREGISTER") {
            node.is_root = true;
        } else if (type.find("TAG") != std::string::npos) {
            std::string dir = to_lower(el.get("direction").as_string());
            if (dir == "send") node.is_root = true;
        } else if (type == "PID" || type == "LE_PID" ||
                   type == "OVERCURRENT_51" || type == "OVERCURRENT" || type == "LE_OVERCURRENT_51" || type == "LE_OVERCURRENT" ||
                   type == "PHASOR_1P" || type == "LE_PHASOR_1P" || type == "LE_1P_WINDING" ||
                   type == "SYM_COMP" || type == "LE_SYM_COMP" ||
                   type == "DIFF_87" || type == "DIFF" || type == "LE_DIFF_87" || type == "LE_DIFF" ||
                   type == "PHASE_COMP" || type == "TRANSFORM_33" || type == "TCOMP" || type == "LE_PHASE_COMP" ||
                   type == "DIST_21" || type == "LE_DIST_21" ||
                   type == "I2C" || type == "LE_I2C" ||
                   type == "SPI" || type == "LE_SPI" ||
                   type == "TON" || type == "LE_TON" ||
                   type == "TOF" || type == "LE_TOF" ||
                   type == "TP" || type == "LE_TP" ||
                   type == "CTU" || type == "LE_CTU" ||
                   type == "CTD" || type == "LE_CTD" ||
                   type == "CTUD" || type == "LE_CTUD" ||
                   type == "SR" || type == "LE_SR" ||
                   type == "RS" || type == "LE_RS" ||
                   type == "LATCH" || type == "LE_LATCH" ||
                   type == "RTRIG" || type == "LE_RTRIG" ||
                   type == "FTRIG" || type == "LE_FTRIG" ||
                   type == "LPF" || type == "LE_LPF" || type == "LPF_1P" ||
                   type == "BIQUAD" || type == "LE_BIQUAD" || type == "BIQUAD_IIR" ||
                   type == "MOVING_AVG" || type == "LE_MOVING_AVG" || type == "WINDOW_AVG" ||
                   type == "RATE_LIMITER" || type == "LE_RATE_LIMITER" || type == "SLEW_LIMITER" ||
                   type == "DEADBAND" || type == "LE_DEADBAND" ||
                   type == "WASHOUT" || type == "LE_WASHOUT" ||
                   type == "PEAK_DETECTOR" || type == "LE_PEAK_DETECTOR" || type == "ENVELOPE" ||
                   type == "RMS" || type == "LE_RMS" ||
                   type == "MEDIAN" || type == "LE_MEDIAN" || type == "MEDIAN_FILTER" ||
                   type == "DERIVATIVE" || type == "LE_DERIVATIVE" ||
                   type == "ZERO_CROSSING" || type == "LE_ZERO_CROSSING" ||
                   type == "LUT_1D" || type == "LE_LUT_1D" || type == "LUT" ||
                   type == "TOTALIZER" || type == "LE_TOTALIZER" ||
                   type == "MIN_MAX_HOLD" || type == "LE_MIN_MAX_HOLD" ||
                   type == "CUSTOMNODE" || type == "LE_CUSTOM" || type == "EXT_CALL" ||
                   custom_nodes.find(raw_type) != custom_nodes.end() ||
                   custom_nodes.find(el.get("custom_type").as_string()) != custom_nodes.end()) {
            node.is_stateful = true;
            node.is_root = true;
        } else if (type == "CONSTANT" || type == "LE_CONSTANT") {
            node.is_const = true;
            std::string dt = to_lower(el.get("dataType").as_string(el.get("data_type").as_string("bool")));
            node.const_type = dt;
            if (dt == "float") {
                node.const_float = el.get("value").as_float(0.0f);
            } else if (dt == "int" || dt == "integer") {
                node.const_int = el.get("value").as_int(0);
            } else {
                node.const_bool = el.get("value").as_bool(false);
            }
        }

        // Map opcodes for gates & math
        if (type == "AND" || type == "LE_AND") node.opcode = LE_OP_AND;
        else if (type == "OR" || type == "LE_OR") node.opcode = LE_OP_OR;
        else if (type == "NOT" || type == "LE_NOT") node.opcode = LE_OP_NOT;
        else if (type == "XOR" || type == "LE_XOR") node.opcode = LE_OP_XOR;
        else if (type == "NAND" || type == "LE_NAND") node.opcode = LE_OP_NAND;
        else if (type == "NOR" || type == "LE_NOR") node.opcode = LE_OP_NOR;
        else if (type == "MUX" || type == "LE_MUX") node.opcode = LE_OP_MUX;
        else if (type == "ADD" || type == "LE_ADD") node.opcode = LE_OP_ADD_F;
        else if (type == "SUB" || type == "SUBTRACT" || type == "LE_SUB") node.opcode = LE_OP_SUB_F;
        else if (type == "MUL" || type == "MULTIPLY" || type == "LE_MUL") node.opcode = LE_OP_MUL_F;
        else if (type == "DIV" || type == "DIVIDE" || type == "LE_DIV") node.opcode = LE_OP_DIV_F;
        else if (type == "CADD" || type == "LE_CADD" || type == "C_ADD") node.opcode = LE_OP_CADD_F;
        else if (type == "CSUB" || type == "LE_CSUB" || type == "C_SUB") node.opcode = LE_OP_CSUB_F;
        else if (type == "CMUL" || type == "LE_CMUL" || type == "C_MUL") node.opcode = LE_OP_CMUL_F;
        else if (type == "CDIV" || type == "LE_CDIV" || type == "C_DIV") node.opcode = LE_OP_CDIV_F;
        else if (type == "CMP_GT" || type == "LE_CMP_GT") node.opcode = LE_OP_CMP_GT;
        else if (type == "CMP_LT" || type == "LE_CMP_LT") node.opcode = LE_OP_CMP_LT;
        else if (type == "CMP_GE" || type == "LE_CMP_GE") node.opcode = LE_OP_CMP_GE;
        else if (type == "CMP_LE" || type == "LE_CMP_LE") node.opcode = LE_OP_CMP_LE;
        else if (type == "CMP_EQ" || type == "LE_CMP_EQ") node.opcode = LE_OP_CMP_EQ;
        else if (type == "CMP_NE" || type == "LE_CMP_NE") node.opcode = LE_OP_CMP_NE;
        else if (type == "LPF" || type == "LE_LPF" || type == "LPF_1P") node.opcode = LE_OP_LPF_1P;
        else if (type == "BIQUAD" || type == "LE_BIQUAD" || type == "BIQUAD_IIR") node.opcode = LE_OP_BIQUAD_IIR;
        else if (type == "MOVING_AVG" || type == "LE_MOVING_AVG" || type == "WINDOW_AVG") node.opcode = LE_OP_MOVING_AVG;
        else if (type == "RATE_LIMITER" || type == "LE_RATE_LIMITER" || type == "SLEW_LIMITER") node.opcode = LE_OP_RATE_LIMITER;
        else if (type == "DEADBAND" || type == "LE_DEADBAND") node.opcode = LE_OP_DEADBAND;
        else if (type == "WASHOUT" || type == "LE_WASHOUT") node.opcode = LE_OP_WASHOUT;
        else if (type == "PEAK_DETECTOR" || type == "LE_PEAK_DETECTOR" || type == "ENVELOPE") node.opcode = LE_OP_PEAK_DETECTOR;
        else if (type == "RMS" || type == "LE_RMS") node.opcode = LE_OP_RMS;
        else if (type == "MEDIAN" || type == "LE_MEDIAN" || type == "MEDIAN_FILTER") node.opcode = LE_OP_MEDIAN;
        else if (type == "DERIVATIVE" || type == "LE_DERIVATIVE") node.opcode = LE_OP_DERIVATIVE;
        else if (type == "ZERO_CROSSING" || type == "LE_ZERO_CROSSING") node.opcode = LE_OP_ZERO_CROSSING;
        else if (type == "LUT_1D" || type == "LE_LUT_1D" || type == "LUT") node.opcode = LE_OP_LUT_1D;
        else if (type == "TOTALIZER" || type == "LE_TOTALIZER") node.opcode = LE_OP_TOTALIZER;
        else if (type == "MIN_MAX_HOLD" || type == "LE_MIN_MAX_HOLD") node.opcode = LE_OP_MIN_MAX_HOLD;
        else if (type == "CLAMP" || type == "LE_CLAMP") node.opcode = LE_OP_BLOCK;
        else if (type == "COMPLEX2POLAR" || type == "LE_COMPLEX2POLAR") node.opcode = LE_OP_BLOCK;
        else if (type == "COMPLEX2RECT" || type == "LE_COMPLEX2RECT") node.opcode = LE_OP_BLOCK;
        else if (type == "RECT2COMPLEX" || type == "LE_RECT2COMPLEX") node.opcode = LE_OP_BLOCK;
        else if (type == "POLAR2COMPLEX" || type == "LE_POLAR2COMPLEX") node.opcode = LE_OP_BLOCK;
        else if (type == "RECT2POLAR" || type == "LE_RECT2POLAR") node.opcode = LE_OP_RECT2POLAR;
        else if (type == "POLAR2RECT" || type == "LE_POLAR2RECT") node.opcode = LE_OP_POLAR2RECT;
        else if (type == "PHASOR_SHIFT" || type == "LE_PHASOR_SHIFT") node.opcode = LE_OP_PHASOR_SHIFT;

        m_nodes[name] = node;
    }

    // 2. Map nets
    for (const auto& net : nets) {
        if (!net.is_object()) continue;
        std::string out_elem = net.get("output").get("name").as_string();
        for (const auto& inp : net.get("inputs").as_array()) {
            if (!inp.is_object()) continue;
            std::string target_elem = inp.get("name").as_string();
            std::string target_port = to_lower(inp.get("port").as_string("input_0"));
            if (m_nodes.find(target_elem) != m_nodes.end()) {
                m_nodes[target_elem].inputs[target_port] = out_elem;
            }
        }
    }

    // 3. Resolve TAG senders & receivers
    std::map<std::string, std::string> tag_senders;
    for (const auto& kv : m_nodes) {
        if (kv.second.type.find("TAG") != std::string::npos) {
            std::string dir = to_lower(kv.second.raw_json.get("direction").as_string());
            if (dir == "send") {
                std::string tag_name = kv.second.raw_json.get("tag_name").as_string(kv.first);
                auto it_in = kv.second.inputs.find("in");
                if (it_in != kv.second.inputs.end()) tag_senders[tag_name] = it_in->second;
                else if (!kv.second.inputs.empty()) tag_senders[tag_name] = kv.second.inputs.begin()->second;
            }
        }
    }

    for (auto& kv : m_nodes) {
        if (kv.second.type.find("TAG") != std::string::npos) {
            std::string dir = to_lower(kv.second.raw_json.get("direction").as_string());
            if (dir != "send") {
                std::string tag_name = kv.second.raw_json.get("tag_name").as_string(kv.first);
                if (tag_senders.find(tag_name) != tag_senders.end()) {
                    kv.second.inputs["in"] = tag_senders[tag_name];
                }
            }
        }
    }

    update_consumer_lists();
}

void Optimizer::update_consumer_lists()
{
    for (auto& kv : m_nodes) {
        kv.second.consumers.clear();
    }
    for (const auto& kv : m_nodes) {
        if (kv.second.is_dead) continue;
        for (const auto& port_src : kv.second.inputs) {
            std::string src = port_src.second;
            if (m_nodes.find(src) != m_nodes.end()) {
                m_nodes[src].consumers.push_back(kv.first);
            }
        }
    }
}

OptimizationStats Optimizer::run_passes()
{
    if (m_options.opt_level == LE_OPT_NONE) {
        compute_topological_order();
        return m_stats;
    }

    int max_iters = (m_options.max_pass_iterations > 0) ? m_options.max_pass_iterations : 5;
    for (int iter = 0; iter < max_iters; ++iter) {
        int prev_const = m_stats.folded_constants;
        int prev_inv = m_stats.folded_inversions;
        int prev_cse = m_stats.eliminated_subexprs;

        // Pass 1: Constant Folding & Propagation
        if (m_options.fold_constants) {
            pass_constant_folding();
        }

        // Pass 2: Inversion Folding into consumer modifiers
        if (m_options.fold_inversions) {
            pass_inversion_folding();
        }

        // Pass 3: Common Subexpression Elimination
        if (m_options.eliminate_common_subexpr) {
            pass_common_subexpression_elimination();
        }

        if (m_stats.folded_constants == prev_const &&
            m_stats.folded_inversions == prev_inv &&
            m_stats.eliminated_subexprs == prev_cse) {
            break;
        }
    }

    // Pass 4: Direct Destination Targeting (Eliminate redundant MOVE to DOUT/registers)
    if (m_options.direct_destination) {
        pass_direct_destination_coalescing();
    }

    // Pass 5: Dead Code Elimination
    if (m_options.eliminate_dead_code) {
        pass_dead_code_elimination();
    }

    compute_topological_order();
    return m_stats;
}

void Optimizer::pass_dead_code_elimination()
{
    update_consumer_lists();

    // Mark reachable nodes starting from roots
    std::set<std::string> reachable;
    std::vector<std::string> worklist;

    for (const auto& kv : m_nodes) {
        if (kv.second.is_dead) continue;
        // Physical inputs, outputs, registers, and stateful elements are roots
        if (kv.second.is_root || kv.second.is_stateful ||
            kv.second.type == "DIGITALINPUT" || kv.second.type == "INPUT" ||
            kv.second.type == "ANALOGINPUT" || kv.second.type == "LE_ANALOG_INPUT" ||
            (kv.second.type == "LE_NODE_DIGITAL" && (kv.first.rfind("IN", 0) == 0 || kv.first.rfind("DI", 0) == 0))) {
            reachable.insert(kv.first);
            worklist.push_back(kv.first);
        }
    }

    while (!worklist.empty()) {
        std::string curr = worklist.back();
        worklist.pop_back();

        if (m_nodes.find(curr) == m_nodes.end()) continue;
        const auto& node = m_nodes[curr];

        for (const auto& port_src : node.inputs) {
            const std::string& src = port_src.second;
            if (reachable.find(src) == reachable.end()) {
                reachable.insert(src);
                worklist.push_back(src);
            }
        }
    }

    // Prune unreachable nodes
    for (auto& kv : m_nodes) {
        if (!kv.second.is_dead && reachable.find(kv.first) == reachable.end()) {
            kv.second.is_dead = true;
            m_stats.eliminated_dead_nodes++;
            m_stats.eliminated_instructions++;
            m_stats.eliminated_registers++;
        }
    }

    update_consumer_lists();
}

void Optimizer::pass_constant_folding()
{
    compute_topological_order();

    for (const std::string& name : m_execution_order) {
        auto it = m_nodes.find(name);
        if (it == m_nodes.end() || it->second.is_dead) continue;
        OptNode& node = it->second;

        // Skip roots, stateful, and already constant nodes
        if (node.is_root || node.is_stateful || node.is_const) continue;

        // Check if inputs are constant
        auto find_input = [&](const std::vector<std::string>& ports) -> std::string {
            for (const auto& p : ports) {
                auto pit = node.inputs.find(p);
                if (pit != node.inputs.end()) return pit->second;
            }
            return "";
        };

        std::string in0_name = find_input({"a", "in_a", "in", "input", "input_0"});
        std::string in1_name = find_input({"b", "in_b", "input_1"});

        OptNode* in0 = (m_nodes.find(in0_name) != m_nodes.end()) ? &m_nodes[in0_name] : nullptr;
        OptNode* in1 = (m_nodes.find(in1_name) != m_nodes.end()) ? &m_nodes[in1_name] : nullptr;

        bool in0_is_const = in0 && in0->is_const;
        bool in1_is_const = in1 && in1->is_const;

        // Helper to forward node consumers to another node
        auto forward_to = [&](const std::string& target_name) {
            for (const std::string& cname : node.consumers) {
                auto itc = m_nodes.find(cname);
                if (itc != m_nodes.end()) {
                    for (auto& port_src : itc->second.inputs) {
                        if (port_src.second == node.name) {
                            port_src.second = target_name;
                        }
                    }
                }
            }
            node.is_dead = true;
            m_stats.folded_constants++;
            m_stats.eliminated_instructions++;
            m_stats.eliminated_registers++;
        };

        // Boolean Gate Constant Folding
        if (node.opcode == LE_OP_NOT && in0_is_const) {
            node.is_const = true;
            node.is_dead = true;
            node.const_type = "bool";
            node.const_bool = !in0->const_bool;
            m_stats.folded_constants++;
            m_stats.eliminated_instructions++;
            m_stats.eliminated_registers++;
        } else if (node.opcode == LE_OP_AND) {
            if (in0_is_const && in1_is_const) {
                node.is_const = true;
                node.is_dead = true;
                node.const_type = "bool";
                node.const_bool = in0->const_bool && in1->const_bool;
                m_stats.folded_constants++;
                m_stats.eliminated_instructions++;
                m_stats.eliminated_registers++;
            } else if (in0_is_const && !in0->const_bool) { // AND(false, x) -> false
                node.is_const = true;
                node.is_dead = true;
                node.const_type = "bool";
                node.const_bool = false;
                m_stats.folded_constants++;
                m_stats.eliminated_instructions++;
                m_stats.eliminated_registers++;
            } else if (in1_is_const && !in1->const_bool) { // AND(x, false) -> false
                node.is_const = true;
                node.is_dead = true;
                node.const_type = "bool";
                node.const_bool = false;
                m_stats.folded_constants++;
                m_stats.eliminated_instructions++;
                m_stats.eliminated_registers++;
            } else if (in0_is_const && in0->const_bool && !in1_name.empty()) { // AND(true, x) -> x
                forward_to(in1_name);
            } else if (in1_is_const && in1->const_bool && !in0_name.empty()) { // AND(x, true) -> x
                forward_to(in0_name);
            }
        } else if (node.opcode == LE_OP_OR) {
            if (in0_is_const && in1_is_const) {
                node.is_const = true;
                node.is_dead = true;
                node.const_type = "bool";
                node.const_bool = in0->const_bool || in1->const_bool;
                m_stats.folded_constants++;
                m_stats.eliminated_instructions++;
                m_stats.eliminated_registers++;
            } else if (in0_is_const && in0->const_bool) { // OR(true, x) -> true
                node.is_const = true;
                node.is_dead = true;
                node.const_type = "bool";
                node.const_bool = true;
                m_stats.folded_constants++;
                m_stats.eliminated_instructions++;
                m_stats.eliminated_registers++;
            } else if (in1_is_const && in1->const_bool) { // OR(x, true) -> true
                node.is_const = true;
                node.is_dead = true;
                node.const_type = "bool";
                node.const_bool = true;
                m_stats.folded_constants++;
                m_stats.eliminated_instructions++;
                m_stats.eliminated_registers++;
            } else if (in0_is_const && !in0->const_bool && !in1_name.empty()) { // OR(false, x) -> x
                forward_to(in1_name);
            } else if (in1_is_const && !in1->const_bool && !in0_name.empty()) { // OR(x, false) -> x
                forward_to(in0_name);
            }
        } else if (node.opcode == LE_OP_ADD_F && in0_is_const && in1_is_const) {
            float f = in0->const_float + in1->const_float;
            if (f == 0.0f || f == 1.0f) {
                node.is_const = true;
                node.is_dead = true;
                node.const_type = "float";
                node.const_float = f;
                m_stats.folded_constants++;
                m_stats.eliminated_instructions++;
                m_stats.eliminated_registers++;
            }
        } else if (node.opcode == LE_OP_SUB_F && in0_is_const && in1_is_const) {
            float f = in0->const_float - in1->const_float;
            if (f == 0.0f || f == 1.0f) {
                node.is_const = true;
                node.is_dead = true;
                node.const_type = "float";
                node.const_float = f;
                m_stats.folded_constants++;
                m_stats.eliminated_instructions++;
                m_stats.eliminated_registers++;
            }
        } else if (node.opcode == LE_OP_MUL_F && in0_is_const && in1_is_const) {
            float f = in0->const_float * in1->const_float;
            if (f == 0.0f || f == 1.0f) {
                node.is_const = true;
                node.is_dead = true;
                node.const_type = "float";
                node.const_float = f;
                m_stats.folded_constants++;
                m_stats.eliminated_instructions++;
                m_stats.eliminated_registers++;
            }
        } else if (node.opcode == LE_OP_DIV_F && in0_is_const && in1_is_const && in1->const_float != 0.0f) {
            float f = in0->const_float / in1->const_float;
            if (f == 0.0f || f == 1.0f) {
                node.is_const = true;
                node.is_dead = true;
                node.const_type = "float";
                node.const_float = f;
                m_stats.folded_constants++;
                m_stats.eliminated_instructions++;
                m_stats.eliminated_registers++;
            }
        }
    }
}

void Optimizer::pass_inversion_folding()
{
    update_consumer_lists();

    for (auto& kv : m_nodes) {
        OptNode& not_node = kv.second;
        if (not_node.is_dead || not_node.opcode != LE_OP_NOT) continue;

        auto it_in = not_node.inputs.find("in");
        if (it_in == not_node.inputs.end()) it_in = not_node.inputs.find("a");
        if (it_in == not_node.inputs.end()) it_in = not_node.inputs.find("input_0");
        if (it_in == not_node.inputs.end()) it_in = not_node.inputs.find("input");
        if (it_in == not_node.inputs.end() && !not_node.inputs.empty()) it_in = not_node.inputs.begin();
        if (it_in == not_node.inputs.end()) continue;

        std::string original_src = it_in->second;
        if (original_src.empty()) continue;

        // Check if all consumers support input inversion
        bool all_consumers_support_invert = true;
        for (const std::string& cname : not_node.consumers) {
            auto itc = m_nodes.find(cname);
            if (itc == m_nodes.end() || itc->second.is_dead) continue;
            uint8_t cop = itc->second.opcode;
            // Pure logic gates that support LE_MOD_INVERT_A / LE_MOD_INVERT_B
            bool can_invert = (cop >= LE_OP_NOT && cop <= LE_OP_NOR);
            if (!can_invert) {
                all_consumers_support_invert = false;
                break;
            }
        }

        if (!all_consumers_support_invert || not_node.consumers.empty()) continue;

        // Fold the inversion into each consumer
        for (const std::string& cname : not_node.consumers) {
            auto itc = m_nodes.find(cname);
            if (itc == m_nodes.end() || itc->second.is_dead) continue;
            OptNode& consumer = itc->second;

            if (consumer.opcode == LE_OP_NOT) {
                // Double negation: NOT(NOT(x)) -> x
                consumer.is_dead = true;
                for (const std::string& sub_cname : consumer.consumers) {
                    auto its = m_nodes.find(sub_cname);
                    if (its != m_nodes.end()) {
                        for (auto& p : its->second.inputs) {
                            if (p.second == consumer.name) {
                                p.second = original_src;
                            }
                        }
                    }
                }
                m_stats.folded_inversions++;
                m_stats.eliminated_instructions++;
                m_stats.eliminated_registers++;
                continue;
            }

            for (auto& port_src : consumer.inputs) {
                if (port_src.second == not_node.name) {
                    port_src.second = original_src;
                    std::string p = to_lower(port_src.first);
                    if (p == "a" || p == "in_a" || p == "in" || p == "input" || p == "input_0") {
                        consumer.modifier ^= LE_MOD_INVERT_A;
                    } else if (p == "b" || p == "in_b" || p == "input_1") {
                        consumer.modifier ^= LE_MOD_INVERT_B;
                    }
                }
            }
        }

        // Eliminate the NOT gate
        not_node.is_dead = true;
        m_stats.folded_inversions++;
        m_stats.eliminated_instructions++;
        m_stats.eliminated_registers++;
    }

    update_consumer_lists();
}

void Optimizer::pass_common_subexpression_elimination()
{
    update_consumer_lists();

    std::map<std::string, std::string> signature_to_node;

    for (const std::string& name : m_execution_order) {
        auto it = m_nodes.find(name);
        if (it == m_nodes.end() || it->second.is_dead || it->second.is_root || it->second.is_stateful) continue;
        OptNode& node = it->second;

        if (node.opcode == LE_OP_NOP) continue;

        auto find_input = [&](const std::vector<std::string>& ports) -> std::string {
            for (const auto& p : ports) {
                auto pit = node.inputs.find(p);
                if (pit != node.inputs.end()) return pit->second;
            }
            return "";
        };

        std::string in0 = find_input({"a", "in_a", "in", "input", "input_0"});
        std::string in1 = find_input({"b", "in_b", "input_1"});

        // Commutative sorting
        bool is_commutative = (node.opcode == LE_OP_AND || node.opcode == LE_OP_OR ||
                               node.opcode == LE_OP_XOR || node.opcode == LE_OP_NAND ||
                               node.opcode == LE_OP_NOR || node.opcode == LE_OP_ADD_F ||
                               node.opcode == LE_OP_MUL_F);
        if (is_commutative && in0 > in1) {
            std::swap(in0, in1);
        }

        std::ostringstream ss;
        ss << static_cast<int>(node.opcode) << ":" << static_cast<int>(node.modifier)
           << ":" << in0 << ":" << in1;
        std::string sig = ss.str();

        auto sit = signature_to_node.find(sig);
        if (sit != signature_to_node.end()) {
            // Found duplicate expression!
            std::string canonical_node = sit->second;

            // Rewire all consumers of this duplicate node to the canonical node
            for (const std::string& cname : node.consumers) {
                auto itc = m_nodes.find(cname);
                if (itc != m_nodes.end()) {
                    for (auto& port_src : itc->second.inputs) {
                        if (port_src.second == node.name) {
                            port_src.second = canonical_node;
                        }
                    }
                }
            }

            node.is_dead = true;
            m_stats.eliminated_subexprs++;
            m_stats.eliminated_instructions++;
            m_stats.eliminated_registers++;
        } else {
            signature_to_node[sig] = node.name;
        }
    }

    update_consumer_lists();
}

void Optimizer::pass_direct_destination_coalescing()
{
    update_consumer_lists();

    for (auto& kv : m_nodes) {
        OptNode& out_node = kv.second;
        if (out_node.is_dead) continue;

        bool is_dout = (out_node.type == "DIGITALOUTPUT" || out_node.type == "OUTPUT" ||
                       (out_node.type == "LE_NODE_DIGITAL" && (out_node.name.rfind("OUT", 0) == 0 || out_node.name.rfind("DO", 0) == 0)));
        bool is_bool_reg = (out_node.type == "BOOLREGISTER" || out_node.type == "LE_BOOLREGISTER");
        bool is_float_reg = (out_node.type == "FLOATREGISTER" || out_node.type == "LE_FLOATREGISTER");

        if (!is_dout && !is_bool_reg && !is_float_reg) continue;

        if (out_node.inputs.empty()) continue;
        std::string src_name = out_node.inputs.begin()->second;

        auto its = m_nodes.find(src_name);
        if (its == m_nodes.end() || its->second.is_dead || its->second.is_stateful) continue;
        OptNode& producer = its->second;

        // Producer must be an instruction-emitting gate or math block, not a physical input or constant
        if (producer.type == "DIGITALINPUT" || producer.type == "INPUT" || producer.type == "CONSTANT" ||
            (producer.type == "LE_NODE_DIGITAL" && (producer.name.rfind("IN", 0) == 0 || producer.name.rfind("DI", 0) == 0))) {
            continue;
        }

        if (is_dout || is_bool_reg) {
            bool is_bool_prod = (producer.opcode >= LE_OP_MOVE && producer.opcode <= LE_OP_NOR) ||
                                producer.opcode == LE_OP_MUX ||
                                (producer.opcode >= LE_OP_CMP_GT && producer.opcode <= LE_OP_CMP_NE);
            if (!is_bool_prod) continue;
        } else if (is_float_reg) {
            bool is_float_prod = (producer.opcode >= LE_OP_MOVE_F && producer.opcode <= LE_OP_MAX_F);
            if (!is_float_prod) continue;
        }

        // Producer must not already be directly targeting another output/register
        if (!producer.direct_dest_node.empty()) continue;

        // Coalesce: Producer writes directly into destination!
        producer.direct_dest_node = out_node.name;
        out_node.direct_dest_eliminated = true;

        m_stats.eliminated_moves++;
        m_stats.eliminated_instructions++;
        m_stats.eliminated_registers++;
    }

    update_consumer_lists();
}

void Optimizer::compute_topological_order()
{
    m_execution_order.clear();
    std::set<std::string> visited;

    std::function<void(const std::string&)> visit = [&](const std::string& name) {
        if (visited.find(name) != visited.end()) return;
        visited.insert(name);

        auto it = m_nodes.find(name);
        if (it != m_nodes.end()) {
            for (const auto& port_src : it->second.inputs) {
                if (m_nodes.find(port_src.second) != m_nodes.end()) {
                    visit(port_src.second);
                }
            }
        }
        m_execution_order.push_back(name);
    };

    for (const auto& kv : m_nodes) {
        visit(kv.first);
    }
}

} // namespace LogicElements

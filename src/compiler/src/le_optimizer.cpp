/**
 * @file le_optimizer.cpp
 * @brief Implementation of the multi-pass optimizer for the LogicElements compiler.
 */

#include "le_optimizer.h"
#include "le_element_registry.h"
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
                         const std::map<std::string, CustomNodeInfo>& custom_nodes,
                         const std::set<std::string>& feedback_state_nodes)
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

        if (feedback_state_nodes.count(name)) {
            node.is_root = true;
            node.is_feedback_state = true;
        }

        // Classify roots and stateful elements using central ElementRegistry
        std::string tag_dir = (type.find("TAG") != std::string::npos) ? to_lower(el.get("direction").as_string()) : "";
        if (ElementRegistry::is_stateful(type) ||
            type == "CUSTOMNODE" || type == "LE_CUSTOM" || type == "EXT_CALL" ||
            custom_nodes.find(raw_type) != custom_nodes.end() ||
            custom_nodes.find(el.get("custom_type").as_string()) != custom_nodes.end()) {
            node.is_stateful = true;
            node.is_root = true;
        } else if (ElementRegistry::is_root(type, tag_dir) ||
                   (type == "LE_NODE_DIGITAL" && (name.rfind("OUT", 0) == 0 || name.rfind("DO", 0) == 0))) {
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

        // Map opcodes for gates & math using ElementRegistry
        uint8_t op = ElementRegistry::get_optimizer_opcode(type);
        if (op != LE_OP_NOP) {
            node.opcode = op;
        }

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
    for (const auto& [name, node] : m_nodes) {
        if (node.type.find("TAG") != std::string::npos) {
            std::string dir = to_lower(node.raw_json.get("direction").as_string());
            if (dir == "send") {
                std::string tag_name = node.raw_json.get("tag_name").as_string(name);
                auto it_in = node.inputs.find("in");
                if (it_in != node.inputs.end()) tag_senders[tag_name] = it_in->second;
                else if (!node.inputs.empty()) tag_senders[tag_name] = node.inputs.begin()->second;
            }
        }
    }

    for (auto& [name, node] : m_nodes) {
        if (node.type.find("TAG") != std::string::npos) {
            std::string dir = to_lower(node.raw_json.get("direction").as_string());
            if (dir != "send") {
                std::string tag_name = node.raw_json.get("tag_name").as_string(name);
                if (tag_senders.find(tag_name) != tag_senders.end()) {
                    node.inputs["in"] = tag_senders[tag_name];
                }
            }
        }
    }

    update_consumer_lists();
}

void Optimizer::update_consumer_lists()
{
    for (auto& [name, node] : m_nodes) {
        node.consumers.clear();
    }
    for (const auto& [name, node] : m_nodes) {
        if (node.is_dead) continue;
        for (const auto& [port, src] : node.inputs) {
            if (m_nodes.find(src) != m_nodes.end()) {
                m_nodes[src].consumers.push_back(name);
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
        int prev_bool = m_stats.simplified_boolean_rules;
        int prev_fused = m_stats.fused_instructions;

        // Pass 1: Constant Folding & Propagation
        if (m_options.fold_constants) {
            pass_constant_folding();
        }

        // Pass 2: Boolean Algebraic Simplification
        if (m_options.simplify_boolean) {
            pass_boolean_simplification();
        }

        // Pass 3: Inversion Folding into consumer modifiers
        if (m_options.fold_inversions) {
            pass_inversion_folding();
        }

        // Pass 4: Common Subexpression Elimination
        if (m_options.eliminate_common_subexpr) {
            pass_common_subexpression_elimination();
        }

        // Pass 5: Instruction Fusion (FMA / FMS)
        if (m_options.fuse_instructions) {
            pass_instruction_fusion();
        }

        if (m_stats.folded_constants == prev_const &&
            m_stats.folded_inversions == prev_inv &&
            m_stats.eliminated_subexprs == prev_cse &&
            m_stats.simplified_boolean_rules == prev_bool &&
            m_stats.fused_instructions == prev_fused) {
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

    for (const auto& [name, node] : m_nodes) {
        if (node.is_dead) continue;
        // Physical inputs, outputs, registers, and stateful elements are roots
        if (node.is_root || node.is_stateful ||
            node.type == "DIGITALINPUT" || node.type == "INPUT" ||
            node.type == "ANALOGINPUT" || node.type == "LE_ANALOG_INPUT" ||
            node.type == "ANALOGOUTPUT" || node.type == "LE_ANALOGOUTPUT" ||
            node.type == "LE_ANALOG_OUTPUT" || node.type == "AOUT" ||
            (node.type == "LE_NODE_DIGITAL" && (name.rfind("IN", 0) == 0 || name.rfind("DI", 0) == 0))) {
            reachable.insert(name);
            worklist.push_back(name);
        }
    }

    while (!worklist.empty()) {
        std::string curr = worklist.back();
        worklist.pop_back();

        if (m_nodes.find(curr) == m_nodes.end()) continue;
        const auto& node = m_nodes[curr];
        if (node.is_dead) continue;

        for (const auto& port_src : node.inputs) {
            const std::string& src = port_src.second;
            if (reachable.find(src) == reachable.end()) {
                reachable.insert(src);
                worklist.push_back(src);
            }
        }
    }

    // Prune unreachable nodes
    for (auto& [name, node] : m_nodes) {
        if (!node.is_dead && reachable.find(name) == reachable.end()) {
            node.is_dead = true;
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

void Optimizer::pass_boolean_simplification()
{
    compute_topological_order();

    struct BoolLit {
        std::string node;
        bool inverted = false;

        bool empty() const { return node.empty(); }
        bool operator==(const BoolLit& o) const {
            return node == o.node && inverted == o.inverted;
        }
        bool is_opposite(const BoolLit& o) const {
            return node == o.node && inverted != o.inverted;
        }
    };

    auto find_input_port = [](const OptNode& n, const std::vector<std::string>& candidates) -> std::pair<std::string, std::string> {
        for (const auto& p : candidates) {
            auto it = n.inputs.find(p);
            if (it != n.inputs.end() && !it->second.empty()) {
                return {it->first, it->second};
            }
        }
        return {"", ""};
    };

    auto get_port_lit = [&](const OptNode& n, int port_idx, const std::string& src_name) -> BoolLit {
        if (src_name.empty()) return BoolLit();

        bool inv = false;
        if (port_idx == 0 && (n.modifier & LE_MOD_INVERT_A)) inv = !inv;
        if (port_idx == 1 && (n.modifier & LE_MOD_INVERT_B)) inv = !inv;

        std::string curr = src_name;
        std::set<std::string> visited;

        while (!curr.empty() && visited.insert(curr).second) {
            auto it = m_nodes.find(curr);
            if (it == m_nodes.end() || it->second.is_dead || it->second.is_root || it->second.is_stateful || it->second.is_const) {
                break;
            }
            if (it->second.opcode == LE_OP_NOT) {
                auto not_p = find_input_port(it->second, {"in", "a", "in_a", "in_0", "input_0", "input"});
                if (not_p.second.empty() && !it->second.inputs.empty()) {
                    not_p = *it->second.inputs.begin();
                }
                if (!not_p.second.empty()) {
                    inv = !inv;
                    if (it->second.modifier & LE_MOD_INVERT_A) {
                        inv = !inv;
                    }
                    curr = not_p.second;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        BoolLit lit;
        lit.node = curr;
        lit.inverted = inv;
        return lit;
    };

    auto simplify_to_const = [&](OptNode& node, bool val) {
        node.is_const = true;
        node.is_dead = true;
        node.const_type = "bool";
        node.const_bool = val;
        node.inputs.clear();
        m_stats.simplified_boolean_rules++;
        m_stats.eliminated_instructions++;
        m_stats.eliminated_registers++;
    };

    auto forward_to_node = [&](OptNode& node, const std::string& target) {
        if (node.name == target) return;
        for (const std::string& cname : node.consumers) {
            auto itc = m_nodes.find(cname);
            if (itc != m_nodes.end()) {
                for (auto& port_src : itc->second.inputs) {
                    if (port_src.second == node.name) {
                        port_src.second = target;
                    }
                }
            }
        }
        node.is_dead = true;
        m_stats.simplified_boolean_rules++;
        m_stats.eliminated_instructions++;
        m_stats.eliminated_registers++;
    };

    auto simplify_to_lit = [&](OptNode& node, const BoolLit& lit) {
        if (lit.empty()) return;
        if (!lit.inverted) {
            forward_to_node(node, lit.node);
        } else {
            // Convert to single NOT gate; inversion folding will fold into consumers if supported
            node.opcode = LE_OP_NOT;
            node.type = "NOT";
            node.modifier = 0;
            node.inputs.clear();
            node.inputs["in"] = lit.node;
            m_stats.simplified_boolean_rules++;
        }
    };

    for (const std::string& name : m_execution_order) {
        auto it = m_nodes.find(name);
        if (it == m_nodes.end() || it->second.is_dead || it->second.is_root || it->second.is_stateful || it->second.is_const) {
            continue;
        }
        OptNode& node = it->second;

        // Only evaluate 2-input boolean logic gates
        bool is_bool_gate = (node.opcode == LE_OP_AND || node.opcode == LE_OP_OR ||
                             node.opcode == LE_OP_XOR || node.opcode == LE_OP_NAND ||
                             node.opcode == LE_OP_NOR);
        if (!is_bool_gate) continue;
        if (node.inputs.size() > 2) continue; // Preserve wide fan-in gates (> 2 inputs)

        auto p0 = find_input_port(node, {"a", "in_a", "in_0", "input_0", "in", "input"});
        auto p1 = find_input_port(node, {"b", "in_b", "in_1", "input_1"});
        if (p0.second.empty() || p1.second.empty()) continue;

        BoolLit L0 = get_port_lit(node, 0, p0.second);
        BoolLit L1 = get_port_lit(node, 1, p1.second);
        if (L0.empty() || L1.empty()) continue;

        // 1. Annihilation: A op ~A
        if (L0.is_opposite(L1)) {
            if (node.opcode == LE_OP_AND) {
                simplify_to_const(node, false);
                continue;
            } else if (node.opcode == LE_OP_OR) {
                simplify_to_const(node, true);
                continue;
            } else if (node.opcode == LE_OP_XOR) {
                simplify_to_const(node, true);
                continue;
            } else if (node.opcode == LE_OP_NAND) {
                simplify_to_const(node, true);
                continue;
            } else if (node.opcode == LE_OP_NOR) {
                simplify_to_const(node, false);
                continue;
            }
        }

        // 2. Idempotence: A op A
        if (L0 == L1) {
            if (node.opcode == LE_OP_AND) {
                simplify_to_lit(node, L0);
                continue;
            } else if (node.opcode == LE_OP_OR) {
                simplify_to_lit(node, L0);
                continue;
            } else if (node.opcode == LE_OP_XOR) {
                simplify_to_const(node, false);
                continue;
            } else if (node.opcode == LE_OP_NAND) {
                simplify_to_lit(node, {L0.node, !L0.inverted});
                continue;
            } else if (node.opcode == LE_OP_NOR) {
                simplify_to_lit(node, {L0.node, !L0.inverted});
                continue;
            }
        }

        // 3. Absorption & Redundancy:
        // Case 3.1: OR(A, AND(B, C))
        if (node.opcode == LE_OP_OR) {
            // Check if port 1 is driven by an AND gate (non-inverted on this OR)
            if (!(node.modifier & LE_MOD_INVERT_B)) {
                auto it_sub = m_nodes.find(p1.second);
                if (it_sub != m_nodes.end() && !it_sub->second.is_dead &&
                    !it_sub->second.is_root && !it_sub->second.is_stateful &&
                    it_sub->second.opcode == LE_OP_AND && it_sub->second.inputs.size() <= 2) {
                    auto sp0 = find_input_port(it_sub->second, {"a", "in_a", "in_0", "input_0", "in", "input"});
                    auto sp1 = find_input_port(it_sub->second, {"b", "in_b", "in_1", "input_1"});
                    BoolLit sub_L0 = get_port_lit(it_sub->second, 0, sp0.second);
                    BoolLit sub_L1 = get_port_lit(it_sub->second, 1, sp1.second);

                    // A | (A & B) -> A
                    if (L0 == sub_L0 || L0 == sub_L1) {
                        simplify_to_lit(node, L0);
                        continue;
                    }
                    // A | (~A & B) -> A | B
                    if (L0.is_opposite(sub_L0) && !sub_L1.empty()) {
                        node.inputs[p1.first] = sub_L1.node;
                        if (sub_L1.inverted) node.modifier |= LE_MOD_INVERT_B;
                        else node.modifier &= ~LE_MOD_INVERT_B;
                        m_stats.simplified_boolean_rules++;
                        continue;
                    }
                    // A | (B & ~A) -> A | B
                    if (L0.is_opposite(sub_L1) && !sub_L0.empty()) {
                        node.inputs[p1.first] = sub_L0.node;
                        if (sub_L0.inverted) node.modifier |= LE_MOD_INVERT_B;
                        else node.modifier &= ~LE_MOD_INVERT_B;
                        m_stats.simplified_boolean_rules++;
                        continue;
                    }
                }
            }

            // Check if port 0 is driven by an AND gate (non-inverted on this OR)
            if (!(node.modifier & LE_MOD_INVERT_A)) {
                auto it_sub = m_nodes.find(p0.second);
                if (it_sub != m_nodes.end() && !it_sub->second.is_dead &&
                    !it_sub->second.is_root && !it_sub->second.is_stateful &&
                    it_sub->second.opcode == LE_OP_AND && it_sub->second.inputs.size() <= 2) {
                    auto sp0 = find_input_port(it_sub->second, {"a", "in_a", "in_0", "input_0", "in", "input"});
                    auto sp1 = find_input_port(it_sub->second, {"b", "in_b", "in_1", "input_1"});
                    BoolLit sub_L0 = get_port_lit(it_sub->second, 0, sp0.second);
                    BoolLit sub_L1 = get_port_lit(it_sub->second, 1, sp1.second);

                    // (A & B) | A -> A
                    if (L1 == sub_L0 || L1 == sub_L1) {
                        simplify_to_lit(node, L1);
                        continue;
                    }
                    // (~A & B) | A -> B | A
                    if (L1.is_opposite(sub_L0) && !sub_L1.empty()) {
                        node.inputs[p0.first] = sub_L1.node;
                        if (sub_L1.inverted) node.modifier |= LE_MOD_INVERT_A;
                        else node.modifier &= ~LE_MOD_INVERT_A;
                        m_stats.simplified_boolean_rules++;
                        continue;
                    }
                    // (B & ~A) | A -> B | A
                    if (L1.is_opposite(sub_L1) && !sub_L0.empty()) {
                        node.inputs[p0.first] = sub_L0.node;
                        if (sub_L0.inverted) node.modifier |= LE_MOD_INVERT_A;
                        else node.modifier &= ~LE_MOD_INVERT_A;
                        m_stats.simplified_boolean_rules++;
                        continue;
                    }
                }
            }
        }

        // Case 3.2: AND(A, OR(B, C))
        if (node.opcode == LE_OP_AND) {
            // Check if port 1 is driven by an OR gate (non-inverted on this AND)
            if (!(node.modifier & LE_MOD_INVERT_B)) {
                auto it_sub = m_nodes.find(p1.second);
                if (it_sub != m_nodes.end() && !it_sub->second.is_dead &&
                    !it_sub->second.is_root && !it_sub->second.is_stateful &&
                    it_sub->second.opcode == LE_OP_OR && it_sub->second.inputs.size() <= 2) {
                    auto sp0 = find_input_port(it_sub->second, {"a", "in_a", "in_0", "input_0", "in", "input"});
                    auto sp1 = find_input_port(it_sub->second, {"b", "in_b", "in_1", "input_1"});
                    BoolLit sub_L0 = get_port_lit(it_sub->second, 0, sp0.second);
                    BoolLit sub_L1 = get_port_lit(it_sub->second, 1, sp1.second);

                    // A & (A | B) -> A
                    if (L0 == sub_L0 || L0 == sub_L1) {
                        simplify_to_lit(node, L0);
                        continue;
                    }
                    // A & (~A | B) -> A & B
                    if (L0.is_opposite(sub_L0) && !sub_L1.empty()) {
                        node.inputs[p1.first] = sub_L1.node;
                        if (sub_L1.inverted) node.modifier |= LE_MOD_INVERT_B;
                        else node.modifier &= ~LE_MOD_INVERT_B;
                        m_stats.simplified_boolean_rules++;
                        continue;
                    }
                    // A & (B | ~A) -> A & B
                    if (L0.is_opposite(sub_L1) && !sub_L0.empty()) {
                        node.inputs[p1.first] = sub_L0.node;
                        if (sub_L0.inverted) node.modifier |= LE_MOD_INVERT_B;
                        else node.modifier &= ~LE_MOD_INVERT_B;
                        m_stats.simplified_boolean_rules++;
                        continue;
                    }
                }
            }

            // Check if port 0 is driven by an OR gate (non-inverted on this AND)
            if (!(node.modifier & LE_MOD_INVERT_A)) {
                auto it_sub = m_nodes.find(p0.second);
                if (it_sub != m_nodes.end() && !it_sub->second.is_dead &&
                    !it_sub->second.is_root && !it_sub->second.is_stateful &&
                    it_sub->second.opcode == LE_OP_OR && it_sub->second.inputs.size() <= 2) {
                    auto sp0 = find_input_port(it_sub->second, {"a", "in_a", "in_0", "input_0", "in", "input"});
                    auto sp1 = find_input_port(it_sub->second, {"b", "in_b", "in_1", "input_1"});
                    BoolLit sub_L0 = get_port_lit(it_sub->second, 0, sp0.second);
                    BoolLit sub_L1 = get_port_lit(it_sub->second, 1, sp1.second);

                    // (A | B) & A -> A
                    if (L1 == sub_L0 || L1 == sub_L1) {
                        simplify_to_lit(node, L1);
                        continue;
                    }
                    // (~A | B) & A -> B & A
                    if (L1.is_opposite(sub_L0) && !sub_L1.empty()) {
                        node.inputs[p0.first] = sub_L1.node;
                        if (sub_L1.inverted) node.modifier |= LE_MOD_INVERT_A;
                        else node.modifier &= ~LE_MOD_INVERT_A;
                        m_stats.simplified_boolean_rules++;
                        continue;
                    }
                    // (B | ~A) & A -> B & A
                    if (L1.is_opposite(sub_L1) && !sub_L0.empty()) {
                        node.inputs[p0.first] = sub_L0.node;
                        if (sub_L0.inverted) node.modifier |= LE_MOD_INVERT_A;
                        else node.modifier &= ~LE_MOD_INVERT_A;
                        m_stats.simplified_boolean_rules++;
                        continue;
                    }
                }
            }
        }

        // 4. Complementary Merging: (A & B) | (A & ~B) -> A, and dual (A | B) & (A | ~B) -> A
        if ((node.modifier & (LE_MOD_INVERT_A | LE_MOD_INVERT_B)) == 0) {
            if (node.opcode == LE_OP_OR) {
                auto it0 = m_nodes.find(p0.second);
                auto it1 = m_nodes.find(p1.second);
                if (it0 != m_nodes.end() && !it0->second.is_dead && !it0->second.is_root && !it0->second.is_stateful &&
                    it1 != m_nodes.end() && !it1->second.is_dead && !it1->second.is_root && !it1->second.is_stateful &&
                    it0->second.opcode == LE_OP_AND && it0->second.inputs.size() <= 2 &&
                    it1->second.opcode == LE_OP_AND && it1->second.inputs.size() <= 2) {
                    auto sp0_0 = find_input_port(it0->second, {"a", "in_a", "in_0", "input_0", "in", "input"});
                    auto sp0_1 = find_input_port(it0->second, {"b", "in_b", "in_1", "input_1"});
                    auto sp1_0 = find_input_port(it1->second, {"a", "in_a", "in_0", "input_0", "in", "input"});
                    auto sp1_1 = find_input_port(it1->second, {"b", "in_b", "in_1", "input_1"});

                    BoolLit P0 = get_port_lit(it0->second, 0, sp0_0.second);
                    BoolLit Q0 = get_port_lit(it0->second, 1, sp0_1.second);
                    BoolLit P1 = get_port_lit(it1->second, 0, sp1_0.second);
                    BoolLit Q1 = get_port_lit(it1->second, 1, sp1_1.second);

                    if (!P0.empty() && !Q0.empty() && !P1.empty() && !Q1.empty()) {
                        if (P0 == P1 && Q0.is_opposite(Q1)) {
                            simplify_to_lit(node, P0);
                            continue;
                        } else if (P0 == Q1 && Q0.is_opposite(P1)) {
                            simplify_to_lit(node, P0);
                            continue;
                        } else if (Q0 == P1 && P0.is_opposite(Q1)) {
                            simplify_to_lit(node, Q0);
                            continue;
                        } else if (Q0 == Q1 && P0.is_opposite(P1)) {
                            simplify_to_lit(node, Q0);
                            continue;
                        }
                    }
                }
            } else if (node.opcode == LE_OP_AND) {
                auto it0 = m_nodes.find(p0.second);
                auto it1 = m_nodes.find(p1.second);
                if (it0 != m_nodes.end() && !it0->second.is_dead && !it0->second.is_root && !it0->second.is_stateful &&
                    it1 != m_nodes.end() && !it1->second.is_dead && !it1->second.is_root && !it1->second.is_stateful &&
                    it0->second.opcode == LE_OP_OR && it0->second.inputs.size() <= 2 &&
                    it1->second.opcode == LE_OP_OR && it1->second.inputs.size() <= 2) {
                    auto sp0_0 = find_input_port(it0->second, {"a", "in_a", "in_0", "input_0", "in", "input"});
                    auto sp0_1 = find_input_port(it0->second, {"b", "in_b", "in_1", "input_1"});
                    auto sp1_0 = find_input_port(it1->second, {"a", "in_a", "in_0", "input_0", "in", "input"});
                    auto sp1_1 = find_input_port(it1->second, {"b", "in_b", "in_1", "input_1"});

                    BoolLit P0 = get_port_lit(it0->second, 0, sp0_0.second);
                    BoolLit Q0 = get_port_lit(it0->second, 1, sp0_1.second);
                    BoolLit P1 = get_port_lit(it1->second, 0, sp1_0.second);
                    BoolLit Q1 = get_port_lit(it1->second, 1, sp1_1.second);

                    if (!P0.empty() && !Q0.empty() && !P1.empty() && !Q1.empty()) {
                        if (P0 == P1 && Q0.is_opposite(Q1)) {
                            simplify_to_lit(node, P0);
                            continue;
                        } else if (P0 == Q1 && Q0.is_opposite(P1)) {
                            simplify_to_lit(node, P0);
                            continue;
                        } else if (Q0 == P1 && P0.is_opposite(Q1)) {
                            simplify_to_lit(node, Q0);
                            continue;
                        } else if (Q0 == Q1 && P0.is_opposite(P1)) {
                            simplify_to_lit(node, Q0);
                            continue;
                        }
                    }
                }
            }
        }
    }

    update_consumer_lists();
}

void Optimizer::pass_inversion_folding()
{
    update_consumer_lists();

    for (auto& [name, not_node] : m_nodes) {
        if (not_node.is_dead || not_node.is_root || not_node.is_feedback_state || not_node.opcode != LE_OP_NOT) continue;

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

        std::string in0 = find_input({"a", "in_a", "in", "input", "input_0", "in0"});
        std::string in1 = find_input({"b", "in_b", "input_1", "in1"});
        std::string in_sel = find_input({"sel", "select", "selector", "s", "in_sel"});
        std::string in2 = find_input({"c", "in_c", "in_2", "input_2", "acc", "addend"});

        // Commutative sorting
        bool is_commutative = (node.opcode == LE_OP_AND || node.opcode == LE_OP_OR ||
                               node.opcode == LE_OP_XOR || node.opcode == LE_OP_NAND ||
                               node.opcode == LE_OP_NOR || node.opcode == LE_OP_ADD_F ||
                               node.opcode == LE_OP_MUL_F || node.opcode == LE_OP_ADD_I ||
                               node.opcode == LE_OP_MUL_I || node.opcode == LE_OP_WORD_AND ||
                               node.opcode == LE_OP_WORD_OR || node.opcode == LE_OP_WORD_XOR);
        if (is_commutative && in0 > in1) {
            std::swap(in0, in1);
        }

        std::ostringstream ss;
        ss << static_cast<int>(node.opcode) << ":" << static_cast<int>(node.modifier)
           << ":" << in0 << ":" << in1 << ":" << in_sel << ":" << in2;
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

void Optimizer::pass_instruction_fusion()
{
    update_consumer_lists();

    for (auto& [name, node] : m_nodes) {
        if (node.is_dead || node.is_stateful || node.is_feedback_state) continue;

        if (node.opcode != LE_OP_ADD_F && node.opcode != LE_OP_SUB_F) continue;

        // An addition or subtraction needs at least two inputs
        std::string in_a_src, in_b_src;
        for (const auto& inp : node.inputs) {
            std::string k = to_lower(inp.first);
            if (k == "a" || k == "in_a" || k == "input_0" || k == "in") in_a_src = inp.second;
            else if (k == "b" || k == "in_b" || k == "input_1") in_b_src = inp.second;
        }
        if (in_a_src.empty() && node.inputs.size() >= 1) in_a_src = node.inputs.begin()->second;
        if (in_b_src.empty() && node.inputs.size() >= 2) {
            auto it = node.inputs.begin();
            ++it;
            in_b_src = it->second;
        }
        if (in_a_src.empty() || in_b_src.empty()) continue;

        // Check if a node is a single-consumer multiplication suitable for fusion
        auto is_fusable_mul = [&](const std::string& name) -> bool {
            auto it = m_nodes.find(name);
            if (it == m_nodes.end()) return false;
            const OptNode& m = it->second;
            if (m.is_dead || m.is_root || m.is_stateful || m.is_feedback_state) return false;
            if (!m.direct_dest_node.empty()) return false;
            if (m.opcode != LE_OP_MUL_F) return false;
            if (m.consumers.size() != 1 || m.consumers[0] != node.name) return false;
            return true;
        };

        if (node.opcode == LE_OP_ADD_F) {
            std::string mul_name;
            std::string addend_name;
            if (is_fusable_mul(in_a_src)) {
                mul_name = in_a_src;
                addend_name = in_b_src;
            } else if (is_fusable_mul(in_b_src)) {
                mul_name = in_b_src;
                addend_name = in_a_src;
            } else {
                continue;
            }

            OptNode& mul_node = m_nodes[mul_name];
            std::string mul_in_a, mul_in_b;
            for (const auto& inp : mul_node.inputs) {
                std::string k = to_lower(inp.first);
                if (k == "a" || k == "in_a" || k == "input_0" || k == "in") mul_in_a = inp.second;
                else if (k == "b" || k == "in_b" || k == "input_1") mul_in_b = inp.second;
            }
            if (mul_in_a.empty() && mul_node.inputs.size() >= 1) mul_in_a = mul_node.inputs.begin()->second;
            if (mul_in_b.empty() && mul_node.inputs.size() >= 2) {
                auto it = mul_node.inputs.begin();
                ++it;
                mul_in_b = it->second;
            }
            if (mul_in_a.empty() || mul_in_b.empty()) continue;

            // Fuse: node becomes FMA (a * b + c)
            node.opcode = LE_OP_FMA_F;
            node.type = "FMA";
            node.inputs.clear();
            node.inputs["a"] = mul_in_a;
            node.inputs["b"] = mul_in_b;
            node.inputs["c"] = addend_name;

            mul_node.is_dead = true;
            mul_node.inputs.clear();

            m_stats.fused_instructions++;
            m_stats.eliminated_instructions++;
            m_stats.eliminated_registers++;
        } else if (node.opcode == LE_OP_SUB_F) {
            // In SUB: out = in_a - in_b.
            // When in_b is the product (a*b): out = in_a - (a*b), which is FMS!
            if (!is_fusable_mul(in_b_src)) continue;

            std::string mul_name = in_b_src;
            std::string addend_name = in_a_src;

            OptNode& mul_node = m_nodes[mul_name];
            std::string mul_in_a, mul_in_b;
            for (const auto& inp : mul_node.inputs) {
                std::string k = to_lower(inp.first);
                if (k == "a" || k == "in_a" || k == "input_0" || k == "in") mul_in_a = inp.second;
                else if (k == "b" || k == "in_b" || k == "input_1") mul_in_b = inp.second;
            }
            if (mul_in_a.empty() && mul_node.inputs.size() >= 1) mul_in_a = mul_node.inputs.begin()->second;
            if (mul_in_b.empty() && mul_node.inputs.size() >= 2) {
                auto it = mul_node.inputs.begin();
                ++it;
                mul_in_b = it->second;
            }
            if (mul_in_a.empty() || mul_in_b.empty()) continue;

            // Fuse: node becomes FMS (c - a * b)
            node.opcode = LE_OP_FMS_F;
            node.type = "FMS";
            node.inputs.clear();
            node.inputs["a"] = mul_in_a;
            node.inputs["b"] = mul_in_b;
            node.inputs["c"] = addend_name;

            mul_node.is_dead = true;
            mul_node.inputs.clear();

            m_stats.fused_instructions++;
            m_stats.eliminated_instructions++;
            m_stats.eliminated_registers++;
        }
    }

    update_consumer_lists();
}

void Optimizer::pass_direct_destination_coalescing()
{
    update_consumer_lists();

    for (auto& [name, out_node] : m_nodes) {
        if (out_node.is_dead) continue;

        bool is_dout = (out_node.type == "DIGITALOUTPUT" || out_node.type == "OUTPUT" ||
                       (out_node.type == "LE_NODE_DIGITAL" && (out_node.name.rfind("OUT", 0) == 0 || out_node.name.rfind("DO", 0) == 0)));
        bool is_aout = (out_node.type == "ANALOGOUTPUT" || out_node.type == "LE_ANALOGOUTPUT" ||
                       out_node.type == "LE_ANALOG_OUTPUT" || out_node.type == "AOUT");
        bool is_bool_reg = (out_node.type == "BOOLREGISTER" || out_node.type == "LE_BOOLREGISTER");
        bool is_float_reg = (out_node.type == "FLOATREGISTER" || out_node.type == "LE_FLOATREGISTER");

        if (!is_dout && !is_aout && !is_bool_reg && !is_float_reg) continue;

        if (out_node.inputs.empty()) continue;
        std::string src_name = out_node.inputs.begin()->second;

        auto its = m_nodes.find(src_name);
        if (its == m_nodes.end() || its->second.is_dead || its->second.is_stateful || its->second.is_feedback_state) continue;
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
        } else if (is_float_reg || is_aout) {
            bool is_float_prod = (producer.opcode >= LE_OP_MOVE_F && producer.opcode <= LE_OP_MAX_F) ||
                                 (producer.opcode == LE_OP_FMA_F) || (producer.opcode == LE_OP_FMS_F);
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
    std::set<std::string> visiting;
    std::set<std::string> visited;

    auto is_reg = [&](const std::string& node_name) -> bool {
        auto it = m_nodes.find(node_name);
        if (it == m_nodes.end()) return false;
        if (it->second.is_feedback_state) return true;
        const std::string& t = it->second.type;
        return (t == "BOOLREGISTER" || t == "LE_BOOLREGISTER" ||
                t == "INTREGISTER" || t == "LE_INTREGISTER" ||
                t == "FLOATREGISTER" || t == "LE_FLOATREGISTER" ||
                t == "COMPLEXREGISTER" || t == "LE_COMPLEXREGISTER");
    };

    auto can_reach = [&](const std::string& start, const std::string& target) -> bool {
        std::set<std::string> seen;
        std::vector<std::string> stack = { start };
        while (!stack.empty()) {
            std::string curr = stack.back();
            stack.pop_back();
            if (curr == target) return true;
            if (seen.count(curr)) continue;
            seen.insert(curr);
            auto itn = m_nodes.find(curr);
            if (itn != m_nodes.end()) {
                for (const auto& inp : itn->second.inputs) {
                    if (!inp.second.empty() && !seen.count(inp.second)) {
                        stack.push_back(inp.second);
                    }
                }
            }
        }
        return false;
    };

    std::function<void(const std::string&)> visit = [&](const std::string& name) {
        if (visited.find(name) != visited.end()) return;
        if (visiting.find(name) != visiting.end()) {
            // Cycle back-edge detected: break cycle
            return;
        }
        visiting.insert(name);

        auto it = m_nodes.find(name);
        if (it != m_nodes.end()) {
            for (const auto& port_src : it->second.inputs) {
                const std::string& src_name = port_src.second;
                if (src_name.empty()) continue;

                // In synchronous digital logic, when a register is part of a feedback loop
                // (i.e. the register's input path depends on this node), the node is reading
                // the register's previous state from memory. Traversing into the register in
                // that cycle would invert the feedback loop and schedule the register's write
                // BEFORE the combinatorial logic that computes its input!
                // For acyclic pipelines (no feedback path back to this node), dependencies
                // are preserved normally.
                if (is_reg(src_name) && can_reach(src_name, name)) {
                    continue;
                }
                if (m_nodes.find(src_name) != m_nodes.end()) {
                    visit(src_name);
                }
            }
        }
        visiting.erase(name);
        visited.insert(name);
        m_execution_order.push_back(name);
    };

    for (const auto& [name, node] : m_nodes) {
        visit(name);
    }
}

} // namespace LogicElements

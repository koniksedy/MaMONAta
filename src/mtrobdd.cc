#include "mtrobdd.hh"

namespace mamonata::mtrobdd
{
    MtBddNodePtr MtRobdd::insert_bit_string(MtBddNodePtr src_node, const VarIndex var_index, const BitVector& bit_string, const NodeValue terminal_value) {
        assert(!bit_string.empty());
        if (var_index == static_cast<VarIndex>(num_of_vars)) {
            return create_terminal_node(terminal_value);
        }

        Bit current_bit = bit_string[var_index];

        // If the source node is null, we need to create new nodes along the path
        if (src_node == nullptr) {
            if (current_bit == LO) {
                MtBddNodePtr low_child = insert_bit_string(nullptr, var_index + 1, bit_string, terminal_value);
                return create_node(var_index, low_child, nullptr);
            }
            MtBddNodePtr high_child = insert_bit_string(nullptr, var_index + 1, bit_string, terminal_value);
            return create_node(var_index, nullptr, high_child);
        }

        // The source node exists, we need to traverse the existing path
        MtBddNodePtr low_child = src_node->low;
        MtBddNodePtr high_child = src_node->high;
        if (current_bit == LO) {
            low_child = insert_bit_string(src_node->low, var_index + 1, bit_string, terminal_value);
        } else {
            high_child = insert_bit_string(src_node->high, var_index + 1, bit_string, terminal_value);
        }

        if (low_child == src_node->low && high_child == src_node->high) {
            // No changes were made, return the original node.
            return src_node;
        }

        return create_node(var_index, low_child, high_child);
    }

    MtRobdd& MtRobdd::trim() {
        NodeSet usefull_nodes;

        std::stack<MtBddNodePtr> worklist;
        for (const auto& [name, root_node] : root_nodes_map) {
            worklist.push(root_node);
            usefull_nodes.insert(root_node);
        }

        while (!worklist.empty()) {
            MtBddNodePtr current_node = worklist.top();
            worklist.pop();

            if (current_node->low != nullptr && !usefull_nodes.contains(current_node->low)) {
                usefull_nodes.insert(current_node->low);
                worklist.push(current_node->low);
            }
            if (current_node->high != nullptr && !usefull_nodes.contains(current_node->high)) {
                usefull_nodes.insert(current_node->high);
                worklist.push(current_node->high);
            }
        }

        nodes = std::move(usefull_nodes);
        return *this;
    }

    MtRobdd& MtRobdd::remove_redundant_tests() {
        NodeSet new_nodes;
        NameToNodeMap new_root_nodes_map;

        std::function<MtBddNodePtr(const MtBddNodePtr)> remove_redundant_rec = [&](const MtBddNodePtr node) -> MtBddNodePtr {
            if (node == nullptr) {
                return node;
            }

            if (node->is_terminal()) {
                if (!new_nodes.contains(node)) {
                    new_nodes.insert(node);
                }
                return node;
            }

            MtBddNodePtr low_child = remove_redundant_rec(node->low);
            MtBddNodePtr high_child = remove_redundant_rec(node->high);

            if (low_child != nullptr && low_child == high_child) {
                return low_child;
            }

            MtBddNodePtr new_node = create_node(node->var_index, low_child, high_child, node->value);
            if (!new_nodes.contains(new_node)) {
                new_nodes.insert(new_node);
            }
            return *new_nodes.find(new_node);
        };

        for (const auto& [name, root_node] : root_nodes_map) {
            MtBddNodePtr new_root = remove_redundant_rec(root_node);
            new_root_nodes_map[name] = new_root;
        }
        nodes = std::move(new_nodes);
        root_nodes_map = std::move(new_root_nodes_map);

        return *this;
    }

    MtRobdd& MtRobdd::make_complete(const NodeValue sink_value, const bool complete_terminal_nodes) {
        MtBddNodePtr terminal_sink = std::make_shared<MtBddNode>(TERMINAL_INDEX, nullptr, nullptr, sink_value);
        bool used_sink = false;

        for (const auto& node : nodes) {
            // Complete terminal nodes that don't have an appropriate root mapping
            if (complete_terminal_nodes && node->is_terminal()) {
                if (!root_nodes_map.contains(node->value)) {
                    root_nodes_map[node->value] = terminal_sink;
                    used_sink = true;
                }
                continue;
            }

            if (node->low == nullptr) {
                node->low = terminal_sink;
                used_sink = true;
            }
            if (node->high == nullptr) {
                node->high = terminal_sink;
                used_sink = true;
            }
        }

        if (used_sink) {
            nodes.insert(terminal_sink);
            root_nodes_map[sink_value] = terminal_sink;
        }

        return *this;
    }

    void MtRobdd::to_dot(std::ostream& os) const {
        // Group nodes by variable index
        std::unordered_map<VarIndex, std::vector<MtBddNodePtr>> levels;
        for (const auto& node : nodes) {
            levels[node->var_index].push_back(node);
        }

        // Header
        os << "digraph MtRobdd {\n";
        os << "  rankdir=LR;\n";

        // Define pre-root nodes
        os << "  node [shape=circle];\n";
        os << "  // Pre-root nodes\n";
        os << "  { rank=same; ";
        for (const auto& [name, root_node] : root_nodes_map) {
            std::string name_str = name == SINK_VALUE ? "sink" : std::to_string(name);
            os << name << " [label=\""<< name_str << "\"]; ";
        }
        os << "}\n";

        // Define non-terminal nodes
        os << "  node [shape=box];\n";
        for (size_t var_index = 0; var_index < num_of_vars; ++var_index) {
            os << "  // Level " << var_index << "\n";
            os << "  { rank=same; ";
            for (const auto& node : levels[var_index]) {
                os << reinterpret_cast<std::uintptr_t>(node.get()) << " [label=\"Var " << node->var_index << "\"]; ";
            }
            os << "}\n";
        }

        // Define terminal nodes
        os << "  node [shape=doublecircle];\n";
        os << "  // Terminal nodes\n";
        os << "  { rank=same; ";
        for (const auto& node : levels.at(TERMINAL_INDEX)) {
            std::string label = node->value == SINK_VALUE ? "sink" : std::to_string(node->value);
            os << reinterpret_cast<std::uintptr_t>(node.get()) << " [label=\"" << label << "\"]; ";
        }
        os << "}\n";

        // Define edges from pre-root nodes to root nodes
        os << "  // Edges from pre-root nodes\n";
        for (const auto& [name, root_node] : root_nodes_map) {
            os << "  " << name << " -> " << reinterpret_cast<std::uintptr_t>(root_node.get()) << ";\n";
        }

        // Define edges between nodes
        os << "  // Edges between nodes\n";
        for (const auto& node : nodes) {
            if (node->low != nullptr) {
                os << "  " << reinterpret_cast<std::uintptr_t>(node.get()) << " -> " << reinterpret_cast<std::uintptr_t>(node->low.get()) << " [label=\"0\"];\n";
            }
            if (node->high != nullptr) {
                os << "  " << reinterpret_cast<std::uintptr_t>(node.get()) << " -> " << reinterpret_cast<std::uintptr_t>(node->high.get()) << " [label=\"1\"];\n";
            }
        }
        os << "}\n";
    }
}

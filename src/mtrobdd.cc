#include "mtrobdd.hh"

namespace mamonata::mtrobdd
{

MtRobdd& MtRobdd::from_mona(const size_t num_of_vars, bdd_manager* bddm, bdd_ptr* root_behavior_ptrs, const size_t num_of_roots) {
    this->num_of_vars = num_of_vars;

    // Prepare MONA BDD manager for transfer
    Table *table = tableInit();
    bdd_prepare_apply1(bddm);

    // Build table of tuples (idx,lo,hi)
    for (size_t i = 0; i < num_of_roots; i++) {
        _export(bddm, root_behavior_ptrs[i], table);
    }

    // Renumber lo/hi pointers to new table ordering
    for (size_t i = 0; i < table->noelems; i++) {
        if (table->elms[i].idx != -1) {
            table->elms[i].lo = bdd_mark(bddm, table->elms[i].lo) - 1;
            table->elms[i].hi = bdd_mark(bddm, table->elms[i].hi) - 1;
        }
    }

    // Get MTROBDD root node behavior pointers
    std::unordered_map<NodeName, bdd_ptr> mona_to_mtrobdd_root_name_map;
    for (NodeName state = 0; state < num_of_roots; ++state) {
        bdd_ptr mona_node_ptr = bdd_mark(bddm, root_behavior_ptrs[state]) - 1;
        mona_to_mtrobdd_root_name_map[state] = mona_node_ptr;
    }

    // Create MTROBDD nodes placeholders
    std::unordered_map<bdd_ptr, MtBddNodePtr> mona_to_mtrobdd_node_map;
    for (bdd_ptr i = 0; i < table->noelems; ++i) {
        mona_to_mtrobdd_node_map[i] = std::make_shared<mtrobdd::MtBddNode>(table->elms[i].idx);
    }

    // Fill LOW/HIGH pointers and values of MTROBDD nodes
    // Insert initialized nodes into the MTROBDD manager
    for (bdd_ptr i = 0; i < table->noelems; ++i) {
        MtBddNodePtr mtrobdd_node = mona_to_mtrobdd_node_map[i];
        if (table->elms[i].idx == -1) {
            // terminal node
            mtrobdd_node->value = static_cast<NodeValue>(table->elms[i].lo);
        } else {
            // inner node
            mtrobdd_node->low = mona_to_mtrobdd_node_map[table->elms[i].lo];
            mtrobdd_node->high = mona_to_mtrobdd_node_map[table->elms[i].hi];
        }
        nodes.insert(mtrobdd_node);
    }

    // Set MTROBDD root nodes
    for (const auto& [root_name, mona_node_ptr]: mona_to_mtrobdd_root_name_map) {
        root_nodes_map[root_name] = mona_to_mtrobdd_node_map[mona_node_ptr];
    }

    tableFree(table);

    return *this;
}

void MtRobdd::to_mona(bdd_manager* bddm, bdd_ptr* root_behavior_ptrs) const {
    // assert for each i in 0 .. root_nodes_map.size()-1 there is a root with name i
    assert(std::all_of(root_nodes_map.begin(), root_nodes_map.end(),
                        [this](const auto& pair) {
                            return pair.first >= 0 && pair.first < this->root_nodes_map.size();
                        }));

    const size_t num_of_nodes = nodes.size();

    // Create positioning for each node in MTROBDD
    NodeToNameMap node_to_position_map;
    size_t position = 0;
    for (const auto& node : nodes) {
        node_to_position_map[node] = position++;
    }

    // Set root behavior pointers
    for (const auto& [root_name, root_node] : root_nodes_map) {
        root_behavior_ptrs[root_name] = static_cast<bdd_ptr>(node_to_position_map[root_node]);
    }

    // Create MONA BDD node table
    BddNode* node_table = (BddNode*)mem_alloc(sizeof(BddNode) * num_of_nodes);

    // Recursive function to create MONA nodes and initialize BDD manager
    std::function<bdd_ptr(bdd_ptr)> make_mona_node = [&](bdd_ptr i) -> bdd_ptr {
        if (node_table[i].p != -1) {
            return node_table[i].p;
        }

        if (node_table[i].idx == -1) {
            // Terminal node
            node_table[i].p = bdd_find_leaf_sequential(bddm, node_table[i].lo);
        } else {
            assert(node_table[i].lo != node_table[i].hi);
            node_table[i].lo = make_mona_node(node_table[i].lo);
            node_table[i].hi = make_mona_node(node_table[i].hi);
            node_table[i].p = bdd_find_node_sequential(bddm,
                                                       node_table[i].lo,
                                                       node_table[i].hi,
                                                       node_table[i].idx);
        }

        return node_table[i].p;
    };

    // Create all MONA nodes
    for (const auto [node, node_pos]: node_to_position_map) {
        node_table[node_pos].p = -1; // mark as uncreated
        if (node->is_terminal()) {
            // MONA stores terminal value in 'lo' field
            node_table[node_pos].lo = static_cast<unsigned>(node->value);
            node_table[node_pos].hi = 0;
            node_table[node_pos].idx = -1;
        } else {
            assert(node->high != nullptr);
            assert(node->low != nullptr);
            node_table[node_pos].lo = static_cast<bdd_ptr>(node_to_position_map[node->low]);
            node_table[node_pos].hi = static_cast<bdd_ptr>(node_to_position_map[node->high]);
            node_table[node_pos].idx = static_cast<int>(node->var_index);
        }
    }

    // Fill the roots_behavior array with actual MONA node pointers
    for (NodeName root_name = 0; root_name < root_nodes_map.size(); ++root_name) {
        root_behavior_ptrs[root_name] = make_mona_node(root_behavior_ptrs[root_name]);
    }

    // Free MONA node table
    mem_free(node_table);
}

MtBddNodePtr MtRobdd::insert_bit_string(const MtBddNodePtr src_node, const VarIndex var_index, const BitVector& bit_string, const NodeValue terminal_value) {
    assert(!bit_string.empty());

    // Base case: reached the end of the bit string
    if (var_index == static_cast<VarIndex>(num_of_vars)) {
        return create_terminal_node(terminal_value);
    }

    Bit current_bit = bit_string[var_index];

    // If the source node is null (does not exist),
    // we need to create new node with appropriate children
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

    // If no changes were made, return the original node
    if (low_child == src_node->low && high_child == src_node->high) {
        return src_node;
    }

    return create_node(var_index, low_child, high_child);
}

std::vector<std::pair<BitVector, NodeValue>> MtRobdd::get_all_bit_strings_from_root_node(const MtBddNodePtr root_node) const {
    // Helper function to calculate transition length.
    auto get_transition_length = [&](const VarIndex src_idx, const VarIndex tgt_idx) -> size_t {
        if (tgt_idx == TERMINAL_INDEX) {
            return num_of_vars - src_idx;
        }
        return tgt_idx - src_idx;
    };

    // Recursive function to expand bit strings with don't cares into all combinations.
    std::function<std::vector<BitVector>(const BitVector&, size_t)> expand_with_dont_cares =
    [&](const BitVector& prefix, const size_t dont_care_count) -> std::vector<BitVector> {
        if (dont_care_count == 0) {
            return { prefix };
        }

        std::vector<BitVector> result;

        // Expand with LO
        BitVector with_lo = prefix;
        with_lo.push_back(LO);
        auto lo_expanded = expand_with_dont_cares(with_lo, dont_care_count - 1);
        result.insert(result.end(), lo_expanded.begin(), lo_expanded.end());

        // Expand with HI
        BitVector with_hi = prefix;
        with_hi.push_back(HI);
        auto hi_expanded = expand_with_dont_cares(with_hi, dont_care_count - 1);
        result.insert(result.end(), hi_expanded.begin(), hi_expanded.end());

        return result;
    };

    std::vector<std::pair<BitVector, NodeValue>> result;
    std::stack<std::tuple<MtBddNodePtr, BitVector>> worklist;

    // Initialize worklist with root node and possible prefixes
    const size_t first_transition_length = get_transition_length(0, root_node->var_index);
    for (const auto& expanded_prefix : expand_with_dont_cares({}, first_transition_length)) {
        worklist.push({ root_node, expanded_prefix });
    }

    while (!worklist.empty()) {
        auto [current_node, current_prefix] = worklist.top();
        worklist.pop();

        // If terminal node, record the bit string and value
        // Stop further descending
        if (current_node->is_terminal()) {
            result.emplace_back(current_prefix, current_node->value);
            continue;
        }

        // Process LOW child
        if (current_node->low != nullptr) {
            size_t transition_length = get_transition_length(current_node->var_index, current_node->low->var_index);
            assert(transition_length > 0);
            BitVector current_base = current_prefix;
            // Append LO decision bit
            current_base.push_back(LO);
            // Process potential don't care bits
            for (const auto& expanded_prefix : expand_with_dont_cares(current_base, transition_length - 1)) {
                worklist.push({ current_node->low, expanded_prefix });
            }
        }
        // Process HIGH child
        if (current_node->high != nullptr) {
            size_t transition_length = get_transition_length(current_node->var_index, current_node->high->var_index);
            assert(transition_length > 0);
            BitVector current_base = current_prefix;
            // Append HI decision bit
            current_base.push_back(HI);
            // Process potential don't care bits
            for (const auto& expanded_prefix : expand_with_dont_cares(current_base, transition_length - 1)) {
                worklist.push({ current_node->high, expanded_prefix });
            }
        }
    }

    return result;
}

MtRobdd& MtRobdd::trim() {
    NodeSet usefull_nodes;

    // Initialize worklist with root nodes
    std::stack<MtBddNodePtr> worklist;
    for (const auto& [name, root_node] : root_nodes_map) {
        worklist.push(root_node);
        usefull_nodes.insert(root_node);
    }

    // Perform a reachability analysis from all root nodes
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

    // Update nodes.
    nodes = std::move(usefull_nodes);

    return *this;
}

MtRobdd& MtRobdd::remove_redundant_tests() {
    NodeSet new_nodes;
    NameToNodeMap new_root_nodes_map;

    // Recursive function to remove redundant tests
    std::function<MtBddNodePtr(const MtBddNodePtr)> remove_redundant_rec = [&](const MtBddNodePtr node) -> MtBddNodePtr {
        if (node == nullptr) {
            return node;
        }

        // Base case: terminal node
        if (node->is_terminal()) {
            if (!new_nodes.contains(node)) {
                new_nodes.insert(node);
            }
            return node;
        }

        MtBddNodePtr low_child = remove_redundant_rec(node->low);
        MtBddNodePtr high_child = remove_redundant_rec(node->high);

        // If both children are the same, skip this test node
        if (low_child != nullptr && low_child == high_child) {
            return low_child;
        }

        MtBddNodePtr new_node = create_node(node->var_index, low_child, high_child, node->value);
        if (!new_nodes.contains(new_node)) {
            new_nodes.insert(new_node);
        }
        return *new_nodes.find(new_node);
    };

    // Recursively process all root nodes
    for (const auto& [name, root_node] : root_nodes_map) {
        MtBddNodePtr new_root = remove_redundant_rec(root_node);
        new_root_nodes_map[name] = new_root;
    }

    // Update nodes and root nodes map
    nodes = std::move(new_nodes);
    root_nodes_map = std::move(new_root_nodes_map);

    return *this;
}

MtRobdd& MtRobdd::make_complete(const NodeValue sink_value, const bool complete_terminal_nodes) {
    MtBddNodePtr terminal_sink = std::make_shared<MtBddNode>(TERMINAL_INDEX, nullptr, nullptr, sink_value);
    bool sink_usage_flag = false;

    for (const auto& node : nodes) {
        if (complete_terminal_nodes && node->is_terminal()) {
            // Complete terminal nodes that don't have an appropriate root mapping.
            // Each such new root will point directly to the terminal sink node.
            if (!root_nodes_map.contains(node->value)) {
                root_nodes_map[node->value] = terminal_sink;
                sink_usage_flag = true;
            }
            continue;
        }

        if (node->low == nullptr) {
            node->low = terminal_sink;
            sink_usage_flag = true;
        }
        if (node->high == nullptr) {
            node->high = terminal_sink;
            sink_usage_flag = true;
        }
    }

    if (sink_usage_flag) {
        nodes.insert(terminal_sink);
        root_nodes_map[sink_value] = terminal_sink;
    }

    return *this;
}

void MtRobdd::_print_as_dot(std::ostream& os) const {
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
    // with respect to their levels
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

    // Define edges between rest of the nodes
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

} // namespace mamonata::mtrobdd

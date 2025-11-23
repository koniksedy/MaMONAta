#ifndef MAMONATA_MTROBDD_HH_
#define MAMONATA_MTROBDD_HH_

#include <memory>
#include <assert.h>
#include <cstddef>
#include <functional>
#include <limits>
#include <unordered_set>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stack>
#include <algorithm>


extern "C" {
#include "BDD/bdd.h"
#include "Mem/mem.h"
// avoid name clash with C++ keyword
#define export _export
#include "BDD/bdd_external.h"
#undef export
extern void _export(bdd_manager *bddm, unsigned p, Table *table) asm("export");
}

/**
 * MaMONAta Multi-Terminal Reduced Ordered Binary Decision Diagram (MTROBDD) implementation.
 */
namespace mamonata::mtrobdd
{

using VarIndex = int;
using MtBddNodePtr = std::shared_ptr<struct MtBddNode>;
using NodeValue = size_t;
using Bit = uint8_t;
using BitVector = std::vector<Bit>;
using NodeName = size_t;

static constexpr NodeValue MAX_NODE_VALUE = std::numeric_limits<NodeValue>::max();
static constexpr NodeValue SINK_VALUE = MAX_NODE_VALUE - 1;
static constexpr VarIndex TERMINAL_INDEX = -1;
static constexpr Bit HI = true;
static constexpr Bit LO = false;

// Implements a hash function for BitVector
struct BitVectorHash {
    size_t operator()(const BitVector& bv) const {
        size_t hash = 0;
        for (const auto& bit : bv) {
            hash = (hash << 1) ^ std::hash<Bit>()(bit);
        }
        return hash;
    }
};

/**
 * Multi-Terminal BDD Node
 *
 * Represents a node in a Multi-Terminal Binary Decision Diagram (MTBDD).
 * Each node can be an inner node (with variable index and LOW/HIGH children)
 * or a terminal node (with a specific value).
 */
struct MtBddNode {
    VarIndex var_index; // Variable index; TERMINAL_INDEX for terminal nodes.
    MtBddNodePtr low;   // Pointer to LOW child node.
    MtBddNodePtr high;  // Pointer to HIGH child node.
    NodeValue value;    // Value for terminal nodes; MAX_VALUE for inner nodes.

    /**
     * Constructor.
     *
     * @param var_index Variable index of the node.
     * @param low Pointer to LOW child node.
     * @param high Pointer to HIGH child node.
     * @param value Value for terminal nodes.
     */
    MtBddNode(VarIndex var_index = TERMINAL_INDEX, MtBddNodePtr low = nullptr, MtBddNodePtr high = nullptr, NodeValue value = MAX_NODE_VALUE)
        : var_index(var_index), low(low), high(high), value(value) {}

    /**
     * Equality operator.
     *
     * Two MtBddNodes are considered equal if they have the same variable index,
     * same LOW and HIGH child pointers, and same value.
     *
     * @param other Another MtBddNode to compare with.
     *
     * @return true if the nodes are equal, false otherwise.
     */
    bool operator==(const MtBddNode& other) const {
        return var_index == other.var_index &&
               low.get() == other.low.get() &&
               high.get() == other.high.get() &&
               value == other.value;
    }

    // Checks if the node is a root node (variable index 0).
    bool is_root() const {
        return var_index == 0;
    }

    // Checks if the node is a terminal node.
    bool is_terminal() const {
        return var_index == TERMINAL_INDEX;
    }
};

// Implements a hash function for MtBddNodePtr.
struct NodePtrHash {
    size_t operator()(const MtBddNodePtr& node) const {
        size_t h1 = std::hash<VarIndex>()(node->var_index);
        size_t h2 = std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(node->low.get()));
        size_t h3 = std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(node->high.get()));
        size_t h4 = std::hash<NodeValue>()(node->value);
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }
};

// Implements an equality function for MtBddNodePtr.
struct NodePtrEqual {
    bool operator()(const MtBddNodePtr& lhs, const MtBddNodePtr& rhs) const {
        if (lhs == rhs) return true;
        if (!lhs || !rhs) return false;
        return *lhs == *rhs;
    }
};

using NodeSet = std::unordered_set<MtBddNodePtr, NodePtrHash, NodePtrEqual>;
using NameToNodeMap = std::unordered_map<NodeName, MtBddNodePtr>;
using NameToMonaNodeMap = std::unordered_map<NodeName, bdd_ptr>;
using NodeToNameMap = std::unordered_map<MtBddNodePtr, NodeName, NodePtrHash, NodePtrEqual>;

// Multi-Terminal Reduced Ordered Binary Decision Diagram (MTROBDD).
class MtRobdd
{
    size_t num_of_vars;             // Number of variables in the MTROBDD.
    NodeSet nodes;                  // Set of unique nodes in the MTROBDD.
    NameToNodeMap root_nodes_map;   // Map from root names to root nodes.

    /**
     * Converts the MTROBDD to DOT format for visualization.
     *
     * @param os Output stream to write the DOT representation to.
     */
    void _print_as_dot(std::ostream& os) const;

public:
    MtRobdd() : num_of_vars(0), nodes(), root_nodes_map() {}

    /**
     * Constructor.
     *
     * @param num_of_vars Number of variables in the MTROBDD.
     */
    explicit MtRobdd(size_t num_of_vars) : num_of_vars(num_of_vars), nodes(), root_nodes_map() {}

    /**
     * Constructor from MONA BDD manager.
     *
     * @param num_of_vars Number of variables in the MTROBDD.
     * @param bddm Pointer to the MONA BDD manager to convert from.
     * @param root_behavior_ptrs Array of root node pointers.
     *                           Note: The caller is responsible for allocating and freeing this array.
     */
    explicit MtRobdd(size_t num_of_vars, bdd_manager* bddm, bdd_ptr *root_behavior_ptrs, size_t num_of_roots)
        : num_of_vars(0), nodes(), root_nodes_map() {
        from_mona(num_of_vars, bddm, root_behavior_ptrs, num_of_roots);
    }

    /**
     * Converts from MONA BDD manager to MTROBDD.
     *
     * @param num_of_vars Number of variables in the MTROBDD.
     * @param bddm Pointer to the MONA BDD manager to convert from.
     * @param root_behavior_ptrs Array of root node pointers.
     *                           Note: The caller is responsible for allocating and freeing this array.
     * @param num_of_roots Number of root nodes.
     *
     * @return this
     */
    MtRobdd& from_mona(size_t num_of_vars, bdd_manager* bddm, bdd_ptr *root_behavior_ptrs, size_t num_of_roots);

    /**
     * Converts the MTROBDD to MONA BDD manager.
     *
     * @param bddm Pointer to the MONA BDD manager to convert to.
     * @param root_behavior_ptrs Array to store root node pointers.
     *                           Note: The caller is responsible for allocating and freeing this array.
    */
    void to_mona(bdd_manager* bddm, bdd_ptr* root_behavior_ptrs) const;

    // Returns number of variables.
    size_t get_num_of_vars() const {
        return num_of_vars;
    }

    // Returns number of nodes.
    size_t get_num_of_nodes() const {
        return nodes.size();
    }

    // Returns number of root nodes.
    size_t get_num_of_roots() const {
        return root_nodes_map.size();
    }

    /**
     * Creates MTROBDD node. If an identical node already exists, returns the existing one.
     *
     * @param var_index Variable index of the node.
     * @param low Pointer to LOW child node.
     * @param high Pointer to HIGH child node.
     * @param value Value for terminal nodes.
     *
     * @return Pointer to the created or existing MtBddNode.
     */
    MtBddNodePtr create_node(VarIndex var_index, const MtBddNodePtr low = nullptr, const MtBddNodePtr high = nullptr, NodeValue value = MAX_NODE_VALUE) {
        MtBddNodePtr new_node = std::make_shared<MtBddNode>(var_index, low, high, value);
        auto it = nodes.find(new_node);
        if (it != nodes.end()) {
            return *it;
        }
        nodes.insert(new_node);
        return new_node;
    }

    /**
     * Creates a root node with a given name.
     *
     * @param name Name of the root node.
     *
     * @return Pointer to the created root MtBddNode.
     */
    MtBddNodePtr create_root_node(NodeName name) {
        assert(root_nodes_map.find(name) == root_nodes_map.end());
        MtBddNodePtr root_node = create_node(0);
        root_nodes_map[name] = root_node;
        return root_node;
    }

    /**
     * Creates terminal node with a given value.
     *
     * @param value Value for the terminal node.
     *
     * @return Pointer to the created terminal MtBddNode.
     */
    MtBddNodePtr create_terminal_node(NodeValue value) {
        return create_node(TERMINAL_INDEX, nullptr, nullptr, value);
    }

    /**
     * Inserts node into the MTROBDD.
     *
     * @param node Pointer to the MtBddNode to insert.
     *
     * @return true if the node was inserted, false if equivalent node already exists.
     */
    bool insert_node(const MtBddNodePtr node) {
        if (nodes.contains(node)) {
            return false;
        }
        nodes.insert(node);
        return true;
    }

    /**
     * Promotes a node to be a root node with the given name.
     *
     * @param node Pointer to the MtBddNode to promote.
     * @param name Name of the root node.
     *
     * @return true if a root with the same name already existed
     *         and was replaced, false otherwise.
     */
    bool promote_to_root(const MtBddNodePtr node, NodeName name) {
        const bool existed = root_nodes_map.contains(name);
        root_nodes_map[name] = node;
        return existed;
    }

    /**
    * Gets the root node by its name.
    *
    * @param name Name of the root node.
    *
    * @return Pointer to the MtBddNode if found, nullptr otherwise.
    */
    MtBddNodePtr get_root_node(NodeName name) const {
        auto it = root_nodes_map.find(name);
        if (it != root_nodes_map.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * Inserts a bit string into the MTROBDD starting from a given node.
     *
     * @param src_node Pointer to the starting MtBddNode, if nullptr a new root node is created.
     * @param var_index Current variable index in the bit string.
     * @param bit_string Bit string to insert.
     * @param terminal_value Value for the terminal node.
     *
     * @return Pointer to the new src_node after insertion.
     */
    MtBddNodePtr insert_bit_string(const MtBddNodePtr src_node, VarIndex var_index, const BitVector& bit_string, NodeValue terminal_value);

    /**
     * Inserts a bit string into the MTROBDD starting from a root node by its name.
     *
     * @param root_name Name of the root node to start from.
     * @param bit_string Bit string to insert.
     * @param terminal_value Value for the terminal node.
     *
     * @return Pointer to the new root node after insertion.
     */
    MtBddNodePtr insert_bit_string_from_root(NodeName root_name, const BitVector& bit_string, NodeValue terminal_value) {
        MtBddNodePtr root_node = get_root_node(root_name);
        MtBddNodePtr new_root = insert_bit_string(root_node, 0, bit_string, terminal_value);
        root_nodes_map[root_name] = new_root;
        return new_root;
    }

    /**
     * Gets all bit strings leading to terminal nodes from a given node.
     *
     * @param node Pointer to the starting MtBddNode.
     *
     * @return Vector of pairs of bit strings and their corresponding terminal values.
     */
    std::vector<std::pair<BitVector, NodeValue>> get_all_bit_strings_from_root_node(const MtBddNodePtr node) const;

    /**
     * Trims the MTROBDD by removing nodes that are not reachable from any root node.
     *
     * @return this
     */
    MtRobdd& trim();

    /**
     * Removes redundant test nodes from the MTROBDD.
     *
     * @return this
     */
    MtRobdd& remove_redundant_tests();

    /**
     * Makes the MTROBDD complete by ensuring all nodes have both LOW and HIGH children.
     * Missing children are connected to a sink terminal node with the specified value.
     *
     * @param sink_value Value for the sink terminal node.
     * @param complete_terminal_nodes If true, also ensure terminal nodes are included as roots.
     *
     * @return this
     */
    MtRobdd& make_complete(NodeValue sink_value = SINK_VALUE, bool complete_terminal_nodes = true);

    /**
     * Saves the MTROBDD as a DOT file.
     *
     * @param file_path Path to the output DOT file.
     */
    void save_as_dot(const std::filesystem::path& file_path) const {
        std::ofstream ofs(file_path);
        _print_as_dot(ofs);
        ofs.close();
    }

    /**
     * Prints the MTROBDD as DOT format to standard output.
     */
    void print_as_dot() const {
        _print_as_dot(std::cout);
    }
};

} // namespace mamonata::mtrobdd

#endif // MAMONATA_MTROBDD_HH_

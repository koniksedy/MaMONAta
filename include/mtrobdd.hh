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


namespace mamonata::mtrobdd
{
using Index = int;
using BddNodePtr = std::shared_ptr<struct BddNode>;
using Value = size_t;
using Bit = uint8_t;
using BitVector = std::vector<Bit>;
using NodeName = size_t;

static constexpr Value MAX_VALUE = std::numeric_limits<Value>::max();
static constexpr Value SINK_VALUE = MAX_VALUE - 1;
static const Index TERMINAL_INDEX = -1;
static const Bit HI = true;
static const Bit LO = false;

struct BitVectorHash {
    size_t operator()(const BitVector& bv) const {
        size_t hash = 0;
        for (const auto& bit : bv) {
            hash = (hash << 1) ^ std::hash<Bit>()(bit);
        }
        return hash;
    }
};

struct BddNode {
    Index var_index;
    BddNodePtr low;
    BddNodePtr high;
    Value value;

    BddNode(Index idx = TERMINAL_INDEX, BddNodePtr l = nullptr, BddNodePtr h = nullptr, Value v = MAX_VALUE)
        : var_index(idx), low(l), high(h), value(v) {}

    bool operator==(const BddNode& other) const {
        return var_index == other.var_index &&
               low.get() == other.low.get() &&
               high.get() == other.high.get() &&
               value == other.value;
    }

    bool is_root() const {
        return var_index == 0;
    }

    bool is_inner() const {
        return var_index > 0;
    }

    bool is_terminal() const {
        return var_index == TERMINAL_INDEX;
    }
};

struct NodePtrHash {
    size_t operator()(const BddNodePtr& node) const {
        size_t h1 = std::hash<Index>()(node->var_index);
        size_t h2 = std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(node->low.get()));
        size_t h3 = std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(node->high.get()));
        size_t h4 = std::hash<Value>()(node->value);
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }
};

struct NodePtrEqual {
    bool operator()(const BddNodePtr& lhs, const BddNodePtr& rhs) const {
        if (lhs == rhs) return true;
        if (!lhs || !rhs) return false;
        return *lhs == *rhs;
    }
};

using NodeSet = std::unordered_set<BddNodePtr, NodePtrHash, NodePtrEqual>;
using NodeMap = std::unordered_map<NodeName, BddNodePtr>;

class MtRobdd
{
    size_t num_of_vars;
    NodeSet nodes;
    NodeMap root_nodes_map;

    void to_dot(std::ostream& os) const;

public:
    MtRobdd() : num_of_vars(0), nodes(), root_nodes_map() {}
    explicit MtRobdd(size_t num_vars) : num_of_vars(num_vars), nodes(), root_nodes_map() {}

    void set_num_of_vars(size_t num_vars) {
        num_of_vars = num_vars;
    }

    size_t get_num_of_vars() const {
        return num_of_vars;
    }

    BddNodePtr create_node(Index var_index, BddNodePtr low = nullptr, BddNodePtr high = nullptr, Value value = MAX_VALUE) {
        BddNodePtr new_node = std::make_shared<BddNode>(var_index, low, high, value);
        auto it = nodes.find(new_node);
        if (it != nodes.end()) {
            return *it;
        }
        nodes.insert(new_node);
        return new_node;
    }

    BddNodePtr create_root_node(NodeName name) {
        assert(root_nodes_map.find(name) == root_nodes_map.end());
        BddNodePtr root_node = create_node(0);
        root_nodes_map[name] = root_node;
        return root_node;
    }

    BddNodePtr get_root_node(NodeName name) const {
        auto it = root_nodes_map.find(name);
        if (it != root_nodes_map.end()) {
            return it->second;
        }
        return nullptr;
    }

    BddNodePtr create_terminal_node(Value value) {
        return create_node(TERMINAL_INDEX, nullptr, nullptr, value);
    }

    BddNodePtr insert_path(BddNodePtr src, Index var_index, const BitVector& path, Value value);

    BddNodePtr insert_path_from_root(NodeName root_name, const BitVector& path, Value value) {
        BddNodePtr root_node = get_root_node(root_name);
        BddNodePtr new_root = insert_path(root_node, 0, path, value);
        root_nodes_map[root_name] = new_root;
        return new_root;
    }

    MtRobdd& trim();

    MtRobdd& remove_redundant_tests();

    MtRobdd& complete(bool complete_terminal_nodes = true);

    void save_as_dot(const std::filesystem::path& file_path) const {
        std::ofstream ofs(file_path);
        to_dot(ofs);
        ofs.close();
    }

    void print_as_dot() const {
        to_dot(std::cout);
    }
};

} // namespace mamonata::mtrobdd

#endif // MAMONATA_MTROBDD_HH_

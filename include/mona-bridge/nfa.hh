#ifndef MAMONATA_MONA_NFA_HH_
#define MAMONATA_MONA_NFA_HH_

#include <cstddef>
#include <cmath>
#include "mtrobdd.hh"
#include "mata-bridge/nfa.hh"
#include <charconv>
#include <limits>

extern "C" {
#include "BDD/bdd.h"
#include "DFA/dfa.h"
#include "Mem/mem.h"
#define export export_mona_reserved
#include "BDD/bdd_external.h"
#undef export
extern void export_mona_reserved(bdd_manager *bddm, unsigned p, Table *table) asm("export");
}


namespace mamonata::mona::nfa {

using State = mamonata::mtrobdd::Value;
using StateVector = std::vector<State>;
using Symbol = size_t;
using EncodeDict = std::unordered_map<Symbol, mamonata::mtrobdd::BitVector>;
using DecodeDict = std::unordered_map<mamonata::mtrobdd::BitVector, Symbol, mamonata::mtrobdd::BitVectorHash>;
using MataSymbolVector = std::vector<mamonata::mata::nfa::Symbol>;

class Nfa {
private:
    DFA *nfa_impl;
    size_t num_of_vars;
    size_t num_of_alphabet_vars;
    size_t num_of_nondet_vars;
    size_t nondeterminism_level;
    size_t alphabet_size;
    EncodeDict alphabet_encode_dict;
    DecodeDict alphabet_decode_dict;

    mamonata::mtrobdd::BitVector encode_symbol(const Symbol symbol) const {
        auto it = alphabet_encode_dict.find(symbol);
        if (it != alphabet_encode_dict.end()) {
            return it->second;
        }
        throw std::runtime_error("Symbol not found in encoding dictionary.");
    }

    Symbol decode_symbol(const mamonata::mtrobdd::BitVector& code) const {
        auto it = alphabet_decode_dict.find(code);
        if (it != alphabet_decode_dict.end()) {
            return it->second;
        }
        throw std::runtime_error("Code not found in decoding dictionary.");
    }

public:
    // initialize all size_t members to avoid UB from uninitialised fields
    Nfa()
        : nfa_impl(nullptr),
          num_of_vars(0),
          num_of_alphabet_vars(0),
          num_of_nondet_vars(0),
          nondeterminism_level(0),
          alphabet_size(0),
          alphabet_encode_dict(),
          alphabet_decode_dict() {}

    explicit Nfa(DFA* dfa_ptr)
        : nfa_impl(dfaCopy(dfa_ptr)),
          num_of_vars(0),
          num_of_alphabet_vars(0),
          num_of_nondet_vars(0),
          nondeterminism_level(0),
          alphabet_size(0),
          alphabet_encode_dict(),
          alphabet_decode_dict() {}

    explicit Nfa(const mata::nfa::Nfa& mata_nfa)
        : nfa_impl(nullptr),
          num_of_vars(0),
          num_of_alphabet_vars(0),
          num_of_nondet_vars(0),
          nondeterminism_level(0),
          alphabet_size(0),
          alphabet_encode_dict(),
          alphabet_decode_dict()
    {
        load(mata_nfa);
    }

    ~Nfa() {
        if (nfa_impl != nullptr) {
            dfaFree(nfa_impl);
            nfa_impl = nullptr;
        }
    }

    Nfa& load(const mamonata::mata::nfa::Nfa& input) {
        auto get_binary_code = [](size_t value, size_t bits) {
            mamonata::mtrobdd::BitVector code(bits, mamonata::mtrobdd::LO);
            for (size_t i = 0; i < bits; ++i) {
                if (value % 2 == 1) {
                    code[bits - i - 1] = mamonata::mtrobdd::HI;
                }
                value /= 2;
            }
            return code;
        };

        mamonata::mata::nfa::Nfa mata_nfa(input);
        if (mata_nfa.get_initial_states().size() > 1) {
            mata_nfa.unify_initial_states();
        }

        // Determine number of variables
        nondeterminism_level = mata_nfa.get_nondeterminism_level();
        if (nondeterminism_level > 1) {
            num_of_nondet_vars = static_cast<size_t>(std::ceil(std::log2(static_cast<double>(nondeterminism_level))));
        } else {
            // explicitly set to 0 when deterministic or single-choice nondet
            num_of_nondet_vars = 0;
        }

        MataSymbolVector alphabet = mata_nfa.get_used_symbols();
        alphabet_size = alphabet.size();
        if (alphabet_size > 1) {
            num_of_alphabet_vars = static_cast<size_t>(std::ceil(std::log2(static_cast<double>(alphabet_size))));
        } else {
            // 0 bits needed for empty or single-symbol alphabets
            num_of_alphabet_vars = 0;
        }

        num_of_vars = num_of_alphabet_vars + num_of_nondet_vars;

        // Build encoding and decoding dictionaries for alphabet symbols
        alphabet_encode_dict.clear();
        alphabet_decode_dict.clear();
        for (size_t a = 0; a < alphabet_size; ++a) {
            mamonata::mtrobdd::BitVector code = get_binary_code(a, num_of_alphabet_vars);
            alphabet_encode_dict[alphabet[a]] = code;
            alphabet_decode_dict[code] = alphabet[a];
        }

        // Build NFA transitions using MTROBDD encoding
        mamonata::mtrobdd::MtRobdd mtrobd(num_of_vars);
        for (size_t src = 0; src < mata_nfa.num_of_states(); ++src) {
            for (const Symbol symbol: alphabet) {
                StateVector targets = mata_nfa.get_successors(src, symbol);
                for (size_t i = 0; i < targets.size(); ++i) {
                    // Build full transition code with nondeterminism bits
                    mamonata::mtrobdd::BitVector transition_code(num_of_vars, mamonata::mtrobdd::LO);
                    // Set alphabet bits
                    const mamonata::mtrobdd::BitVector& symbol_code = alphabet_encode_dict[symbol];
                    for (size_t j = 0; j < num_of_alphabet_vars; ++j) {
                        transition_code[j] = symbol_code[j];
                    }
                    // Set nondeterminism bits
                    mamonata::mtrobdd::BitVector nondet_code = get_binary_code(i, num_of_nondet_vars);
                    for (size_t j = 0; j < num_of_nondet_vars; ++j) {
                        transition_code[num_of_alphabet_vars + j] = nondet_code[j];
                    }
                    // Add transition MTORBDD
                    mtrobd.insert_path_from_root(src, transition_code, targets[i]);
                }
            }
        }
        // Reduce MTROBDD to canonical form
        mtrobd.trim().remove_redundant_tests().complete(input.num_of_states());
        // mtrobd.print_as_dot();


        // Construct MONA DFA
        nfa_impl = dfaMake(static_cast<int>(mtrobd.get_num_of_roots()));
        // set initial state
        nfa_impl->s = static_cast<int>(mata_nfa.get_initial_states().front());
        // set final states
        for (State state = 0; state < mata_nfa.num_of_states(); ++state) {
            if (mata_nfa.is_final_state(state)) {
                nfa_impl->f[state] = 1;
            } else {
                nfa_impl->f[state] = 0;
            }
        }

        for (State state = mata_nfa.num_of_states(); state < mtrobd.get_num_of_roots(); ++state) {
            nfa_impl->f[state] = -1; // sink is reject
        }

        mtrobdd::NodeToNameMap node_to_position_map = mtrobd.get_node_to_position_map();
        const size_t num_of_nodes = node_to_position_map.size();
        // set behavior of each state in BDD
        for (State state = 0; state < mtrobd.get_num_of_roots(); ++state) {
            mtrobdd::BddNodePtr root_node = mtrobd.get_root_node(state);
            assert(root_node != nullptr);
            size_t node_position = node_to_position_map[root_node];
            nfa_impl->q[state] = static_cast<bdd_ptr>(node_position);
        }

        std::unordered_map<size_t, mamonata::mtrobdd::BddNodePtr> position_to_node_map;
        for (const auto& [node_ptr, node_pos]: node_to_position_map) {
            assert(position_to_node_map.contains(node_pos) == false);
            position_to_node_map[node_pos] = node_ptr;
        }

        BddNode *node_table = (BddNode *) mem_alloc(sizeof(BddNode) * num_of_nodes);

        std::function<bdd_ptr(bdd_ptr)> make_mona_node = [&](bdd_ptr i) -> bdd_ptr {
            if (node_table[i].p != -1) {
                return node_table[i].p;
            }

            if (node_table[i].idx == -1)  /* a leaf */
            {
                node_table[i].p = bdd_find_leaf_sequential(nfa_impl->bddm, node_table[i].lo);
            } else {
                assert(node_table[i].lo != node_table[i].hi);
                node_table[i].lo = make_mona_node(node_table[i].lo);
                node_table[i].hi = make_mona_node(node_table[i].hi);
                node_table[i].p = bdd_find_node_sequential(nfa_impl->bddm,
                                                           node_table[i].lo,
                                                           node_table[i].hi,
                                                           node_table[i].idx);
            }

            return node_table[i].p;
        };

        for (const auto [node_ptr, node_pos]: node_to_position_map) {
            node_table[node_pos].p = -1; // mark as uncreated
            if (node_ptr->is_terminal()) {
                node_table[node_pos].lo = static_cast<unsigned>(node_ptr->value);
                node_table[node_pos].hi = 0;
                node_table[node_pos].idx = -1;
            } else {
                assert(node_ptr->high != nullptr);
                assert(node_ptr->low != nullptr);
                node_table[node_pos].lo = static_cast<bdd_ptr>(node_to_position_map[node_ptr->low]);
                node_table[node_pos].hi = static_cast<bdd_ptr>(node_to_position_map[node_ptr->high]);
                node_table[node_pos].idx = static_cast<int>(node_ptr->var_index);
            }
        }

        // assert table
        for (size_t i = 0; i < num_of_nodes; ++i) {
            assert(node_table[i].p == -1);
            if (node_table[i].idx == -1) {
                // terminal
                assert(node_table[i].hi == 0);
                assert(node_table[i].lo == position_to_node_map[i]->value);
                assert(position_to_node_map[i]->is_terminal());
            } else {
                // inner node
                assert(node_table[i].lo != node_table[i].hi);
                assert(position_to_node_map[i]->var_index == static_cast<mtrobdd::Index>(node_table[i].idx));
                assert(node_table[i].lo == node_to_position_map[position_to_node_map[i]->low]);
                assert(node_table[i].hi == node_to_position_map[position_to_node_map[i]->high]);
                assert(!position_to_node_map[i]->is_terminal());
            }
        }


        // fill MONA bdd-manager
        for (State state = 0; state < mtrobd.get_num_of_roots(); ++state) {
            nfa_impl->q[state] = make_mona_node(nfa_impl->q[state]);
        }

        mem_free(node_table);

        print_mona();
        mtrobd.print_as_dot();

        return *this;
    }

    mamonata::mata::nfa::Nfa to_mata() const {
        // construct mata NFA states
        const size_t num_of_states = static_cast<size_t>(nfa_impl->ns);
        mamonata::mata::nfa::Nfa mata_nfa(num_of_states);
        mata_nfa.add_initial_state(static_cast<State>(nfa_impl->s));
        for (size_t state = 0; state < num_of_states; ++state) {
            if (nfa_impl->f[state] == 1) {
                mata_nfa.add_final_state(state);
            }
        }

        Table *table = tableInit();
        bdd_prepare_apply1(nfa_impl->bddm);

        // build table of tuples (idx,lo,hi)
        for (size_t i = 0; i < nfa_impl->ns; i++) {
            export_mona_reserved(nfa_impl->bddm, nfa_impl->q[i], table);
        }

        // renumber lo/hi pointers to new table ordering
        for (size_t i = 0; i < table->noelems; i++) {
            if (table->elms[i].idx != -1) {
                table->elms[i].lo = bdd_mark(nfa_impl->bddm, table->elms[i].lo) - 1;
                table->elms[i].hi = bdd_mark(nfa_impl->bddm, table->elms[i].hi) - 1;
            }
        }


        mtrobdd::MtRobdd mtrobdd_manager(num_of_vars);
        // create empty nodes
        std::unordered_map<bdd_ptr, mtrobdd::BddNodePtr> mona_to_mtrobdd_node_map;
        std::unordered_map<mtrobdd::NodeName, bdd_ptr> mona_to_mtrobdd_root_name_map;
        for (mtrobdd::NodeName state = 0; state < num_of_states; ++state) {
            bdd_ptr mona_node_ptr = bdd_mark(nfa_impl->bddm, nfa_impl->q[state]) - 1;
            mona_to_mtrobdd_root_name_map[state] = mona_node_ptr;
        }

        // create MTROBDD nodes placeholders
        for (bdd_ptr i = 0; i < table->noelems; ++i) {
            mona_to_mtrobdd_node_map[i] = std::make_shared<mtrobdd::BddNode>(table->elms[i].idx);
        }

        // fill MTROBDD nodes
        for (bdd_ptr i = 0; i < table->noelems; ++i) {
            mtrobdd::BddNodePtr mtrobdd_node = mona_to_mtrobdd_node_map[i];
            if (table->elms[i].idx == -1) {
                // terminal node
                mtrobdd_node->value = static_cast<mtrobdd::Value>(table->elms[i].lo);
            } else {
                // inner node
                mtrobdd_node->low = mona_to_mtrobdd_node_map[table->elms[i].lo];
                mtrobdd_node->high = mona_to_mtrobdd_node_map[table->elms[i].hi];
            }
            mtrobdd_manager.insert_node(mtrobdd_node);
        }

        // set MTROBDD root nodes
        for (const auto& [root_name, mona_node_ptr]: mona_to_mtrobdd_root_name_map) {
            mtrobdd::BddNodePtr mtrobdd_root_node = mona_to_mtrobdd_node_map[mona_node_ptr];
            mtrobdd_manager.promote_to_root(mtrobdd_root_node, root_name);
        }


        mtrobdd_manager.print_as_dot();

        tableFree(table);

        return mata_nfa;
    }

    void print_mona() const {

        char *orders = new char[num_of_vars];
        for (size_t i = 0; i < num_of_vars; ++i) {
            orders[i] = 0;
        }
        char **vars = new char*[num_of_vars];
        for (size_t i = 0; i < num_of_vars; ++i) {
            // safe upper bound for decimal digits of size_t
            const size_t max_digits = std::numeric_limits<size_t>::digits10 + 1;
            // 'x' + digits + terminating NUL
            const size_t buf_size = 1 + max_digits + 1;
            vars[i] = new char[buf_size];

            vars[i][0] = 'x';
            // leave one byte for terminating NUL; to_chars writes up to (end) exclusive
            auto res = std::to_chars(vars[i] + 1, vars[i] + buf_size - 1, i);
            *res.ptr = '\0';
        }

        dfaExport(nfa_impl, NULL, num_of_vars, vars, orders);

        for (size_t i = 0; i < num_of_vars; ++i) {
            delete[] vars[i];
        }
        delete[] vars;
        delete[] orders;
    }

    bool is_deterministic() const {
        return nondeterminism_level <= 1;
    }

    Nfa& minimize() {
        nfa_impl = dfaMinimize(nfa_impl);
        return *this;
    }

    Nfa& union_det_complete_with(const Nfa& aut) {
        nfa_impl = dfaProduct(nfa_impl, aut.nfa_impl, dfaOR);
        return *this;
    }

    Nfa& intersection(const Nfa& aut) {
        nfa_impl = dfaProduct(nfa_impl, aut.nfa_impl, dfaAND);
        return *this;
    }

    Nfa& complement() {
        dfaNegation(nfa_impl);
        return *this;
    }

};
}

#endif /* MAMONATA_MONA_NFA_HH_ */

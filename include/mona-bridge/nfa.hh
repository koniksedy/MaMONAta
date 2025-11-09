#ifndef MAMONATA_MONA_NFA_HH_
#define MAMONATA_MONA_NFA_HH_

#include <cstddef>
#include <cmath>
#include "mtrobdd.hh"
#include "mata-bridge/nfa.hh"

extern "C" {
#include "BDD/bdd.h"
#include "DFA/dfa.h"
#include "Mem/mem.h"
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
          alphabet_size(0) {}

    explicit Nfa(DFA* dfa_ptr)
        : nfa_impl(dfaCopy(dfa_ptr)),
          num_of_vars(0),
          num_of_alphabet_vars(0),
          num_of_nondet_vars(0),
          nondeterminism_level(0),
          alphabet_size(0) {}
    explicit Nfa(const mata::nfa::Nfa& mata_nfa) : nfa_impl(nullptr) {
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

        try {
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
            std::cerr << "Constructed MTROBDD with " << mtrobd.get_num_of_vars() << " variables." << std::endl;
            // Reduce MTROBDD to canonical form
            mtrobd.trim().remove_redundant_tests().complete();
            mtrobd.print_as_dot();
        } catch (const std::bad_alloc&) {
            throw std::runtime_error("Failed to allocate memory for MTROBDD construction.");
        }

        return *this;
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

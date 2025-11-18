#include "mona-bridge/nfa.hh"

// In order to mitigate copy/move overhead, each function
// returning a new Nfa instance uses a temporary variable
// and swaps its content with the current instance.
// This way, in c++17 and later, the initialization
// of the temporary variable does not require a copy/move
// operation (due to guaranteed copy elision).
// This is important if we want to add time measurements
// around such function calls. Additionaly, the swap
// operation is generally cheaper than move operation.

namespace mamonata::mona::nfa {

void Nfa::_print_mona(const std::string& file_path) const {

    // Prepare order and vars arrays for MONA export function
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
        // leave one byte for terminating NULL
        // to_chars writes up to (end) exclusive
        auto res = std::to_chars(vars[i] + 1, vars[i] + buf_size - 1, i);
        *res.ptr = '\0';
    }

    // Export using MONA function
    if (!file_path.empty()) {
        // Print to file
        dfaExport(nfa_impl, const_cast<char*>(file_path.c_str()), num_of_vars, vars, orders);
    } else {
        // Print to standard output
        dfaExport(nfa_impl, NULL, num_of_vars, vars, orders);
    }

    // Clean up
    for (size_t i = 0; i < num_of_vars; ++i) {
        delete[] vars[i];
    }
    delete[] vars;
    delete[] orders;
}

Nfa& Nfa::from_mata(const mamonata::mata::nfa::Nfa& input) {
    // Helper lambda to get binary code (big-endian) for a decimal value.
    auto get_binary_code = [](size_t decimal_value, size_t num_of_bits) {
        mamonata::mtrobdd::BitVector code(num_of_bits, mamonata::mtrobdd::LO);
        for (size_t i = 0; i < num_of_bits; ++i) {
            if (decimal_value % 2 == 1) {
                code[num_of_bits - i - 1] = mamonata::mtrobdd::HI;
            }
            decimal_value /= 2;
        }
        return code;
    };

    // Ensure single initial state. Don't modify input NFA.
    mamonata::mata::nfa::Nfa mata_nfa(input);
    if (mata_nfa.get_initial_states().size() > 1) {
        mata_nfa.unify_initial_states();
    }

    // Determine number of noneterminism bits.
    nondeterminism_level = mata_nfa.get_nondeterminism_level();
    if (nondeterminism_level > 1) {
        num_of_nondet_vars = static_cast<size_t>(std::ceil(std::log2(static_cast<double>(nondeterminism_level))));
    } else {
        // Set to 0 when deterministic.
        num_of_nondet_vars = 0;
    }

    // Determine alphabet size and number of alphabet bits.
    MataSymbolVector alphabet = mata_nfa.get_used_symbols();
    alphabet_size = alphabet.size();
    assert(alphabet_size > 0);
    num_of_alphabet_vars = static_cast<size_t>(std::ceil(std::log2(static_cast<double>(alphabet_size))));

    // Total number of variables.
    num_of_vars = num_of_alphabet_vars + num_of_nondet_vars;

    // Build encoding and decoding dictionaries for alphabet symbols.
    alphabet_encode_dict.clear();
    alphabet_decode_dict.clear();
    for (size_t a = 0; a < alphabet_size; ++a) {
        mamonata::mtrobdd::BitVector code = get_binary_code(a, num_of_alphabet_vars);
        alphabet_encode_dict[alphabet[a]] = code;
        alphabet_decode_dict[code] = alphabet[a];
    }

    // Build NFA transitions using MTROBDD encoding.
    mamonata::mtrobdd::MtRobdd mtrobdd_manager(num_of_vars);
    for (size_t src = 0; src < mata_nfa.num_of_states(); ++src) {
        for (const Symbol symbol: alphabet) {
            StateVector targets = mata_nfa.get_successors(src, symbol);
            for (size_t i = 0; i < targets.size(); ++i) {
                mamonata::mtrobdd::BitVector transition_code(num_of_vars, mamonata::mtrobdd::LO);
                // Set alphabet bits.
                const mamonata::mtrobdd::BitVector& symbol_code = alphabet_encode_dict[symbol];
                for (size_t j = 0; j < num_of_alphabet_vars; ++j) {
                    transition_code[j] = symbol_code[j];
                }
                // Set nondeterminism bits.
                mamonata::mtrobdd::BitVector nondet_code = get_binary_code(i, num_of_nondet_vars);
                for (size_t j = 0; j < num_of_nondet_vars; ++j) {
                    transition_code[num_of_alphabet_vars + j] = nondet_code[j];
                }
                // Add transition to MTORBDD.
                mtrobdd_manager.insert_bit_string_from_root(src, transition_code, targets[i]);
            }
        }
    }

    // Reduce MTROBDD to canonical form.
    mtrobdd_manager.trim().remove_redundant_tests().make_complete(input.num_of_states());
    // mtrobd.print_as_dot();   // DEBUG


    // Construct MONA DFA.
    nfa_impl = dfaMake(static_cast<int>(mtrobdd_manager.get_num_of_roots()));
    // Set initial state.
    nfa_impl->s = static_cast<int>(mata_nfa.get_initial_states().front());
    // Set final states.
    for (State state = 0; state < mata_nfa.num_of_states(); ++state) {
        if (mata_nfa.is_final_state(state)) {
            nfa_impl->f[state] = 1;
        } else {
            nfa_impl->f[state] = 0;
        }
    }
    // Set sink state as reject if exists.
    for (State state = mata_nfa.num_of_states(); state < mtrobdd_manager.get_num_of_roots(); ++state) {
        nfa_impl->f[state] = -1;
    }

    // Export MTROBDD to MONA representation.
    mtrobdd_manager.to_mona(nfa_impl->bddm, nfa_impl->q);

    print_mona();   // DEBUG
    mtrobdd_manager.print_as_dot();  // DEBUG

    return *this;
}

mamonata::mata::nfa::Nfa Nfa::to_mata() const {
    // Construct mata NFA states
    const size_t num_of_states = static_cast<size_t>(nfa_impl->ns);
    mamonata::mata::nfa::Nfa mata_nfa(num_of_states);

    // Set initial and final states
    mata_nfa.add_initial_state(static_cast<State>(nfa_impl->s));
    for (size_t state = 0; state < num_of_states; ++state) {
        if (nfa_impl->f[state] == 1) {
            mata_nfa.add_final_state(state);
        }
    }

    // Build MTROBDD from MONA representation
    mtrobdd::MtRobdd mtrobdd_manager(num_of_vars, nfa_impl->bddm, nfa_impl->q, num_of_states);

    mtrobdd_manager.print_as_dot(); // DEBUG

    // TODO: Extract transitions from MTROBDD + decoding

    return mata_nfa;
}

Nfa& Nfa::minimize() {
    auto tmp { dfaMinimize(nfa_impl) };
    std::swap(nfa_impl, tmp);
    return *this;
}

Nfa& Nfa::union_det_complete(const Nfa& aut) {
    auto tmp { dfaProduct(nfa_impl, aut.nfa_impl, dfaOR) };
    std::swap(nfa_impl, tmp);
    return *this;
}

Nfa& Nfa::intersection(const Nfa& aut) {
    auto tmp { dfaProduct(nfa_impl, aut.nfa_impl, dfaAND) };
    std::swap(nfa_impl, tmp);
    return *this;
}

Nfa& Nfa::complement() {
    dfaNegation(nfa_impl);
    return *this;
}

} // namespace mamonata::mona::nfa

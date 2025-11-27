#include "mata-bridge/nfa.hh"

// In order to mitigate copy/move overhead, each function
// returning a new Nfa instance uses a temporary variable
// and swaps its content with the current instance.
// This way, in c++17 and later, the initialization
// of the temporary variable does not require a copy/move
// operation (due to guaranteed copy elision).
// This is important if we want to add time measurements
// around such function calls. Additionaly, the swap
// operation is generally cheaper than move operation.

namespace mamonata::mata::nfa {

Nfa& Nfa::trim() {
    TIME(nfa_impl.trim());
    return *this;
}

Nfa& Nfa::remove_epsilon(const Symbol epsilon) {
    TIME(nfa_impl.remove_epsilon(epsilon));
    return *this;
}

Nfa& Nfa::revert() {
    TIME(auto tmp{ mata::nfa::revert(nfa_impl) });
    std::swap(nfa_impl, tmp);
    return *this;
}

Nfa& Nfa::minimize_brzozowski() {
    TIME(auto tmp{ mata::nfa::algorithms::minimize_brzozowski(nfa_impl) });
    std::swap(nfa_impl, tmp);
    return *this;
}

Nfa& Nfa::minimize_hopcroft() {
    TIME(auto tmp{ mata::nfa::algorithms::minimize_hopcroft(nfa_impl) });
    std::swap(nfa_impl, tmp);
    return *this;
}

Nfa& Nfa::reduce_simulation() {
    std::unordered_map<State,State> reduced_state_map;
    TIME(auto tmp{ mata::nfa::algorithms::reduce_simulation(nfa_impl, reduced_state_map) });
    std::swap(nfa_impl, tmp);
    return *this;
}

Nfa& Nfa::reduce_residual(const std::string& type, const std::string& direction) {
    std::unordered_map<State,State> reduced_state_map;
    TIME(auto tmp{ mata::nfa::algorithms::reduce_residual(nfa_impl, reduced_state_map, type, direction) });
    std::swap(nfa_impl, tmp);
    return *this;
}

Nfa& Nfa::concatenate(const Nfa& aut) {
    TIME(nfa_impl.concatenate(aut.nfa_impl));
    return *this;
}

Nfa& Nfa::union_nondet(const Nfa& aut) {
    TIME(nfa_impl.unite_nondet_with(aut.nfa_impl));
    return *this;
}

Nfa& Nfa::union_det_complete(const Nfa& aut) {
    TIME(auto tmp{ mata::nfa::union_det_complete(nfa_impl, aut.nfa_impl) });
    std::swap(nfa_impl, tmp);
    return *this;
}

Nfa& Nfa::determinize() {
    TIME(auto tmp{ mata::nfa::determinize(nfa_impl) });
    std::swap(nfa_impl, tmp);
    return *this;
}

Nfa& Nfa::intersection(const Nfa& aut, const Symbol first_epsilon) {
    TIME(auto tmp{ mata::nfa::intersection(nfa_impl, aut.nfa_impl, first_epsilon) });
    std::swap(nfa_impl, tmp);
    return *this;
}

Nfa& Nfa::complement_classical(const SymbolVector& symbols) {
    OrdVector<Symbol> ord_symbols{ symbols.begin(), symbols.end() };
    TIME(auto tmp{ mata::nfa::algorithms::complement_classical(nfa_impl, ord_symbols) });
    std::swap(nfa_impl, tmp);
    return *this;
}

Nfa& Nfa::complement_brzozowski(const SymbolVector& symbols) {
    OrdVector<Symbol> ord_symbols{ symbols.begin(), symbols.end() };
    TIME(auto tmp{ mata::nfa::algorithms::complement_brzozowski(nfa_impl, ord_symbols) });
    std::swap(nfa_impl, tmp);
    return *this;
}

} // namespace mamonata::mata::nfa

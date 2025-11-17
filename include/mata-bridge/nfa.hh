#ifndef MAMONATA_MATA_NFA_HH_
#define MAMONATA_MATA_NFA_HH_

#include <vector>
#include <numeric>
#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/nfa/builder.hh"


namespace mamonata::mata::nfa {
// ensure all mata::... refer to the top-level ::mata
namespace mata = ::mata;

using State = mata::nfa::State;
using StateSet = mata::nfa::StateSet;
using Symbol = mata::Symbol;
using Transition = mata::nfa::Transition;
template<typename Key>
using OrdVector = mata::utils::OrdVector<Key>;
using ParameterMap = mata::nfa::ParameterMap;

class Nfa {
private:
    mata::nfa::Nfa nfa_impl;

public:
    Nfa() : nfa_impl() {}
    explicit Nfa(size_t num_of_states) : nfa_impl(num_of_states) {}
    explicit Nfa(const mata::nfa::Nfa& other) : nfa_impl(other) {}

    void clear() {
        nfa_impl.clear();
    }

    State add_state() {
        return nfa_impl.add_state();
    }

    State add_state(State state) {
        return nfa_impl.add_state(state);
    }

    void add_initial_state(State state) {
        nfa_impl.initial.insert(state);
    }

    void add_final_state(State state) {
        nfa_impl.final.insert(state);
    }

    void add_transition(State source, Symbol symbol, State target) {
        nfa_impl.delta.add(source, symbol, target);
    }

    size_t num_of_states() const {
        return nfa_impl.num_of_states();
    }

    std::vector<State> get_initial_states() const {
        return { nfa_impl.initial.begin(), nfa_impl.initial.end() };
    }

    bool is_initial_state(State state) const {
        return nfa_impl.initial.contains(state);
    }

    std::vector<State> get_final_states() const {
        return { nfa_impl.final.begin(), nfa_impl.final.end() };
    }

    bool is_final_state(State state) const {
        return nfa_impl.final.contains(state);
    }

    std::vector<Symbol> get_states() const {
        std::vector<Symbol> states(nfa_impl.num_of_states());
        std::iota(states.begin(), states.end(), 0);
        return states;
    }

    std::vector<Symbol> get_used_symbols() const {
        return nfa_impl.delta.get_used_symbols().to_vector();
    }

    size_t num_of_transitions() const {
        return nfa_impl.delta.num_of_transitions();
    }

    std::vector<Transition> get_transitions() const {
        return { nfa_impl.delta.transitions().begin(), nfa_impl.delta.transitions().end() };
    }

    std::vector<State> get_successors(State source, Symbol symbol) const {
        StateSet successors = nfa_impl.post(source, symbol);
        return { successors.begin(), successors.end() };
    }

    size_t get_nondeterminism_level() const {
        size_t max_nondet = 0;
        for (const auto& state_post : nfa_impl.delta) {
            for (const auto& symbol_post : state_post) {
                size_t num_targets = symbol_post.targets.size();
                if (num_targets > max_nondet) {
                    max_nondet = num_targets;
                }
            }
        }
        return max_nondet;
    }

    Nfa& unify_initial_states(bool force_new_state = false) {
        nfa_impl.unify_initial(force_new_state);
        return *this;
    }

    Nfa& unify_final_states(bool force_new_state = false) {
        nfa_impl.unify_final(force_new_state);
        return *this;
    }

    Nfa& trim() {
        nfa_impl.trim();
        return *this;
    }

    Nfa& remove_epsilon(Symbol epsilon = mata::nfa::EPSILON) {
        nfa_impl.remove_epsilon(epsilon);
        return *this;
    }

    Nfa& revert() {
        nfa_impl = mata::nfa::revert(nfa_impl);
        return *this;
    }

    Nfa& minimize_brzozowski() {
        nfa_impl = mata::nfa::algorithms::minimize_brzozowski(nfa_impl);
        return *this;
    }

    Nfa& minimize_hopcroft() {
        nfa_impl = mata::nfa::algorithms::minimize_hopcroft(nfa_impl);
        return *this;
    }

    Nfa& reduce_simulation() {
        std::unordered_map<State,State> reduced_state_map;
        nfa_impl = mata::nfa::algorithms::reduce_simulation(nfa_impl, reduced_state_map);
        return *this;
    }

    Nfa& reduce_residual(const std::string& type, const std::string& direction) {
        std::unordered_map<State,State> reduced_state_map;
        nfa_impl = mata::nfa::algorithms::reduce_residual(nfa_impl, reduced_state_map, type, direction);
        return *this;
    }

    Nfa& concatenate(const Nfa& aut) {
        nfa_impl.concatenate(aut.nfa_impl);
        return *this;
    }

    Nfa& union_nondet_with(const Nfa& aut) {
        nfa_impl.unite_nondet_with(aut.nfa_impl);
        return *this;
    }

    Nfa& union_det_complete_with(const Nfa& aut) {
        nfa_impl = mata::nfa::union_det_complete(nfa_impl, aut.nfa_impl);
        return *this;
    }

    Nfa& determinize() {
        nfa_impl = mata::nfa::determinize(nfa_impl);
        return *this;
    }

    Nfa& intersection(const Nfa& aut, Symbol first_epsilon = mata::nfa::EPSILON) {
        nfa_impl = mata::nfa::intersection(nfa_impl, aut.nfa_impl, first_epsilon);
        return *this;
    }

    Nfa& complement_classical(const OrdVector<Symbol>& symbols) {
        nfa_impl = mata::nfa::algorithms::complement_classical(nfa_impl, symbols);
        return *this;
    }

    Nfa& complement_brzozowski(const OrdVector<Symbol>& symbols) {
        nfa_impl = mata::nfa::algorithms::complement_brzozowski(nfa_impl, symbols);
        return *this;
    }

    Nfa& load(const std::filesystem::path& file_path) {
        nfa_impl = mata::nfa::builder::parse_from_mata(file_path);
        return *this;
    }

    void save(const std::filesystem::path& file_path) const {
        nfa_impl.print_to_mata(file_path);
    }

    void save_as_dot(const std::filesystem::path& file_path, const bool decode_ascii_chars = false, bool use_intervals = false, int max_label_length = -1) const {
        nfa_impl.print_to_dot(file_path, decode_ascii_chars, use_intervals, max_label_length);
    }

    void print() const {
        std::cout << nfa_impl.print_to_mata();
    }

    void print_as_dot(const bool decode_ascii_chars = false, bool use_intervals = false, int max_label_length = -1) const {
        std::cout << nfa_impl.print_to_dot(decode_ascii_chars, use_intervals, max_label_length);
    }
};
}

#endif /* MAMONATA_NFA_NFA_HH_ */

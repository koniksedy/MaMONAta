#ifndef MAMONATA_MATA_NFA_HH_
#define MAMONATA_MATA_NFA_HH_

#include <fstream>
#include <vector>
#include <numeric>
#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/nfa/builder.hh"
#include "timer.hh"


// Bridge namespace exposing Mata NFA functionality.
namespace mamonata::mata::nfa {
// ensure all mata::... refer to the top-level ::mata
namespace mata = ::mata;

template<typename Key>
using OrdVector = mata::utils::OrdVector<Key>;

using State = mata::nfa::State;
using StateSet = mata::nfa::StateSet;
using StateVector = std::vector<State>;
using Symbol = mata::Symbol;
using SymbolVector = std::vector<Symbol>;
using Transition = mata::nfa::Transition;
using TransitionVector = std::vector<Transition>;

/**
 * Class exposing NFA functionality from Mata.
 */
class Nfa {
private:
    mata::nfa::Nfa nfa_impl;

public:
    Nfa() : nfa_impl() {}

    /**
     * @brief Constructs a new Nfa object with given number of states.
     *
     * @param num_of_states Number of states to preallocate in the NFA.
     */
    explicit Nfa(size_t num_of_states) : nfa_impl(num_of_states) {}
    explicit Nfa(const mata::nfa::Nfa& other) : nfa_impl(other) {}

    // Clears the NFA.
    void clear() {
        nfa_impl.clear();
    }

    /**
     * Add a new (fresh) state to the automaton.
     *
     * @return The newly created state.
     */
    State add_state() {
        return nfa_impl.add_state();
    }

    /**
     * Add state @p state to the automaton.
     *
     * @param state State to be added.
     *
     * @return The requested @p state.
     */
    State add_state(const State state) {
        return nfa_impl.add_state(state);
    }

    /**
     * @brief Adds an initial state.
     *
     * @param state State to be added as initial.
     */
    void add_initial_state(const State state) {
        nfa_impl.initial.insert(state);
    }

    /**
     * @brief Adds a final state.
     *
     * @param state State to be added as final.
     */
    void add_final_state(const State state) {
        nfa_impl.final.insert(state);
    }

    /**
     * @brief Adds a transition from source to target on symbol.
     *
     * @param source Source state.
     * @param symbol Transition symbol.
     * @param target Target state.
     */
    void add_transition(const State source, const Symbol symbol, const State target) {
        nfa_impl.delta.add(source, symbol, target);
    }

    /**
     * @brief Checks if a state is an initial state.
     *
     * @param state State to be checked.
     *
     * @return true if the state is initial, false otherwise.
     */
    bool is_initial_state(const State state) const {
        return nfa_impl.initial.contains(state);
    }

    /**
     * @brief Checks if a state is a final state.
     *
     * @param state State to be checked.
     *
     * @return true if the state is final, false otherwise.
     */
    bool is_final_state(State state) const {
        return nfa_impl.final.contains(state);
    }

    /**
     * @brief Gets the number of initial states.
     *
     * @return Number of initial states.
     */
    size_t num_of_initial_states() const {
        return nfa_impl.initial.size();
    }

    /**
     * @brief Gets the number of final states.
     *
     * @return Number of final states.
     */
    size_t num_of_final_states() const {
        return nfa_impl.final.size();
    }

    /**
     * @brief Gets the number of states in the NFA.
     *
     * @return Number of states.
     */
    size_t num_of_states() const {
        return nfa_impl.num_of_states();
    }

    /**
     * @brief Gets the initial states of the NFA.
     *
     * @return Vector of initial states.
     */
    StateVector get_initial_states() const {
        return { nfa_impl.initial.begin(), nfa_impl.initial.end() };
    }

    /**
     * @brief Gets the final states of the NFA.
     *
     * @return Vector of final states.
     */
    StateVector get_final_states() const {
        return { nfa_impl.final.begin(), nfa_impl.final.end() };
    }

    /**
     * @brief Gets all states of the NFA.
     *
     * @note It is a vector [0, ..., num_of_states - 1].
     *
     * @return Vector of all states.
     */
    StateVector get_states() const {
        StateVector states(nfa_impl.num_of_states());
        std::iota(states.begin(), states.end(), 0);
        return states;
    }

    /**
     * @brief Gets all used symbols in the NFA.
     *
     * @return Vector of all used symbols.
     */
    SymbolVector get_used_symbols() const {
        return nfa_impl.delta.get_used_symbols().to_vector();
    }

    /**
     * @brief Gets the number of transitions in the NFA.
     *
     * @return Number of transitions.
     */
    size_t num_of_transitions() const {
        return nfa_impl.delta.num_of_transitions();
    }

    /**
     * @brief Gets all transitions of the NFA.
     *
     * @return Vector of all transitions.
     */
    TransitionVector get_transitions() const {
        return { nfa_impl.delta.transitions().begin(), nfa_impl.delta.transitions().end() };
    }

    /**
     * @brief Gets all successors of a state.
     *
     * @param source Source state.
     *
     * @return Vector of successor states.
     */
    StateVector get_successors(const State source) const {
        StateSet successors = nfa_impl.delta.get_successors(source);
        return { successors.begin(), successors.end() };
    }

    /**
     * @brief Gets the successors of a state on a given symbol.
     *
     * @param source Source state.
     * @param symbol Transition symbol.
     *
     * @return Vector of successor states.
     */
    StateVector get_successors(const State source, const Symbol symbol) const {
        StateSet successors = nfa_impl.post(source, symbol);
        return { successors.begin(), successors.end() };
    }

    /**
     * @brief Gets the level of nondeterminism of the NFA.
     *
     * The level of nondeterminism is defined as the maximum number of transitions
     * on the same symbol from any state. This value is 1 for deterministic automata.
     *
     * @return Level of nondeterminism.
     */
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

    /**
     * @brief Unifies initial states into a single initial state.
     *
     * @param force_new_state If true, a new initial state is created even if there is only one initial state.
     *
     * @return Reference to this NFA.
     */
    Nfa& unify_initial_states(const bool force_new_state = false) {
        nfa_impl.unify_initial(force_new_state);
        return *this;
    }

    /**
     * @brief Unifies final states into a single final state.
     *
     * @param force_new_state If true, a new final state is created even if there is only one final state.
     *
     * @return Reference to this NFA.
     */
    Nfa& unify_final_states(const bool force_new_state = false) {
        nfa_impl.unify_final(force_new_state);
        return *this;
    }

    /**
     * @brief Loads an NFA from a Mata file.
     * @warning TODO: Use Intermediate representation to allow more automaton types.
     *
     * @param file_path Path to the Mata file.
     *
     * @return Reference to this NFA.
     */
    Nfa& load(const std::string& file_path) {
        // Open the file
        std::fstream fs(file_path, std::ios::in);
        if (!fs.is_open()) {
            throw std::runtime_error("Could not open file: " + file_path);
        }

        mata::parser::Parsed parsed;
        try {
            // Try to parse the file
            parsed = mata::parser::parse_mf(fs, true);
            fs.close();

            // Create IntermediateAut
            std::vector<mata::IntermediateAut> inter_auts = mata::IntermediateAut::parse_from_mf(parsed);
            if (inter_auts.size() != 1 || !inter_auts[0].is_nfa()) {
                throw std::runtime_error("Only single NFA automaton is supported in Mata files.");
            }

            // Build NFA from IntermediateAut
            nfa_impl = mata::nfa::builder::construct(inter_auts[0]);
        } catch (const std::exception& e) {
            fs.close();
            throw std::runtime_error("Error parsing Mata file: " + std::string(e.what()));
        }

        // nfa_impl = mata::nfa::builder::parse_from_mata(file_path);
        return *this;
    }

    /**
     * @brief Saves the NFA to a Mata file.
     *
     * @param file_path Path to the Mata file.
     */
    void save(const std::filesystem::path& file_path) const {
        nfa_impl.print_to_mata(file_path);
    }

    /**
     * @brief Saves the NFA as a DOT file.
     *
     * @param file_path Path to the DOT file.
     * @param decode_ascii_chars If true, ASCII characters in labels are decoded to their character representation.
     * @param use_intervals If true, consecutive symbols are represented as intervals in labels.
     * @param max_label_length Maximum length of labels; longer labels are truncated. Use -1 for no limit.
     */
    void save_as_dot(const std::filesystem::path& file_path, const bool decode_ascii_chars = false, bool use_intervals = false, int max_label_length = -1) const {
        nfa_impl.print_to_dot(file_path, decode_ascii_chars, use_intervals, max_label_length);
    }

    /**
     * @brief Prints the NFA in Mata format to standard output.
     */
    void print() const {
        std::cout << nfa_impl.print_to_mata();
    }

    /**
     * @brief Prints the NFA as a DOT format to standard output.
     *
     * @param decode_ascii_chars If true, ASCII characters in labels are decoded to their character representation.
     * @param use_intervals If true, consecutive symbols are represented as intervals in labels.
     * @param max_label_length Maximum length of labels; longer labels are truncated. Use -1 for no limit.
     */
    void print_as_dot(const bool decode_ascii_chars = false, bool use_intervals = false, int max_label_length = -1) const {
        std::cout << nfa_impl.print_to_dot(decode_ascii_chars, use_intervals, max_label_length);
    }

    /**
     * @brief Checks if this NFA is equivalent to another NFA.
     *
     * @param other NFA to compare with.
     *
     * @return true if the NFAs are equivalent, false otherwise.
     */
    bool are_equivalent(const Nfa& other) const {
        return mata::nfa::are_equivalent(nfa_impl, other.nfa_impl);
    }

    /**
     * @brief Trims the NFA by removing unreachable and non-coaccessible states.
     *
     * @return Reference to this NFA.
     */
    Nfa& trim();

    /**
     * @brief Removes epsilon transitions from the NFA.
     *
     * @param epsilon Symbol representing epsilon.
     *
     * @return Reference to this NFA.
     */
    Nfa& remove_epsilon(Symbol epsilon = mata::nfa::EPSILON);

    /**
     * @brief Reverts the NFA by swapping initial and final states and reversing all transitions.
     *
     * @return Reference to this NFA.
     */
    Nfa& revert();

    /**
     * @brief Minimizes the NFA using Brzozowski's algorithm.
     *
     * @return Reference to this NFA.
     */
    Nfa& minimize_brzozowski();

    /**
     * @brief Minimizes the NFA using Hopcroft's algorithm.
     *
     * @return Reference to this NFA.
     */
    Nfa& minimize_hopcroft();

    /**
     * @brief Reduces the NFA using simulation reduction.
     *
     * @return Reference to this NFA.
     */
    Nfa& reduce_simulation();

    /**
     * @brief Reduces the NFA using residual reduction.
     *
     * @param type Type of residual reduction ("after" or "with").
     * @param direction Direction of residual reduction ("inward" or "outward").
     */
    Nfa& reduce_residual(const std::string& type, const std::string& direction);

    /**
     * @brief Concatenates this NFA with another NFA.
     *
     * @param aut NFA to concatenate with.
     *
     * @return Reference to this NFA.
     */
    Nfa& concatenate(const Nfa& aut);

    /**
     * @brief Unions this NFA with another NFA in a nondeterministic manner.
     * Does not add epsilon transitions, just unites initial and final states.
     *
     * @param aut NFA to union with.
     */
    Nfa& union_nondet(const Nfa& aut);

    /**
     * @brief Compute union of two complete deterministic automata. Perserves determinism.
     * The union is computed by product construction with OR condition on the final states.
     *
     * @param aut Complete deterministic automaton to union with.
     *
     * @return Reference to this NFA.
     */
    Nfa& union_det_complete(const Nfa& aut);

    /**
     * @brief Determinizes the NFA using the subset construction algorithm.
     *
     * @return Reference to this NFA.
     */
    Nfa& determinize();

    /**
     * @brief Computes the intersection of this NFA with another NFA.
     *
     * @param aut NFA to intersect with.
     * @param first_epsilon The first epsilon symbol to be used in the product construction.
     *
     * @return Reference to this NFA.
     */
    Nfa& intersection(const Nfa& aut, Symbol first_epsilon = mata::nfa::EPSILON);

    /**
     * @brief Complements the NFA by determinizing it, adding a sink state, and making the automaton complete.
     * Non-final states in the original automaton are converted to final states in the complemented automaton.
     */
    Nfa& complement_classical(const SymbolVector& symbols);

    /**
     * @brief Complements the NFA by determinizing it using Brzozowski's algorithm, adding a sink state,
     * and making the automaton complete. Then it swaps final and non-final states.
     *
     * @return Reference to this NFA.
     */
    Nfa& complement_brzozowski(const SymbolVector& symbols);
};

}

#endif /* MAMONATA_NFA_NFA_HH_ */

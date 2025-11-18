#ifndef MAMONATA_MONA_NFA_HH_
#define MAMONATA_MONA_NFA_HH_

#include <cstddef>
#include <cmath>
#include "mata-bridge/nfa.hh"
#include <charconv>
#include <limits>
#include "mtrobdd.hh"

// Avoid macro collisions with TRUE/FALSE from MONA headers
#pragma push_macro("TRUE")
#pragma push_macro("FALSE")
#undef TRUE
#undef FALSE
extern "C" {
#include "DFA/dfa.h"
}
#pragma pop_macro("FALSE")
#pragma pop_macro("TRUE")

/**
 * Bridge namespace exposing MONA NFA functionality.
 * @warning MONA allows only deterministic complete automata with binary-encoded symbols.
 */
namespace mamonata::mona::nfa {

using State = mamonata::mtrobdd::NodeValue;
using StateVector = std::vector<State>;
using Symbol = size_t;
using MataSymbolVector = std::vector<mamonata::mata::nfa::Symbol>;
using EncodeDict = std::unordered_map<Symbol, mamonata::mtrobdd::BitVector>;
using DecodeDict = std::unordered_map<mamonata::mtrobdd::BitVector, Symbol, mamonata::mtrobdd::BitVectorHash>;

/**
 * Class exposing NFA functionality from MONA.
 *
 * MONA represents automata as DFAs using shared MTROBDDs for transitions.
 * In order to encode nonbinary alphabets we use binary encoding of symbols
 * with the minimum number of bits required to represent all symbols.
 * Additionally, to represent nondeterministic automata, we add extra bits at
 * the end of the sequence of bits representing the symbol. These extra bits
 * allow to distinguish between different nondeterministic choices for the same symbol.
 * Obviously, we cannot operate on such pseudo-nondeterministic automata in MONA directly.
 * We allways convert them to deterministic automata first by projecting out the nondeterminism bits.
 *
 * Example of mona format:
 * TODO: add example
 *
 * @warning MONA allows only deterministic complete automata with binary-encoded symbols.
 *          Read the class documentation for details.
 */
class Nfa {
private:
    DFA *nfa_impl;                      // Internal MONA DFA representation
    size_t num_of_vars;                 // Total number of variables (alphabet + nondet)
    size_t num_of_alphabet_vars;        // Number of variables for alphabet encoding
    size_t num_of_nondet_vars;          // Number of variables for nondeterminism encoding
    size_t nondeterminism_level;        // Level of nondeterminism (1 for deterministic automata)
    size_t alphabet_size;               // Size of the alphabet
    EncodeDict alphabet_encode_dict;    // Mapping from symbols to binary codes
    DecodeDict alphabet_decode_dict;    // Mapping from binary codes to symbols

    /**
     * @brief Encodes a symbol into its binary representation.
     *
     * @param symbol Symbol to be encoded.
     *
     * @return BitVector Binary representation of the symbol.
     */
    mamonata::mtrobdd::BitVector encode_symbol(const Symbol symbol) const {
        assert(alphabet_encode_dict.contains(symbol));
        return alphabet_encode_dict.at(symbol);
    }

    /**
     * @brief Decodes a binary representation into its symbol.
     *
     * @param code BitVector to be decoded.
     *
     * @return Symbol Decoded symbol.
     */
    Symbol decode_symbol(const mamonata::mtrobdd::BitVector& code) const {
        assert(alphabet_decode_dict.contains(code));
        return alphabet_decode_dict.at(code);
    }

    /**
     * @brief Prints the MONA NFA to the given output stream.
     *
     * @param file_path Output file path. If empty, prints to standard output.
     *
     */
    void _print_mona(const std::string &file_path = "") const;

public:
    Nfa()
        : nfa_impl(nullptr),
          num_of_vars(0),
          num_of_alphabet_vars(0),
          num_of_nondet_vars(0),
          nondeterminism_level(0),
          alphabet_size(0),
          alphabet_encode_dict(),
          alphabet_decode_dict() {}

    /**
     * @brief Constructs a new Nfa object from a Mata NFA.
     *
     * @param mata_nfa Mata NFA to be converted.
     *
     * @return Nfa Newly constructed Nfa object.
     */
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
        from_mata(mata_nfa);
    }

    ~Nfa() {
        if (nfa_impl != nullptr) {
            dfaFree(nfa_impl);
            nfa_impl = nullptr;
        }
    }

    Nfa& load(const std::string& file_path);

    /**
     * @brief Initializes the MONA NFA by converting from a Mata NFA.
     *
     * @param input Mata NFA to be converted.
     *
     * @return this
     */
    Nfa& from_mata(const mamonata::mata::nfa::Nfa& input);

    /**
     * @brief Converts the MONA NFA to a Mata NFA.
     *
     * @return Mata NFA equivalent to this MONA NFA.
     */
    mamonata::mata::nfa::Nfa to_mata() const;

    /**
     * @brief Saves the MONA NFA to a file
     *
     * @param file_path Output file path.
     */
    void save(const std::string& file_path) const {
        _print_mona(file_path);
    }

    /**
     * @brief Saves the MTROBDD representation of the NFA as a DOT file.
     *
     * @param file_path Output file path.
     */
    void save_as_dot(const std::filesystem::path& file_path) const {
        mamonata::mtrobdd::MtRobdd mtrobdd_manager(num_of_vars, nfa_impl->bddm, nfa_impl->q, static_cast<size_t>(nfa_impl->ns));
        mtrobdd_manager.save_as_dot(file_path);
    }

    /**
     * @brief Prints the MONA NFA to standard output.
     */
    void print_mona() const {
        _print_mona();
    }

    /**
     * @brief Prints the MTROBDD representation of the NFA in DOT format to standard output.
     */
    void print_as_dot() const {
        mamonata::mtrobdd::MtRobdd mtrobdd_manager(num_of_vars, nfa_impl->bddm, nfa_impl->q, static_cast<size_t>(nfa_impl->ns));
        mtrobdd_manager.print_as_dot();
    }

    /**
     * @brief Checks if the NFA is deterministic.
     *
     * Of course, in MONA all automata are represented as DFAs.
     * However, this method checks whether the automaton uses any
     * nondeterminism bits in its encoding.
     *
     * @return true if the NFA is deterministic, false otherwise.
     */
    bool is_deterministic() const {
        return nondeterminism_level <= 1;
    }

    /**
     * @brief Minimizes the automaton using MONA's DFA minimization.
     *
     * @return Reference to this.
     */
    Nfa& minimize();

    /**
     * @brief Computes the union of this automaton with another automaton.
     * Uses MONA's DFA product construction with OR operation.
     *
     * @param aut Automaton to union with.
     *
     * @return Reference to this.
     */
    Nfa& union_det_complete(const Nfa& aut);

    /**
     * @brief Computes the intersection of this automaton with another automaton.
     * Uses MONA's DFA product construction with AND operation.
     *
     * @param aut Automaton to intersect with.
     *
     * @return Reference to this.
     */
    Nfa& intersection(const Nfa& aut);

    /**
     * @brief Computes the complement of this automaton.
     *
     * @return Reference to this.
     */
    Nfa& complement();
};

}

#endif /* MAMONATA_MONA_NFA_HH_ */

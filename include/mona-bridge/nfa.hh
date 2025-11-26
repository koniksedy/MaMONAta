#ifndef MAMONATA_MONA_NFA_HH_
#define MAMONATA_MONA_NFA_HH_

#include <cstddef>
#include <cmath>
#include "mata-bridge/nfa.hh"
#include <charconv>
#include <limits>
#include <optional>
#include "mtrobdd.hh"
#include "timer.hh"

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
        return alphabet_decode_dict.at(code);
    }

    /**
     * @brief Prints the MONA NFA to the given output stream.
     *
     * @param file_path Output file path. If empty, prints to standard output.
     *
     */
    void _print(const std::string &file_path = "") const;

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
     * @param alphabet_order Optional order of symbols in the alphabet.
     *
     * @return Nfa Newly constructed Nfa object.
     */
    explicit Nfa(const mata::nfa::Nfa& mata_nfa, const std::optional<MataSymbolVector>& alphabet_order = std::nullopt)
        : nfa_impl(nullptr),
          num_of_vars(0),
          num_of_alphabet_vars(0),
          num_of_nondet_vars(0),
          nondeterminism_level(0),
          alphabet_size(0),
          alphabet_encode_dict(),
          alphabet_decode_dict()
    {
        from_mata(mata_nfa, alphabet_order);
    }

    Nfa(const Nfa& other)
        : nfa_impl(dfaCopy(other.nfa_impl)),
          num_of_vars(other.num_of_vars),
          num_of_alphabet_vars(other.num_of_alphabet_vars),
          num_of_nondet_vars(other.num_of_nondet_vars),
          nondeterminism_level(other.nondeterminism_level),
          alphabet_size(other.alphabet_size),
          alphabet_encode_dict(other.alphabet_encode_dict),
          alphabet_decode_dict(other.alphabet_decode_dict) {}

    Nfa& operator=(const Nfa& other) {
        if (this != &other) {
            if (nfa_impl != nullptr) {
                dfaFree(nfa_impl);
            }
            nfa_impl = dfaCopy(other.nfa_impl);
            num_of_vars = other.num_of_vars;
            num_of_alphabet_vars = other.num_of_alphabet_vars;
            num_of_nondet_vars = other.num_of_nondet_vars;
            nondeterminism_level = other.nondeterminism_level;
            alphabet_size = other.alphabet_size;
            alphabet_encode_dict = other.alphabet_encode_dict;
            alphabet_decode_dict = other.alphabet_decode_dict;
        }
        return *this;
    }

    Nfa& operator=(Nfa&& other) noexcept {
        if (this != &other) {
            if (nfa_impl != nullptr) {
                dfaFree(nfa_impl);
            }
            nfa_impl = other.nfa_impl;
            num_of_vars = other.num_of_vars;
            num_of_alphabet_vars = other.num_of_alphabet_vars;
            num_of_nondet_vars = other.num_of_nondet_vars;
            nondeterminism_level = other.nondeterminism_level;
            alphabet_size = other.alphabet_size;
            alphabet_encode_dict = std::move(other.alphabet_encode_dict);
            alphabet_decode_dict = std::move(other.alphabet_decode_dict);

            other.nfa_impl = nullptr;
        }
        return *this;
    }

    // Move constructor
    // Ensures proper resource management
    Nfa(Nfa&& other) noexcept : nfa_impl(other.nfa_impl)
    {
        other.nfa_impl = nullptr;
    }

    ~Nfa() {
        if (nfa_impl != nullptr) {
            dfaFree(nfa_impl);
            nfa_impl = nullptr;
        }
    }

    /**
     * @brief Generates the alphabet symbols and their encodings.
     *
     * @param size Size of the alphabet to be generated.
     */
    void generate_alphabet(const size_t size);

    /**
     * @brief Updates the alphabet decoding to include codes from another NFA.
     * Encodings that already exist are preserved (no updated).
     *
     * @warning User must ensure that there are no conflicting encodings between the two NFAs.
     *
     * @param other NFA whose alphabet symbols are to be included.
     */
    void update_alphabet(const Nfa& other);

    /**
     * @brief Loads a MONA NFA from a file.
     *
     * @param file_path Input file path.
     *
     * @return this
     */
    Nfa& load(const std::string& file_path);

    /**
     * @brief Initializes the MONA NFA by converting from a Mata NFA.
     *
     * @param input Mata NFA to be converted.
     * @param alphabet_order Optional order of symbols in the alphabet.
     *
     * @return this
     */
    Nfa& from_mata(const mamonata::mata::nfa::Nfa& input, const std::optional<MataSymbolVector>& alphabet_order = std::nullopt);

    /**
     * @brief Converts the MONA NFA to a Mata NFA. With predefined encoding/decoding dictionaries.
     *
     * @warning All transitions with symbols that are not present in the decoding dictionary will be ignored.
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
        _print(file_path);
    }

    /**
     * @brief Saves the MONA representation of the NFA as a DOT file.
     *
     * @param file_path Output file path.
     */
    void save_as_dot(const std::filesystem::path& file_path) const {
        FILE* orig_stdout = stdout;
        freopen(file_path.string().c_str(), "w", stdout);
        print_as_dot();
        fflush(stdout);
        freopen("/dev/tty", "w", stdout);
        stdout = orig_stdout;
    }

    /**
     * @brief Saves the MTROBDD representation of the NFA as a DOT file.
     *
     * @param file_path Output file path.
     */
    void save_mtrobdd_as_dot(const std::filesystem::path& file_path) const {
        mamonata::mtrobdd::MtRobdd mtrobdd_manager(num_of_vars, nfa_impl->bddm, nfa_impl->q, static_cast<size_t>(nfa_impl->ns));
        mtrobdd_manager.save_as_dot(file_path);
    }

    /**
     * @brief Prints the MONA NFA to standard output.
     */
    void print() const {
        _print();
    }

    /**
     * @brief Prints the MONA representation of the NFA in DOT format to standard output.
     */
    void print_as_dot() const {
        unsigned *indices = new unsigned[num_of_vars];
        for (unsigned i = 0; i < num_of_vars; ++i) {
            indices[i] = i;
        }
        dfaPrintGraphviz(nfa_impl, num_of_vars, indices);
        delete[] indices;
    }

    /**
     * @brief Prints the MTROBDD representation of the NFA in DOT format to standard output.
     */
    void print_mtrobdd_as_dot() const {
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
     * @brief Determinizes the NFA by projecting out nondeterminism bits.
     *
     * @return Reference to this.
     */
    Nfa& determinize();

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
     * @warning User must ensore that both automata have the same alphabet encoding.
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
     * @warning User must ensore that both automata have the same alphabet encoding.
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

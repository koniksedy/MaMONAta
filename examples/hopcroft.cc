/**
 * @file minimize.cc
 * @brief Example comparing Hopcroft minimization of NFA in Mata and MONA.
 * This example loads an NFA in Mata format.
 */
#include "mata-bridge/nfa.hh"
#include "mona-bridge/nfa.hh"
#include "timer.hh"

#ifndef TIMING_ENABLED
#error "This example requires timing to be enabled. Please enable TIMING_ENABLED in CMake configuration."
#endif

using MataNfa = mamonata::mata::nfa::Nfa;
using MonaNfa = mamonata::mona::nfa::Nfa;


int main(int argc, char *argv[]) {
    MataNfa mata_nfa;
    mata_nfa.load(argv[1]);
    MonaNfa mona_nfa(mata_nfa);
    Timer::microseconds det_time = 0;
    if (!mona_nfa.is_deterministic()) {
        mona_nfa.determinize();
    }

    std::cerr << "Mata;Mona-Det;Mona" << std::endl;
    mata_nfa.minimize_hopcroft();
    std::cerr << Timer::get("minimize_hopcroft") << ";";
    std::cerr << det_time << ";";
    mona_nfa.minimize();
    std::cerr << Timer::get("minimize") << std::endl;

    mona_nfa.print();

#ifdef DEBUG
    MataNfa converted_mata_nfa = mona_nfa.to_mata().trim();
    MataNfa minimized_mata_nfa = mata_nfa.trim();
    assert(converted_mata_nfa.are_equivalent(minimized_mata_nfa));
    assert(converted_mata_nfa.num_of_states() == minimized_mata_nfa.num_of_states());
    assert(converted_mata_nfa.num_of_states() == mona_nfa.num_of_states());
#endif

    return 0;
}

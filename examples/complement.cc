/**
 * @file complement.cc
 * @brief Example comparing complementation of NFA in Mata and MONA.
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


    std::cerr << "Mata;Mona-Det;Mona" << std::endl;
    mata_nfa.complement_classical(mata_nfa.get_used_symbols());
    std::cerr << Timer::get("complement_classical") << ";";
    Timer::microseconds det_time = 0;
    if (!mona_nfa.is_deterministic()) {
        mona_nfa.determinize();
        det_time = Timer::get("determinize");
    }
    std::cerr << det_time << ";";
    mona_nfa.complement();
    std::cerr << Timer::get("complement") << std::endl;

    mona_nfa.print();

    assert(mona_nfa.to_mata().are_equivalent(mata_nfa));

    return 0;
}

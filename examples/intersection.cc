/**
 * @file intersection.cc
 * @brief Example comparing intersection of NFA in Mata and MONA.
 * This example loads NFAs in Mata format.
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
    MataNfa mata_a;
    mata_a.load(argv[1]);
    MonaNfa mona_a(mata_a);
    Timer::microseconds det_time = 0;
    if (!mona_a.is_deterministic()) {
        mona_a.determinize();
        det_time += Timer::get("determinize");
    }

    MataNfa mata_b;
    mata_b.load(argv[2]);
    MonaNfa mona_b(mata_b);
    if (!mona_b.is_deterministic()) {
        mona_b.determinize();
        det_time += Timer::get("determinize");
    }

    std::cerr << "Mata;Mona-Det;Mona" << std::endl;
    MataNfa mata_nfa = mata_a.intersection(mata_b);
    std::cerr << Timer::get("intersection") << ";";
    std::cerr << det_time << ";";
    MonaNfa mona_nfa = mona_a.intersection(mona_b);
    std::cerr << Timer::get("intersection") << std::endl;

    mona_nfa.print();

    assert(mona_nfa.to_mata().are_equivalent(mata_nfa));

    return 0;
}

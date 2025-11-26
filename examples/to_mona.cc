/**
 * @file to_mona_dot.cc
 * @brief Example program that converts a Mata NFA to MONA DOT format.
 */
#include "mata-bridge/nfa.hh"
#include "mona-bridge/nfa.hh"

using MataNfa = mamonata::mata::nfa::Nfa;
using MonaNfa = mamonata::mona::nfa::Nfa;


int main(int argc, char *argv[]) {
    MataNfa mata;
    mata.load(argv[1]);
    MonaNfa mona(mata);
    mona.print();

    return 0;
}

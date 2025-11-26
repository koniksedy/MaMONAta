#include "mata-bridge/nfa.hh"
#include "mona-bridge/nfa.hh"
#include "mtrobdd.hh"
#include "timer.hh"

using namespace mamonata::mtrobdd;
using MataNfa = mamonata::mata::nfa::Nfa;
using MonaNfa = mamonata::mona::nfa::Nfa;


int main(int argc, char *argv[]) {

    MataNfa mata_a;
    mata_a.load(argv[1]);
    MonaNfa mona_a(mata_a);

    MataNfa mata_b;
    mata_b.load(argv[2]);
    MonaNfa mona_b(mata_b);

    MataNfa mata_nfa = mata_a.intersection(mata_b);
    // mata_nfa.print_as_dot();
    MonaNfa mona_nfa = mona_a.intersection(mona_b);

    MataNfa converted_mata_nfa = mona_nfa.to_mata();
    converted_mata_nfa.trim().print_as_dot();

    if (converted_mata_nfa.are_equivalent(mata_nfa)) {
        std::cout << "PASS" << std::endl;
    } else {
        std::cout << "FAIL" << std::endl;
    }

    // MataNfa mata_nfa;
    // mata_nfa.load(argv[1]);
    // MonaNfa mona_nfa(mata_nfa);

    // mata_nfa.determinize();
    // mata_nfa.print_as_dot();

    // mona_nfa.determinize();
    // mona_nfa.print_as_dot();

    // MataNfa converted_mata_nfa = mona_nfa.to_mata();
    // converted_mata_nfa.trim().print_as_dot();

    // if (converted_mata_nfa.are_equivalent(mata_nfa)) {
    //     std::cout << "PASS" << std::endl;
    // } else {
    //     std::cout << "FAIL" << std::endl;
    // }

    // return 0;
}

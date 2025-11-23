#include "mata-bridge/nfa.hh"
#include "mona-bridge/nfa.hh"
#include "mtrobdd.hh"

using namespace mamonata::mtrobdd;
using MataNfa = mamonata::mata::nfa::Nfa;
using MonaNfa = mamonata::mona::nfa::Nfa;


int main(int argc, char *argv[]) {

    MataNfa mata_nfa;
    mata_nfa.load(std::filesystem::path("../atm1.mata"));
    mata_nfa.print_as_dot();
    MonaNfa mona_nfa(mata_nfa);
    mona_nfa.print();
    MataNfa converted_mata_nfa = mona_nfa.to_mata();
    converted_mata_nfa.trim().print_as_dot();

    // MtRobdd mt_roBDD(3);
    // mt_roBDD.insert_path_from_root(1, {LO, LO, LO}, 2);
    // mt_roBDD.insert_path_from_root(1, {LO, LO, HI}, 2);
    // mt_roBDD.insert_path_from_root(1, {HI, LO, LO}, 2);
    // mt_roBDD.insert_path_from_root(1, {HI, LO, HI}, 2);
    // mt_roBDD.insert_path_from_root(1, {HI, HI, LO}, 2);
    // mt_roBDD.insert_path_from_root(1, {HI, HI, HI}, 2);
    // mt_roBDD.insert_path_from_root(2, {HI, LO, LO}, 2);
    // mt_roBDD.insert_path_from_root(2, {HI, LO, HI}, 3);
    // mt_roBDD.trim();
    // mt_roBDD.remove_redundant_tests();
    // mt_roBDD.complete();

    // mt_roBDD.print_as_dot();

    return 0;
}

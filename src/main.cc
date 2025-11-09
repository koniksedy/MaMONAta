#include "mata/nfa/nfa.hh"
#include "mtrobdd.hh"

using namespace mata::nfa;
using namespace mamonata::mtrobdd;

extern "C" {
#include "BDD/bdd.h"
#include "DFA/dfa.h"
#include "Mem/mem.h"
}

int main(int argc, char *argv[]) {

    MtRobdd mt_roBDD(3);
    mt_roBDD.insert_path_from_root(1, {LO, LO, LO}, 2);
    mt_roBDD.insert_path_from_root(1, {LO, LO, HI}, 2);
    mt_roBDD.insert_path_from_root(1, {HI, LO, LO}, 2);
    mt_roBDD.insert_path_from_root(1, {HI, LO, HI}, 2);
    mt_roBDD.insert_path_from_root(1, {HI, HI, LO}, 2);
    mt_roBDD.insert_path_from_root(1, {HI, HI, HI}, 2);
    mt_roBDD.insert_path_from_root(2, {HI, LO, LO}, 2);
    mt_roBDD.insert_path_from_root(2, {HI, LO, HI}, 3);
    mt_roBDD.trim();
    mt_roBDD.remove_redundant_tests();
    mt_roBDD.complete();

    mt_roBDD.print_as_dot();

    return 0;
}

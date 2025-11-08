#include "mata/nfa/nfa.hh"

using namespace mata::nfa;

extern "C" {
#include "BDD/bdd.h"
#include "DFA/dfa.h"
#include "Mem/mem.h"
}

int main(int argc, char *argv[]) {
    // Load and print automaton

    Nfa nfa;

    char **names;
    int *orders;
    DFA *dfa = dfaImport(argv[1], &names, &orders);
    dfaPrintVerbose(dfa);
    dfaExport(dfa, (char *)"exported.mona", 3, names, (char *)orders);
    return 0;
}

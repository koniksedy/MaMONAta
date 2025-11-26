# MaMONAta
MaMONAta is a library that provides an adapter for Mata and MONA implementations of automata.
It allows users to easily convert between different automata representations and transparently
compare the performance of the two tools.

## Features
- Parsing of automata in `.mata` and `.mona` formats.
- All possible conversions between Mata and MONA automata.
- Transparent performance comparison between Mata and MONA implementations.
- `Timer` class for measuring the execution time of functions.
- `MtRobdd` class implementing Multi-terminal Reduced Ordered Binary Decision Diagrams (MtROBDDs).
- DOT visualization of Mata, MONA, and MtROBDD structures.

## Automata Operations
Operation       | Mata | MONA
--------------- | ---- | ---
Union           |  X   | X
Intersection    |  X   | X
Complement      |  X   | X
Minimization    |  X   | X
Determinization |  X   | X

## Nonbinary Alphabet Support
MaMONAta adds MONA support for automata with nonbinary alphabets by encoding symbols as binary strings
using log₂(n) bits, where n is the size of the alphabet.

## Nondeterministic Automata Support
Even though MONA only supports deterministic automata, MaMONAta allows users to simulate
nondeterminism in MONA by introducing additional variables to represent multiple transitions.
For N nondeterministic choices, log₂(N) additional variables are added.

**NOTE:** This feature only allows MONA to represent nondeterministic automata.
Before performing any MONA operations, the automaton must be determinized.

## MONA Format
```
MONA DFA
number of variables: 2
variables: P Q
orders: 2 2
states: 3
initial: 0
bdd nodes: 4
final: -1 1 -1
behaviour: 0 1 3
bdd:
-1 1 0
0 0 2
1 3 0
-1 2 0
end
```
- The name of each variable is used internally to identify alphabet symbol bits (prefix `A`)
  and nondeterministic choice bits (prefix `N`). All variables are of the zero-`order` type.
- `states` indicates the number of states in the automaton.
- `initial` indicates the initial state.
- `final` is a list of final states, where `-1` indicates a non-final state and `1` indicates a final state.
- `behaviour` is a list indexed by state, where each entry indicates the BDD node representing the starting point
  of the transition function for that state.
- `bdd` is a list of BDD nodes, where each node is represented by three integers:
  - The first integer is the variable index (0-based) used for branching at this node.
  - The second integer is the index of the low child node (taken when the variable is 0).
  - The third integer is the index of the high child node (taken when the variable is 1).
  - Terminal nodes are represented by `-1` followed by the value stored in the low child position, with the high child position being `0`.
  - The BDD must be reduced.


## Building MaMONAta
Download the repositories by running the script `extern/download.sh`.
Then, build the project using `make release` or `make debug` for a debug build.
In `CMakeLists.txt`, you can choose whether to time the operations by setting the `TIMING_ENABLED` option to `ON` or `OFF`.
All operations are timed by default.

## Examples
See the `examples/` directory for sample usage of MaMONAta features.

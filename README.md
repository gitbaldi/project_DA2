# Compiler Register Allocation Algorithms

## Project Overview
This project implements a compiler register allocation tool designed to operate as part of a compiler back-end. It processes the intermediate representation of program variables, specifically their live execution ranges, and maps them to physical registers for enhanced execution performance.

The tool achieves this by translating variable live ranges into structural "webs", constructing an interference graph, and applying graph coloring heuristics to find an optimal assignment within a constrained number of physical registers.


## Features and Heuristics
When the number of webs exceeds the available physical registers (causing register pressure), the application applies specific fallback optimization heuristics to find an acceptable solution:

* **Web Generation**: Fuses intersecting ranges of the same variable across different lines into a single structural unit called a "Web".
* **Interference Graph Construction**: Builds a bidirectional graph using a custom `Graph` data structure where each node represents a Web, and edges denote overlapping lifetimes (interference).
* **Basic Coloring**: Minimizes the number of registers used by mapping each graph node to a distinct physical register index.
* **Web Spilling**: Run for up to $K$ iterations. Identifies the web with the highest interference degree, marks it to be spilled into memory (assigned to a placeholder register state `-2`), and removes it from the graph to alleviate pressure for other variables.
* **Web Splitting**: Run for up to $K$ iterations. Finds the web with the highest degree spanning multiple code lines and splits its set of active lines in half. This creates two distinct sub-webs, significantly reducing interference and making the graph easier to color.


## File Structure
The project repository contains the following core components:

* **`main.cpp`**: The primary entry point that instantiates the `Solver`, triggers argument validation, executes register allocation, and manages terminal lifecycle logging.
* **`cli_parser.hpp` / `cli_parser.cpp`**: Handles robust command-line argument validation and filters out extraneous formatting/whitespaces from input configuration files.
* **`solver.hpp` / `solver.cpp`**: Holds the main core algorithmic logic, including web lifecycle building, interference evaluation, graph coloring, spilling routines, and splitting sub-routines.
* **`Graph.h`**: A generic, highly adaptive template graph data structure utilized to represent and manipulate the interference graph.
* **`MutablePriorityQueue.h`**: An optimized priority queue implementation helpful for efficiently extracting elements with extreme weight or node degrees.


## Input Specifications

The executable processes tasks in batch mode (`-b`) and requires two text configuration files:

### 1. Ranges File (e.g., `ranges1.txt`)
Defines the name of each variable along with the specific execution code lines where it is alive.
* **Format**: `variable_name: line1, line2, line3...`
* **Sufixes**:
    * A `+` suffix signifies a variable's definition point (write).
    * A `-` suffix signifies the variable's dead line (last read).

*Example content:*
```text
sum: 7+,8,9,10-
i: 1+,2,3,4,5,6-
i: 9+,10,11,12-
```

### 2. Registers File (e.g., `registers1.txt`)
Configures constraints regarding target hardware parameters and selection profiles.

* **Format**: Contains lines specifying the total register pool size and the designated strategy layout (`basic`, `spilling`, or `splitting`).

*Example content:*
```text
registers: 3
algorithm: spilling 2
```
## Compilation and Execution
### Compilation
Compile all source files using a standard C++ compiler (such as g++):

Bash
```text
g++ -std=c++17 main.cpp src/cli_parser.cpp src/solver.cpp -o myProg
```

### Execution Command
The program runs strictly in batch mode using the -b flag and accepts three parameters:

```text
./myProg -b data/ranges/<ranges_file> data/registers/<registers_file> <output_file>
```
Example Usage
```text
./myProg -b ranges1.txt registers1.txt allocation.txt
```

### Expected Output
Upon successful allocation execution, the terminal will report:
`processing ended successfully!` <br>
The register mappings and optimization outcomes will then be written directly into the specified output file (e.g., allocation.txt).
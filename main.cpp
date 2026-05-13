#include "include/cli_parser.hpp"
#include "include/solver.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
    Solver solver;
    if (parseArguments(argc, argv, solver) != 0) return 1;
    solver.allocateRegisters();

    std::cout << "Processamento concluído com sucesso!" << std::endl;

    return 0;
}
#include "../include/solver.hpp"
#include <iostream>

// constructor
Solver::Solver() {
    kParam = 0;
    numRegisters = 0;
}

// cli_parser functions
void Solver::addLiveRangeStart(std::string varName, int line) {
    std::cout << "-> Start lido: variável '" << varName << "' na linha " << line << std::endl;
}

void Solver::addLiveRangeEnd(std::string varName, int line) {
    std::cout << "-> End lido: variável '" << varName << "' na linha " << line << std::endl;
}

void Solver::addLiveRangePoint(std::string varName, int line) {
    std::cout << "-> Ponto lido: variável '" << varName << "' na linha " << line << std::endl;
}

void Solver::setNumRegisters(int n) {
    numRegisters = n;
    std::cout << "-> Número de registos (N) lido: " << n << std::endl;
}

void Solver::setAlgorithm(std::string alg, int param) {
    algorithm = alg;
    kParam = param;
    std::cout << "-> Algoritmo lido: " << alg << " com parâmetro " << param << std::endl;
}

void Solver::updateOutputFile(std::string path) {
    outputFile = path;
}

void Solver::generateOutput() {
    std::cout << "generating output file: " << outputFile << std::endl;
}

// main functions
void Solver::buildInterferenceGraph() {
    // graph with webs
}

void Solver::applySpilling(int k) {
    std::cout << "executing spilling with K = " << k << std::endl;
    // web spilling
}

void Solver::applySplitting(int k) {
    std::cout << "executing splitting with K = " << k << std::endl;
    // web splitting
}

bool Solver::tryColoring() {
    // graph coloring
    return true;
}

void Solver::allocateRegisters() {
    buildInterferenceGraph();

    if (algorithm == "spilling") {
        applySpilling(kParam);
    } else if (algorithm == "splitting") {
        applySplitting(kParam);
    }

    bool success = tryColoring();

    if (!success) {
        std::cout << "couldn't color the graph" << std::endl;
    }

    generateOutput();
}
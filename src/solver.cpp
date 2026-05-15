#include "../include/solver.hpp"
#include <iostream>

// constructor
Solver::Solver() {
    kParam = 0;
    numRegisters = 0;
}

// cli_parser functions
void Solver::addLiveRangeStart(std::string varName, int line) {
    Web newWeb;
    newWeb.id = nextWebId++;
    newWeb.varName = varName;
    newWeb.lines.insert(line);
    allWebs.push_back(newWeb);
}

void Solver::addLiveRangePoint(std::string varName, int line) {
    if (!allWebs.empty()) allWebs.back().lines.insert(line);
}

void Solver::addLiveRangeEnd(std::string varName, int line) {
    if (!allWebs.empty()) allWebs.back().lines.insert(line);
}

void Solver::setNumRegisters(int n) {
    numRegisters = n;
    std::cout << "-> number of registers read (N): " << n << std::endl;
}

void Solver::setAlgorithm(std::string alg, int param) {
    algorithm = alg;
    kParam = param;
    std::cout << "-> algorithm read: " << alg << " with parameter " << param << std::endl;
}

void Solver::updateOutputFile(std::string path) {
    outputFile = path;
}

void Solver::generateOutput() {
    std::cout << "generating output file: " << outputFile << std::endl;
}

// main functions
void Solver::buildInterferenceGraph() {
    std::cout << "building graph" << std::endl;
    std::cout << "total webs read: " << allWebs.size() << std::endl;

    for (const auto& w : allWebs) {
        std::cout << "web id " << w.id << " (" << w.varName
                  << ") active in lines: ";
        for (int l : w.lines) std::cout << l << " ";
        std::cout << std::endl;
    }

    // create edges in the graph if two webs share the same line
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
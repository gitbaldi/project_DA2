#ifndef SOLVER_H
#define SOLVER_H

#pragma once
#include <string>
#include <vector>
#include <set>

struct Web {
    int id;
    std::string varName;
    std::set<int> lines;
    int reg = -1; // -1: none, -2: spill (mem), 0...N: registers
};

class Solver {
private:
    std::string algorithm;
    int kParam;
    int numRegisters;
    std::string outputFile;
    std::vector<Web> allWebs;
    int nextWebId = 0;

public:
    Solver(); // constructor

    // cli_parser functions
    void addLiveRangeStart(std::string varName, int line);
    void addLiveRangeEnd(std::string varName, int line);
    void addLiveRangePoint(std::string varName, int line);

    void setNumRegisters(int n);
    void setAlgorithm(std::string alg, int param);

    void updateOutputFile(std::string path);
    void allocateRegisters();
    void generateOutput();

    // algorithm functions
    void buildInterferenceGraph();
    void applySpilling(int k);
    void applySplitting(int k);
    bool tryColoring();
};

#endif
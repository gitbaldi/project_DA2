#ifndef SOLVER_H
#define SOLVER_H

#pragma once
#include "../data_structures/Graph.h"
#include <string>
#include <vector>
#include <set>
#include <unordered_map>


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
    std::unordered_map<std::string, int> varToWebId;
    Graph<int> interferenceGraph;



public:
    Solver(); // constructor

    // cli_parser functions
    void addLiveRangeStart(std::string varName, int line);
    void addLiveRangeEnd(std::string varName, int line);
    void addLiveRangePoint(std::string varName, int line);

private:
    int getOrCreateWebId(const std::string &varName);

public:


    void setNumRegisters(int n);
    void setAlgorithm(std::string alg, int param);

    void updateOutputFile(std::string path);
    void allocateRegisters();
    void generateOutput();

    // register assignment result bookkeeping
    void assignRegistersOrSpill();


    // algorithm functions
    void buildInterferenceGraph();
    void applySpilling(int k);
    void applySplitting(int k);
    bool tryColoring();
};

#endif
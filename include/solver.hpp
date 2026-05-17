#ifndef SOLVER_H
#define SOLVER_H

#pragma once
#include "../data_structures/Graph.h"
#include <string>
#include <vector>
#include <set>
#include <unordered_map>


/**
 * @struct Web
 * @brief Represents a Live Range (Web) of a variable in the program.
 * * Contains all necessary information about a variable's life cycle, including
 * its unique identifier, name, the specific lines of code where it is active,
 * and its assigned register or memory state.
 */
struct Web {
    int id;              /**< Unique identifier for the web. */
    std::string varName; /**< Original name of the variable. */

    // Track live-range endpoints separately so we can correctly model cases where:
    //   one web ends at line L and another starts at line L -> NO interference.
    std::set<int> startLines;   /**< Lines where the web becomes live (+L). */
    std::set<int> pointLines;   /**< Lines where the web is live in the middle (plain number). */
    std::set<int> endLines;     /**< Lines where the web stops being live (-L). */

    int reg = -1;        /**< Register assignment status (-1: unassigned, -2: spilled to memory, 0...N: physical register index). */
};

class Solver {
private:
    std::string algorithm;
    int kParam;
    int numRegisters;
    std::string outputFile;
    std::vector<Web> allWebs;
    std::unordered_map<std::string, int> varToWebId;
    int nextWebId = 0;
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
  
    /**
     * @brief Builds the Interference Graph from scratch based on active live ranges.
     * * This method resets the current interference graph, populates it with vertices
     * representing all active webs, and performs a pairwise comparison between them.
     * If two webs share at least one line of code, a bidirectional interference edge is
     * created, indicating they cannot be allocated to the same physical register.
     * * @note Time Complexity: O(W^2 * L) where W is the number of webs and L is the average number of lines per web.
     */
     void buildInterferenceGraph();

    /**
     * @brief Applies the Web Spilling heuristic to alleviate register pressure.
     * * This heuristic executes up to K iterations. In each iteration, it identifies the
     * "most problematic" web (the vertex with the highest degree/most interference edges).
     * The chosen web is marked as spilled (reg = -2) and its vertex is removed from the graph,
     * effectively freeing up register space for the remaining webs.
     * * @param k Maximum number of spilling iterations allowed.
     * @see buildInterferenceGraph
     */
    void applySpilling(int k);

    /**
     * @brief Applies the Web Splitting heuristic to break down high-degree live ranges.
     * * This heuristic runs for up to K iterations. It searches for the web with the highest
     * interference degree that spans at least 2 code lines. It divides its set of active lines
     * in half, updates the original web, and generates a new independent sub-web for the second half.
     * The interference graph is automatically reconstructed after each split to update degrees.
     * * @param k Maximum number of splitting operations allowed.
     * @see buildInterferenceGraph
     */
    void applySplitting(int k);
    bool tryColoring();
};

#endif
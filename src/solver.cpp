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

/**
 * @details Clears any existing graph data from previous runs to avoid pollution,
 * inserts a vertex for every existing web, and uses a nested loop combination to
 * detect overlapping line sets between distinct webs.
 */
void Solver::buildInterferenceGraph() {
    std::cout << "building interference graph" << std::endl;
    interferenceGraph = Graph<int>();

    // add webs
    for (const auto& w : allWebs) {
        interferenceGraph.addVertex(w.id);
    }

    // add edges
    for (size_t i = 0; i < allWebs.size(); ++i) {
        for (size_t j = i + 1; j < allWebs.size(); ++j) {
            bool interfere = false;

            // verify if webs share a line
            for (int lineA : allWebs[i].lines) {
                if (allWebs[j].lines.count(lineA)) {
                    interfere = true;
                    break;
                }
            }

            // create edge
            if (interfere) {
                std::cout << "conflict detected: web " << allWebs[i].id
                          << " <-> web " << allWebs[j].id << std::endl;
                interferenceGraph.addBidirectionalEdge(allWebs[i].id, allWebs[j].id, 0);
            }
        }
    }
}

/**
 * @details Iterates a maximum of K times. In each step, it filters out already spilled
 * webs, finds the one currently causing the most interference in the graph, and marks it
 * for memory storage. By removing it from the graph, the degrees of all adjacent nodes drop.
 */
void Solver::applySpilling(int k) {
    std::cout << "executing web spilling (max k = " << k << ")" << std::endl;

    // repeat process k times or until graph is empty
    for (int i = 0; i < k; ++i) {
        int maxDegree = -1;
        int webIdToSpill = -1;

        // fing web with most edges
        for (auto &web : allWebs) {
            if (web.reg == -2) continue; // ignore if web is spilling was already applied

            auto v = interferenceGraph.findVertex(web.id);
            if (v != nullptr) {
                int degree = v->getAdj().size();
                if (degree > maxDegree) {
                    maxDegree = degree;
                    webIdToSpill = web.id;
                }
            }
        }

        // spill the web found
        if (webIdToSpill != -1 && maxDegree > 0) {
            std::cout << "[Spill] web " << webIdToSpill << " chosen (grau " << maxDegree << ")" << std::endl;

            // mark in list
            for(auto &w : allWebs) {
                if(w.id == webIdToSpill) w.reg = -2;
            }

            // remove web from graph
            interferenceGraph.removeVertex(webIdToSpill);
        } else {
            break;
        }
    }
}

/**
 * @details Implements a passive splitting strategy. It targets the web with the maximum
 * number of conflicts, ensuring it has a splitable size (>= 2 lines). The live range is
 * bisected, a new Web object is pushed to the collection with an appended '_split' suffix,
 * and a full graph reconstruction is triggered to adjust to the new topology.
 */
void Solver::applySplitting(int k) {
    std::cout << "executing web splitting (max k = " << k << ")" << std::endl;

    for (int step = 0; step < k; ++step) {
        int maxDegree = -1;
        int webIdToSplit = -1;

        // find web with most conflicts
        for (auto &web : allWebs) {
            if (web.reg == -2 || web.lines.size() < 2) continue;

            auto v = interferenceGraph.findVertex(web.id);
            if (v != nullptr) {
                int degree = v->getAdj().size();
                if (degree > maxDegree) {
                    maxDegree = degree;
                    webIdToSplit = web.id;
                }
            }
        }

        if (webIdToSplit != -1 && maxDegree > 0) {
            std::cout << "[Split] web " << webIdToSplit << " chosen (grau " << maxDegree << ")" << std::endl;

            // find web index on the list
            int index = -1;
            for (size_t i = 0; i < allWebs.size(); ++i) {
                if (allWebs[i].id == webIdToSplit) {
                    index = i; break;
                }
            }

            if (index != -1) {
                Web newWeb;
                newWeb.id = nextWebId++;
                newWeb.varName = allWebs[index].varName + "_split";

                // cut set<int> in half
                std::set<int> firstHalf, secondHalf;
                int count = 0;
                int half = allWebs[index].lines.size() / 2;

                for (int line : allWebs[index].lines) {
                    if (count < half) firstHalf.insert(line);
                    else secondHalf.insert(line);
                    count++;
                }

                // update new web and save
                allWebs[index].lines = firstHalf;
                newWeb.lines = secondHalf;

                allWebs.push_back(newWeb);

                std::cout << "  -> cut in: web " << allWebs[index].id << " and web " << newWeb.id << std::endl;

                // rebuild graph
                buildInterferenceGraph();
            }
        } else {
            break;
        }
    }
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
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
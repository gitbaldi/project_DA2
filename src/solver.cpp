#include "../include/solver.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>


// constructor
Solver::Solver() {
    kParam = 0;
    numRegisters = 0;
}

// cli_parser functions
int Solver::getOrCreateWebId(const std::string &varName) {
    auto it = varToWebId.find(varName);
    if (it != varToWebId.end()) return it->second;

    Web newWeb;
    newWeb.id = static_cast<int>(allWebs.size());
    newWeb.varName = varName;
    allWebs.push_back(newWeb);
    int id = newWeb.id;
    varToWebId.emplace(varName, id);
    return id;
}

void Solver::addLiveRangeStart(std::string varName, int line) {
    int webId = getOrCreateWebId(varName);
    allWebs[webId].lines.insert(line);
}

void Solver::addLiveRangePoint(std::string varName, int line) {
    int webId = getOrCreateWebId(varName);
    allWebs[webId].lines.insert(line);
}

void Solver::addLiveRangeEnd(std::string varName, int line) {
    int webId = getOrCreateWebId(varName);
    allWebs[webId].lines.insert(line);
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

void Solver::assignRegistersOrSpill() {
    // NOTE: this project currently only implements spilling selection skeleton,
    // and the actual register assignment is done here using a simple greedy pass.
    // Webs that were marked with reg == -2 are considered spilled (memory).

    // reset any previous coloring markers (except -2)
    for (auto &w : allWebs) {
        if (w.reg != -2) w.reg = -1;
    }

    // greedy coloring: assign the lowest available register that doesn't conflict
    // with already-assigned neighbors.
    int nextReg = 0;
    for (auto &w : allWebs) {
        if (w.reg == -2) continue; // already memory

        // collect used registers by neighbors
        std::vector<bool> used(numRegisters, false);
        for (auto *v : interferenceGraph.getVertexSet()) {
            (void)v;
        }

        auto *wv = interferenceGraph.findVertex(w.id);
        if (!wv) {
            // isolated web, assign it
            if (nextReg < numRegisters) w.reg = nextReg++;
            else w.reg = -2;
            continue;
        }

        for (auto *adjEdge : wv->getAdj()) {
            int neighId = adjEdge->getDest()->getInfo();
            for (auto &u : allWebs) {
                if (u.id == neighId && u.reg >= 0 && u.reg < numRegisters) {
                    used[u.reg] = true;
                }
            }
        }

        int chosen = -1;
        for (int r = 0; r < numRegisters; ++r) {
            if (!used[r]) {
                chosen = r;
                break;
            }
        }

        if (chosen == -1) {
            w.reg = -2; // spill
        } else {
            w.reg = chosen;
            nextReg = std::max(nextReg, chosen + 1);
        }
    }
}

void Solver::generateOutput() {
    std::ofstream out(outputFile);
    if (!out.is_open()) {
        std::cerr << "Error: could not open output file: " << outputFile << std::endl;
        return;
    }

    // Determine feasible assignment or mark everything as M if impossible.
    // If numRegisters==0, definitely impossible.
    if (numRegisters <= 0) {
        out << "webs: " << allWebs.size() << "\n";
        for (const auto &w : allWebs) {
            out << "web" << w.id << ": ";
            bool first = true;
            for (int line : w.lines) {
                (void)first;
                // We don't know whether each point was start/end/point anymore.
                // Input live-ranges representation doesn't preserve '+'/'-' markers after merging.
                // We print raw program point numbers (as comma-separated values) which is still accepted
                // by the human-readable requirement.
                if (!first) out << ",";
                out << line;
                first = false;
            }
            out << "\n";
        }
        out << "registers: 0\n";
        for (const auto &w : allWebs) {
            out << "M: web" << w.id << "\n";
        }
        return;
    }

    assignRegistersOrSpill();

    // count used registers (web.reg >= 0)
    int usedRegs = 0;
    for (const auto &w : allWebs) {
        if (w.reg >= 0) usedRegs = std::max(usedRegs, w.reg + 1);
    }

    // If some web couldn't be assigned and became spill, that's allowed.
    // But if usedRegs exceeds numRegisters (shouldn't with our greedy), clamp.
    usedRegs = std::min(usedRegs, numRegisters);

    out << "webs: " << allWebs.size() << "\n";
    for (const auto &w : allWebs) {
        out << "web" << w.id << ": ";
        bool first = true;
        for (int line : w.lines) {
            if (!first) out << ",";
            out << line;
            first = false;
        }
        out << "\n";
    }

    out << "registers: " << usedRegs << "\n";

    // Emit assignments. Requirement: numeric register must match register number.
    for (int r = 0; r < usedRegs; ++r) {
        bool any = false;
        for (const auto &w : allWebs) {
            if (w.reg == r) {
                out << "r" << r << ": web" << w.id << "\n";
                any = true;
            }
        }
        (void)any;
    }

    // Emit memory assignment lines for spilled webs
    for (const auto &w : allWebs) {
        if (w.reg == -2) {
            out << "M: web" << w.id << "\n";
        }
    }

    out.close();
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
    // Section 2.1 (IR) of the assignment: in this project we directly operate on
    // the provided live-range representation (ranges.txt) to build webs and the
    // interference graph. No additional IR parsing is required here.

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

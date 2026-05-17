#include "../include/solver.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <set>


// constructor
Solver::Solver() {
    kParam = 0;
    numRegisters = 0;
}

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
    allWebs[webId].startLines.insert(line);
}

void Solver::addLiveRangePoint(std::string varName, int line) {
    int webId = getOrCreateWebId(varName);
    allWebs[webId].pointLines.insert(line);
}

void Solver::addLiveRangeEnd(std::string varName, int line) {
    int webId = getOrCreateWebId(varName);
    allWebs[webId].endLines.insert(line);
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

static bool webContains(const Web &w, int line) {
    return w.startLines.count(line) || w.pointLines.count(line) || w.endLines.count(line);
}

static bool webInterfere(const Web &a, const Web &b) {
    // Required behavior to avoid false interference:
    // a conflict only exists if they share some line that is in the lifetime
    // of both webs. In the original +/-(start/end) encoding, if one web ends
    // at line L and the other starts at line L, that should NOT be an
    // interference.
    //
    // With our split representation, we model:
    //   active-at-L for start/end/point:
    //     - startLines: active (present)
    //     - pointLines: active
    //     - endLines: treat as "no longer active at L" (so not overlapping with other's start at same L)
    //
    // Therefore, an overlap exists if there is a line L such that:
    //   L is active for a AND active for b.

    // Active sets for overlap:
    //   aActive = startLines U pointLines
    //   bActive = startLines U pointLines

    for (int line : a.startLines) {
        if (b.startLines.count(line) || b.pointLines.count(line)) return true;
    }
    for (int line : a.pointLines) {
        if (b.startLines.count(line) || b.pointLines.count(line)) return true;
    }
    return false;
}

void Solver::buildInterferenceGraph() {
    std::cout << "building interference graph" << std::endl;
    interferenceGraph = Graph<int>();

    for (const auto &w : allWebs) {
        interferenceGraph.addVertex(w.id);
    }

    for (size_t i = 0; i < allWebs.size(); ++i) {
        for (size_t j = i + 1; j < allWebs.size(); ++j) {
            if (webInterfere(allWebs[i], allWebs[j])) {
                std::cout << "conflict detected: web " << allWebs[i].id << " <-> web " << allWebs[j].id
                          << std::endl;
                interferenceGraph.addBidirectionalEdge(allWebs[i].id, allWebs[j].id, 0);
            }
        }
    }
}

void Solver::applySpilling(int k) {
    std::cout << "executing web spilling (max k = " << k << ")" << std::endl;

    for (int iter = 0; iter < k; ++iter) {
        int maxDegree = -1;
        int webIdToSpill = -1;

        for (auto &web : allWebs) {
            if (web.reg == -2) continue;
            auto *v = interferenceGraph.findVertex(web.id);
            if (!v) continue;

            int degree = static_cast<int>(v->getAdj().size());
            if (degree > maxDegree) {
                maxDegree = degree;
                webIdToSpill = web.id;
            }
        }

        if (webIdToSpill == -1) break;

        // mark + remove from graph
        for (auto &w : allWebs) {
            if (w.id == webIdToSpill) w.reg = -2;
        }
        interferenceGraph.removeVertex(webIdToSpill);
    }
}

void Solver::applySplitting(int k) {
    std::cout << "executing web splitting (max k = " << k << ")" << std::endl;

    for (int iter = 0; iter < k; ++iter) {
        int maxDegree = -1;
        int webIdToSplit = -1;

        for (auto &web : allWebs) {
            if (web.reg == -2) continue;

            const size_t totalPoints = web.startLines.size() + web.pointLines.size() + web.endLines.size();
            if (totalPoints < 2) continue;

            auto *v = interferenceGraph.findVertex(web.id);
            if (!v) continue;

            int degree = static_cast<int>(v->getAdj().size());
            if (degree > maxDegree) {
                maxDegree = degree;
                webIdToSplit = web.id;
            }
        }

        if (webIdToSplit == -1) break;

        int index = -1;
        for (size_t i = 0; i < allWebs.size(); ++i) {
            if (allWebs[i].id == webIdToSplit) {
                index = static_cast<int>(i);
                break;
            }
        }
        if (index == -1) break;

        // Build timeline across start+point+end (sorted by line).
        std::vector<int> timeline;
        timeline.reserve(allWebs[index].startLines.size() + allWebs[index].pointLines.size() +
                          allWebs[index].endLines.size());
        for (int x : allWebs[index].startLines) timeline.push_back(x);
        for (int x : allWebs[index].pointLines) timeline.push_back(x);
        for (int x : allWebs[index].endLines) timeline.push_back(x);
        std::sort(timeline.begin(), timeline.end());

        int half = static_cast<int>(timeline.size()) / 2;

        std::set<int> firstHalf(timeline.begin(), timeline.begin() + half);
        std::set<int> secondHalf(timeline.begin() + half, timeline.end());

        Web newWeb;
        newWeb.id = nextWebId++;
        newWeb.varName = allWebs[index].varName + "_split";

        // reset and rebuild endpoint sets from halves
        allWebs[index].startLines.clear();
        allWebs[index].pointLines.clear();
        allWebs[index].endLines.clear();
        newWeb.startLines.clear();
        newWeb.pointLines.clear();
        newWeb.endLines.clear();

        for (int x : firstHalf) {
            if (webContains(allWebs[index], x) && allWebs[index].startLines.count(x)) {
                // unreachable because we cleared; handle by checking original membership before clearing
            }
        }

        // To preserve original membership, compute from the old web snapshot.
        // (Reconstruct snapshot before clearing.)
        // --- take snapshot ---
        Web old = allWebs[index];

        // clear again (safety)
        allWebs[index].startLines.clear();
        allWebs[index].pointLines.clear();
        allWebs[index].endLines.clear();

        for (int x : firstHalf) {
            if (old.startLines.count(x)) allWebs[index].startLines.insert(x);
            else if (old.pointLines.count(x)) allWebs[index].pointLines.insert(x);
            else if (old.endLines.count(x)) allWebs[index].endLines.insert(x);
        }
        for (int x : secondHalf) {
            if (old.startLines.count(x)) newWeb.startLines.insert(x);
            else if (old.pointLines.count(x)) newWeb.pointLines.insert(x);
            else if (old.endLines.count(x)) newWeb.endLines.insert(x);
        }

        allWebs.push_back(newWeb);

        // rebuild graph after topology change
        buildInterferenceGraph();
    }
}

bool Solver::tryColoring() {
    // Reset all non-spilled webs.
    for (auto &w : allWebs) {
        if (w.reg != -2) w.reg = -1;
    }

    if (numRegisters <= 0) {
        for (auto &w : allWebs) w.reg = -2;
        return false;
    }

    // Order: descending degree.
    std::vector<int> order;
    order.reserve(allWebs.size());
    for (auto &w : allWebs) {
        if (w.reg == -2) continue;
        order.push_back(w.id);
    }

    auto degreeOf = [&](int webId) {
        auto *v = interferenceGraph.findVertex(webId);
        return v ? static_cast<int>(v->getAdj().size()) : 0;
    };

    std::sort(order.begin(), order.end(), [&](int a, int b) { return degreeOf(a) > degreeOf(b); });

    auto findWebById = [&](int webId) -> Web * {
        for (auto &w : allWebs) {
            if (w.id == webId) return &w;
        }
        return nullptr;
    };

    for (int webId : order) {
        Web *w = findWebById(webId);
        if (!w || w->reg == -2) continue;

        std::vector<bool> used(numRegisters, false);
        auto *v = interferenceGraph.findVertex(webId);
        if (v) {
            for (auto *adjEdge : v->getAdj()) {
                int neighId = adjEdge->getDest()->getInfo();
                Web *neigh = findWebById(neighId);
                if (neigh && neigh->reg >= 0 && neigh->reg < numRegisters) {
                    used[neigh->reg] = true;
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
            // Spec requirement: if impossible, all webs go to memory.
            for (auto &ww : allWebs) ww.reg = -2;
            return false;
        }

        w->reg = chosen;
    }

    // Any remaining unassigned => memory.
    for (auto &w : allWebs) {
        if (w.reg != -2 && w.reg < 0) w.reg = -2;
    }

    return true;
}

void Solver::generateOutput() {
    std::ofstream out(outputFile);
    if (!out.is_open()) {
        std::cerr << "Error: could not open output file: " << outputFile << std::endl;
        return;
    }

    // If numRegisters==0 => everything to memory.
    if (numRegisters <= 0) {
        out << "webs: " << allWebs.size() << "\n";
        for (const auto &w : allWebs) {
            out << "web" << w.id << ": ";
            bool first = true;
            for (int x : w.startLines) {
                if (!first) out << ",";
                out << x << "+";
                first = false;
            }
            for (int x : w.pointLines) {
                if (!first) out << ",";
                out << x;
                first = false;
            }
            for (int x : w.endLines) {
                if (!first) out << ",";
                out << x << "-";
                first = false;
            }
            out << "\n";
        }
        out << "registers: 0\n";
        for (const auto &w : allWebs) out << "M: web" << w.id << "\n";
        return;
    }

    int usedRegs = 0;
    for (const auto &w : allWebs) {
        if (w.reg >= 0) usedRegs = std::max(usedRegs, w.reg + 1);
    }
    usedRegs = std::min(usedRegs, numRegisters);

    out << "webs: " << allWebs.size() << "\n";
        for (const auto &w : allWebs) {
            out << "web" << w.id << ": ";
            bool first = true;
            for (int x : w.startLines) {
                if (!first) out << ",";
                out << x << "+";
                first = false;
            }
            for (int x : w.pointLines) {
                if (!first) out << ",";
                out << x;
                first = false;
            }
            for (int x : w.endLines) {
                if (!first) out << ",";
                out << x << "-";
                first = false;
            }
        out << "\n";
    }

    out << "registers: " << usedRegs << "\n";

    for (int r = 0; r < usedRegs; ++r) {
        for (const auto &w : allWebs) {
            if (w.reg == r) {
                out << "r" << r << ": web" << w.id << "\n";
            }
        }
    }

    for (const auto &w : allWebs) {
        if (w.reg == -2) out << "M: web" << w.id << "\n";
    }
}

void Solver::allocateRegisters() {
    buildInterferenceGraph();

    // Algorithm selection:
    // - basic: only tryColoring
    // - spilling: applySpilling then tryColoring
    // - splitting: applySplitting then tryColoring
    if (algorithm == "spilling") {
        applySpilling(kParam);
    } else if (algorithm == "splitting") {
        applySplitting(kParam);
    }

    bool success = tryColoring();
    if (!success) {
        std::cout << "couldn't color the graph (falling back to memory)" << std::endl;
    }

    generateOutput();
}


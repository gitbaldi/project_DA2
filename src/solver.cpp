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

/**
 * @brief Retrieves the web ID for a variable or creates a new web if it does not exist.
 *
 * @param varName Name of the variable.
 * @return int Unique web ID associated with the variable.
 */
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

/**
 * @brief Adds a live range start line to a variable's web.
 *
 * @param varName Name of the variable.
 * @param line Line where the live range starts.
 */
void Solver::addLiveRangeStart(std::string varName, int line) {
    int webId = getOrCreateWebId(varName);
    allWebs[webId].startLines.insert(line);
}

/**
 * @brief Adds a live range point line to a variable's web.
 *
 * @param varName Name of the variable.
 * @param line Line where the live range is alive.
 */
void Solver::addLiveRangePoint(std::string varName, int line) {
    int webId = getOrCreateWebId(varName);
    allWebs[webId].pointLines.insert(line);
}

/**
 * @brief Adds a live range end line to a variable's web.
 *
 * @param varName Name of the variable.
 * @param line Line where the live range ends.
 */
void Solver::addLiveRangeEnd(std::string varName, int line) {
    int webId = getOrCreateWebId(varName);
    allWebs[webId].endLines.insert(line);
}

/**
 * @brief Sets the number of available registers for allocation.
 *
 * @param n Number of registers.
 */
void Solver::setNumRegisters(int n) {
    numRegisters = n;
    std::cout << "-> number of registers read (N): " << n << std::endl;
}

/**
 * @brief Configures the register allocation algorithm and its parameter.
 *
 * @param alg Name of the algorithm to use.
 * @param param Optional integer parameter for the algorithm.
 */
void Solver::setAlgorithm(std::string alg, int param) {
    algorithm = alg;
    kParam = param;
    std::cout << "-> algorithm read: " << alg << " with parameter " << param << std::endl;
}

/**
 * @brief Updates the output file path.
 *
 * @param path Path to the output file.
 */
    void Solver::updateOutputFile(std::string path) {
        outputFile = path;
    }

/**
 * @brief Determines if a web is active at a given line.
 * This function checks if the specified line is part of the web's live range by verifying if it exists in any of the web's startLines, pointLines, or endLines sets.
 * 
 * @param w The web to check.
 * @param line The line number to check for activity.
 * @return true if the web is active at the given line, false otherwise.
 */
static bool webContains(const Web &w, int line) {
    return w.startLines.count(line) || w.pointLines.count(line) || w.endLines.count(line);
}


/**
 * @brief Determines whether two webs interfere (i.e., overlap in live ranges).
 *
 * Two webs interfere if they share at least one program line where both are
 * considered active. End lines are not treated as active to avoid false conflicts
 * when one web ends exactly where another starts.
 *
 * @param a First web.
 * @param b Second web.
 * @return true if the webs interfere, false otherwise.
 */
static bool webInterfere(const Web &a, const Web &b) {

    for (int line : a.startLines) {
        if (b.startLines.count(line) || b.pointLines.count(line)) return true;
    }
    for (int line : a.pointLines) {
        if (b.startLines.count(line) || b.pointLines.count(line)) return true;
    }
    return false;
}


/**
 * @brief Builds the interference graph based on live range overlaps between webs.
 *
 * Creates a graph where each vertex represents a web, and edges represent
 * interference (i.e., overlapping live ranges that cannot share a register).
 * Uses pairwise comparison of webs to detect conflicts.
 */
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

/**
 * @brief Performs spilling by iteratively removing highest-degree webs.
 *
 * Repeatedly selects the web with the maximum interference graph degree and
 * marks it as spilled (reg = -2), then removes it from the graph.
 *
 * Time Complexity:
 * Let n = number of webs, m = number of edges in the interference graph, and k = spill iterations.
 * Each iteration scans all webs (O(n)) and computes degree via adjacency list lookup (O(1) per vertex access,
 * but O(deg) for size depending on representation). Overall worst-case:
 *   O(k * (n + m))
 *
 * @param k Maximum number of webs to spill.
 */
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

/**
 * @brief Splits high-degree webs into two new webs to reduce register pressure.
 *
 * Repeatedly selects a candidate web (high interference degree and sufficient live points),
 * splits its timeline into two halves, and rebuilds interference information.
 *
 * Each split:
 *  - selects max-degree web
 *  - builds + sorts a timeline of live points
 *  - partitions into two sets
 *  - updates web sets and rebuilds the interference graph
 *
 * Time Complexity:
 * Let:
 *   n = number of webs
 *   m = number of edges in interference graph
 *   p = total number of live points per selected web (start+point+end)
 *   k = number of splitting iterations
 *
 * Per iteration:
 *   - scanning all webs: O(n)
 *   - finding vertex degree: O(1)–O(deg) per lookup, worst-case O(m)
 *   - building + sorting timeline: O(p log p)
 *   - rebuilding interference graph: O(n² * cost(webInterfere))
 *     where webInterfere depends on set lookups → O(s) average per check
 *
 * Overall worst-case per iteration:
 *   O(n + m + p log p + n² · s)
 *
 * Total for k iterations:
 *   O(k · (n + m + p log p + n² · s))
 *
 * @param k Maximum number of split operations.
 */
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

/**
 * @brief Attempts greedy graph coloring for register allocation.
 *
 * Assigns registers to webs using a greedy strategy ordered by decreasing
 * interference degree. If coloring fails (insufficient registers), all webs
 * are spilled to memory.
 *
 * Time Complexity:
 * Let:
 *   n = number of webs
 *   m = number of interference edges
 *   R = number of registers (numRegisters)
 *
 * Breakdown:
 *   - Reset phase: O(n)
 *   - Build order list: O(n)
 *   - Sorting by degree:
 *       degreeOf uses adjacency size → O(1)–O(deg)
 *       sorting cost: O(n log n)
 *   - Coloring loop:
 *       For each web:
 *         * findWebById: O(n)
 *         * inspect neighbors: O(deg)
 *         * register scan: O(R)
 *
 *   Worst-case coloring step:
 *       O(n · (n + deg + R)) ≈ O(n² + n·R + n·deg)
 *
 * Since Σdeg = O(m), total neighbor scanning across all vertices is O(m),
 * but repeated lookups dominate due to linear search.
 *
 * Overall worst-case:
 *   O(n² + m + n log n + n·R)
 *
 * Dominant term typically:
 *   O(n²) due to repeated findWebById lookups.
 *
 * @return true if coloring succeeds, false if spilling is required.
 */
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

/**
 * @brief Writes the register allocation result to the output file.
 *
 * Produces a formatted report of all webs, their live ranges, assigned registers,
 * and spilled (memory) webs. If no registers are available, all webs are emitted
 * as memory-resident.
 *
 * Output format:
 *  - Web definitions with + (start), plain (use), and - (end)
 *  - Register count used
 *  - Mapping of registers to webs
 *  - Memory-resident webs (M)
 */
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

/**
 * @brief Main driver for register allocation.
 *
 * Builds the interference graph, applies the selected optimization strategy
 * (none, spilling, or splitting), then attempts graph coloring. Finally
 * generates the output file.
 */
void Solver::allocateRegisters() {
    // 1) Always start by building the interference graph and trying plain coloring.
    //    Only if it fails do we execute spilling/splitting.
    buildInterferenceGraph();

    bool success = tryColoring();
    if (success) {
        generateOutput();
        return;
    }

    // 2) If plain coloring failed, iteratively apply the selected algorithm and retry.
    //    We interpret kParam as the maximum number of algorithm attempts.
    //    If algorithm isn't spilling/splitting, we stop (deemed impossible).
    for (int iter = 0; iter < kParam; ++iter) {
        // Rebuild interference graph at each attempt to ensure consistency.
        buildInterferenceGraph();

        if (algorithm == "spilling") {
            applySpilling(1);
        } else if (algorithm == "splitting") {
            applySplitting(1);
        } else {
            break;
        }

        success = tryColoring();
        if (success) {
            generateOutput();
            return;
        }
    }

    // 3) Still not possible after attempts -> generate output with memory-spilling state.
    generateOutput();
}


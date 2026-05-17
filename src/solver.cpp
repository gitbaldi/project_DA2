#include "../include/solver.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>


// constructor
Solver::Solver() {
    kParam = 0;
    numRegisters = 0;
}

/**
 * @brief Retrieves an existing web identifier for a variable or creates a new one.
 *
 * This function maintains a mapping between variable names and their corresponding
 * "web" (live range) identifiers. If the variable already has an associated web,
 * its ID is returned. Otherwise, a new web is created and registered.
 *
 * @details
 * ### Behavior
 * - If `varName` exists in `varToWebId`, returns the stored web ID.
 * - Otherwise:
 *   1. Creates a new `Web` object
 *   2. Assigns it a unique ID based on `allWebs.size()`
 *   3. Stores the variable name inside the web
 *   4. Inserts the new web into `allWebs`
 *   5. Updates `varToWebId` mapping
 *
 * @param varName Name of the source variable.
 * @return int Unique identifier for the corresponding web.
 *
 * @note This function guarantees that each variable maps to exactly one web
 *       for the lifetime of the Solver instance.
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
 * @brief Registers the start of a live range for a variable web.
 *
 * Associates the given source line with the live range of the specified
 * variable. If the variable does not yet have an assigned web identifier,
 * a new web is created automatically.
 *
 * @param varName Name of the variable whose live range is being updated.
 * @param line Source code line number where the live range begins or is active.
 */
void Solver::addLiveRangeStart(std::string varName, int line) {
    int webId = getOrCreateWebId(varName);
    allWebs[webId].lines.insert(line);
}

/**
 * @brief Adds a point in the live range of a variable.
 *
 * Marks the specified source line as part of the live range for the given variable.
 * If the variable has not yet been assigned a web identifier, a new one is created.
 *
 * This function is typically used to record intermediate points where the variable
 * remains live (i.e., neither defined nor killed but still in use).
 *
 * @param varName Name of the variable whose live range is being extended.
 * @param line Source code line number to be added to the variable's live range.
 */
void Solver::addLiveRangePoint(std::string varName, int line) {
    int webId = getOrCreateWebId(varName);
    allWebs[webId].lines.insert(line);
}

/**
 * @brief Marks the end of a variable's live range.
 *
 * Inserts the given source line into the set of lines associated with the
 * variable's live range. If the variable does not yet have an assigned web
 * identifier, a new one is created.
 *
 * This function is typically used to indicate the last point at which a
 * variable remains live before it is considered no longer in use.
 *
 * @param varName Name of the variable whose live range is being terminated.
 * @param line Source code line number representing the end of the live range.
 */
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

/**
 * @brief Performs register allocation using a greedy graph-coloring heuristic,
 *        with fallback spilling when registers are exhausted.
 *
 * This method assigns physical registers to "webs" (live ranges) based on an
 * interference graph. Webs that cannot be assigned a register due to conflicts
 * or register pressure are marked as spilled (reg == -2), meaning they must
 * reside in memory instead of a register.
 *
 * ### Algorithm Overview
 * 1. **Reset phase**
 *    - All non-spilled webs (reg != -2) are reset to unassigned state (reg = -1).
 *
 * 2. **Greedy allocation**
 *    - Iterate over all webs.
 *    - Skip webs already marked as spilled.
 *    - For each web, compute which registers are used by its neighbors in the
 *      interference graph.
 *
 * 3. **Register selection**
 *    - Assign the lowest-numbered free register.
 *    - If no register is available, mark the web as spilled.
 *
 * 4. **Edge cases**
 *    - Isolated webs (no interference edges) are assigned a register if available,
 *      otherwise spilled.
 *
 * ### Spilling Convention
 * - `reg >= 0` → assigned hardware register
 * - `reg == -1` → unassigned (temporary state during allocation)
 * - `reg == -2` → spilled to memory
 *
 * @note This is a simplified allocator; it does not perform optimal coloring or
 *       priority-based spilling heuristics.
 */
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

/**
 * @brief Generates the final register allocation output file.
 *
 * This function writes the computed live ranges ("webs"), register assignments,
 * and spill decisions to the output file specified by `outputFile`.
 *
 * It performs the full reporting stage of the register allocator pipeline:
 * - Dumps all live-range webs
 * - Executes register allocation if registers are available
 * - Prints final register assignments
 * - Marks spilled webs as memory-resident (M)
 *
 * @details
 * ### Output structure
 * The generated file follows this format:
 *
 * 1. Number of webs
 * 2. Each web with its associated program points (comma-separated)
 * 3. Number of used registers
 * 4. Register-to-web mappings (rX: webY)
 * 5. Memory mappings for spilled webs (M: webY)
 *
 * ### Special cases
 * - If `numRegisters <= 0`, all webs are treated as spilled to memory.
 * - If allocation is possible, `assignRegistersOrSpill()` is invoked.
 *
 * ### Register usage computation
 * The function computes the highest used register index and derives
 * `usedRegs = maxAssignedRegister + 1`, capped by `numRegisters`.
 *
 * ### Spill convention
 * - `w.reg >= 0` → assigned register
 * - `w.reg == -2` → spilled to memory (M)
 *
 * @note This is the final stage of the allocator pipeline and assumes that
 *       interference information and live ranges have already been constructed.
 *
 * @param outputFile Path to the file where allocation results are written.
 */
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

/**
 * @brief Constructs the interference graph for register allocation.
 *
 * This function builds an undirected interference graph where each node
 * represents a live-range web, and edges represent conflicts (overlaps)
 * between webs that cannot share the same register.
 *
 * @details
 * ### Construction steps
 * 1. **Graph initialization**
 *    - Resets the existing interference graph.
 *
 * 2. **Vertex insertion**
 *    - Each web in `allWebs` becomes a vertex in the graph, identified by `w.id`.
 *
 * 3. **Edge creation (conflict detection)**
 *    - For every pair of webs (i, j), where i < j:
 *      - Check whether they share any program line in their live-range sets.
 *      - If a shared line exists, an interference edge is added.
 *
 * 4. **Edge semantics**
 *    - The graph is undirected (bidirectional edges).
 *    - Edge weight is unused (set to 0).
 *
 * @note Complexity is O(n² · k) in the worst case, where:
 *       - n = number of webs
 *       - k = average number of live-range points per web
 *
 * @warning This implementation performs a nested scan over line sets,
 *          which may become expensive for large inputs. A more scalable
 *          approach would use interval merging or bitset-based overlap checks.
 *
 * @return void
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

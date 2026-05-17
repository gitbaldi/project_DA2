- [ ] Inspect remaining solver code paths (verify where assignRegistersOrSpill is called and how allocateRegisters triggers algorithms)
- [x] Implement 3 algorithms: basic (only tryColoring), spilling (applySpilling then tryColoring), splitting (applySplitting then tryColoring)

- [ ] Move all register assignment logic into tryColoring (remove greedy cheat logic from generateOutput)
- [ ] Implement failure behavior: if coloring impossible, set ALL webs to memory (reg = -2) and return false
- [ ] Ensure output generator uses existing web.reg state only
- [ ] Build & run (or compile) project with each algorithm option and validate outputs


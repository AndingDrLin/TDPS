# Final Report Revision Plan (Post-Review)

Generated: 2026-06-12 | Source: TA review of `src/main.tex`

---

## Phase 1: Critical Fixes (30 min)

### 1.1 Remove all agent comments
- **Lines**: 43, 66, 146, 289, 310, 328, 437, 488
- Delete all `% [REVISED by Agent N]` comment lines

### 1.2 Fix Figure 7 gap
- Figures numbered 1-6, 8-11 (no Fig. 7)
- Rename: fig8→fig7, fig9→fig8, fig10→fig9, fig11→fig10
- Update all `\includegraphics` paths, `\ref{}`, `\label{}`, captions

### 1.3 Resolve 7-vs-8 mechanism count
- Table II has 8 rows; text says "seven" (lines 121, 189, 231-238, 492)
- Merge "Integral limit & sep. threshold" into "Integral gain k_i" row

### 1.4 Fix overclaim: "global optimum" (line 445)
- Current: "is likely the global optimum for this controller structure"
- Fix: "is consistent with a single optimum in the tested range, with no competing peaks"

### 1.5 Add missing citations
- Line 72: "priority-inversion risks" → add citation
- Line 294: FMCW radar → add `\cite{ld2410s_datasheet}`

### 1.6 Fix "nine coupled parameters" (lines 463, 496)
- Fix: "nine tuned parameters (eight timing values plus one safety distance)"

---

## Phase 2: De-AI Language (20 min)

### 2.1 Rewrite AI-sounding sentences

| Line | Current | Replacement |
|------|---------|-------------|
| 47 | "making the project fundamentally an engineering integration challenge rather than a control problem in isolation" | "making integration, not control design alone, the primary challenge" |
| 59 | "Our contribution lies in the systematic simplification and co-optimization" | "This work simplifies and co-optimizes" |
| 151 | "three design principles that address" | "three processing stages that address" |
| 443 | "not merely a reduction in count but a systematic elimination of competing mechanisms" | "reduced the parameter count by eliminating competing mechanisms" |
| 445 | "providing empirical evidence of zero hidden coupling effects" | "suggesting negligible coupling among the remaining six parameters" |
| 447 | "This observation generalizes beyond line-following robotics." | "The simplify-first approach applies to any resource-constrained embedded control system with multiple interacting mechanisms." |
| 451 | "This distinction is critical for understanding the following limitations" | Delete sentence; let next sentence stand alone |
| 451 | "a strictly weaker requirement that substantially lowers the barrier for simulator adoption in embedded robotics" | "a weaker requirement that makes simulation-based tuning practical for resource-constrained teams" |
| 494 | "a strictly weaker fidelity requirement that substantially lowers the adoption barrier for..." | Delete entire clause; point already made in V.B |

### 2.2 "fundamentally" overuse (6→2)
- Keep lines 47, 294
- Line 92: "fundamentally governs" → "directly governs"
- Line 287: "fundamentally constrains" → "constrains"
- Lines 151, 463: already addressed above

### 2.3 Abstract vs conclusions wording
- Line 35 (abstract): "findings of general relevance" → "extend beyond this platform"
- Line 494 (conclusions): keep "extend beyond this specific platform"

### 2.4 Standardize "on-car" / "physical" / "vehicle"
- Unify all to "on-car" throughout

---

## Phase 3: Redundancy Cuts (15 min, ~200 words saved)

### 3.1 62→6 narrative (8→5 mentions)
- Delete line 221 (tables are self-explanatory)
- Section V.A opening (line 443): avoid restating abstract; start with kff ablation evidence

### 3.2 19% kff stat (7→4 mentions)
- Line 336: replace prose with "The optimal kff=0.0008 (Table V) reduces correction variance by 19%"
- Fig. 10 parent caption (line 379): remove "19% correction variance reduction" (already in subfig b)

### 3.3 22cm preview (15→~10 mentions)
- Line 115: merge with line 92 (both in II.B); delete standalone paragraph
- Line 287: delete final sentence "The fixed geometry... constrains both..."
- Line 334: remove "(22 cm lookahead)" parenthetical

### 3.4 Verbose passages to cut
- Line 72 (II.A): merge two RTOS sentences into one
- Line 88: "This layering enables PC-stub compilation of the core control logic, decoupled from hardware, without requiring mock frameworks or conditional compilation." → "This layering enables compiling the core control logic for PC without mock frameworks."
- Line 113: delete "This V1-to-V2 iteration demonstrates that the power architecture was not merely designed but physically verified on fabricated boards."
- Line 137: delete last two sentences of II.D (repeats simulator pitch)
- Line 296: "that would otherwise occur in any threshold-based detector subjected to measurement noise near the boundary" → "near the threshold boundary"
- Line 316/494: remove one of two near-identical weak-symbol sentences
- Line 416: "generalizes robustly to heterogeneous fault conditions rather than overfitting to a single disturbance type" → "consistent performance across all fault categories"
- Line 189: condense three-step methodology from 4 sentences to 2

---

## Phase 4: Technical Polish (15 min)

### 4.1 Add equation cross-references
- Section IV.A or V.A: add "Eq.~\ref{eq:correction}" and "Eq.~\ref{eq:speed}"

### 4.2 Abstract: mention full-course score
- Add: "...quick-gate score of 97.65/100 ...; the full-course simulation achieves 92.81/100."

### 4.3 Date specificity (line 121)
- "2026-06-01" → "during the final integration phase"

### 4.4 Motor speed units
- First mention of "speed 240" (III.B): add "(PWM duty cycle, 0--500 range)"

### 4.5 Caption fixes
- Fig. 3 caption (line 126): "M5 is on track; M6 is pending" → "M5--M6 culminate in full-system integration"
- Fig. 10 caption (line 379): "confirming" → "showing"

### 4.6 Weak-symbol finding reframing
- Conclusions (line 494): acknowledge it's a known GCC pattern, not a novel contribution → "the weak-symbol hook mechanism, a standard GCC pattern, achieved formal subsystem decoupling at zero runtime overhead"

### 4.7 Bibliography expansion (optional, 2-3 refs)
- RTOS priority inversion
- Non-blocking embedded architecture
- Weak-symbol / link-time injection patterns

---

## Execution Order

| Phase | Time | Description |
|-------|------|-------------|
| 1 | 30 min | Critical fixes — no judgment calls |
| 2 | 20 min | Sentence-level rewrites |
| 3 | 15 min | Deletion/shortening |
| 4 | 15 min | Scattered polish edits |
| **Total** | **~80 min** | |

---

## Post-Edit Verification

- [ ] `grep -c 'REVISED by' main.tex` → 0
- [ ] All `\ref{}` resolve (no undefined references)
- [ ] Figure numbers sequential 1--10, no gaps
- [ ] Mechanism count: all "seven" consistent with table
- [ ] "substantially" / "strictly weaker" / "fundamentally" each ≤ 2 occurrences
- [ ] Word count reduced ≥ 200 from current

# Revision Agent Prompts

These are 6 self-contained prompts for launching parallel agents.
Run all 6 in a single message with multiple Agent tool calls.
After Round 1 completes, run Round 2 to integrate.

---

## How to Launch

In Claude Code, send a message like:

"I want to run the 6 revision agents in parallel. Here are the prompts:"

Then include all 6 Agent calls. Each agent will read its reference files
and output a revised LaTeX section as its final message.

---

## Agent Prompts

### Agent 1 — Introduction

```
You are revising the Introduction (Section I) of an IEEE-style final report
for a university embedded robotics project. The project is a line-following
robot on STM32F407VET6 with radar avoidance and LoRa telemetry.

Read these reference files first:
- /Users/linyujia/Project/TDPS/docs/basic_design.md
- /Users/linyujia/Project/TDPS/docs/hardware.md
- /Users/linyujia/Project/TDPS/reports/mid_reports/scripts/report_content.py

Then read the current Introduction section from:
- /Users/linyujia/Project/TDPS/reports/final_report/src/main.tex
  (lines 45–68, from \section{Introduction} to just before \section{Overall System Design})

Output ONLY the replacement LaTeX for this section (from \section{Introduction}
through the end of \subsection{Report Structure}). Include the \section and all
\subsection commands.

Revision requirements:
1. Motivation: Start from the TDPS university team design project context —
   three mandatory tasks (line-following, radar avoidance, LoRa telemetry) that
   must coexist on one MCU without RTOS. Frame this as an engineering integration
   challenge, not just a control problem.
2. Problem statement: Emphasize the core tension between control loop timing,
   sensor geometry (22 cm preview), and communication latency. Keep the three
   coupled challenges but rewrite to be more concise and engineering-focused.
3. Literature survey: Restructure into two paragraphs — (a) control paradigms
   for line-following on resource-constrained platforms, (b) sensor/communication
   integration in multi-task embedded systems. Remove the differential-drive
   kinematics paragraph (it belongs in Section II/III). Keep all existing \cite{}.
4. Report structure: Keep as-is, just tighten wording.

Style: formal IEEE, third-person, no code identifiers in prose.
```

### Agent 2 — Overall System Design

```
You are revising Section II (Overall System Design Approach) of an IEEE-style
final report for a university embedded robotics project.

Read these reference files first:
- /Users/linyujia/Project/TDPS/docs/hardware.md
- /Users/linyujia/Project/TDPS/docs/basic_design.md
- /Users/linyujia/Project/TDPS/docs_reference/README.md
- /Users/linyujia/Project/TDPS/reports/mid_reports/scripts/report_content.py

Then read the current Section II from:
- /Users/linyujia/Project/TDPS/reports/final_report/src/main.tex
  (lines 72–141, from \section{Overall System Design} to just before
  \section{Subsystem Design})

Output ONLY the replacement LaTeX for this section (from \section{Overall System
Design Approach} through the end of the last \subsection in this section).

Revision requirements:
1. Architecture overview: Add one sentence on why tick-based non-blocking
   pattern was chosen over cooperative scheduling or RTOS, with concrete
   reasoning (3 bounded periodic sources, predictable worst-case execution time).
2. Hardware platform: Expand with power architecture details from the mid-report:
   LM2596S buck converter (5V/3A for motor driver), AMS1117-3.3 LDO (3.3V for
   MCU/logic), LM74700 reverse-polarity protection added in PCB V2. Mention
   the PCB V1→V2 evolution as physical implementation evidence. Reference
   pin mapping from docs/hardware.md.
3. Project planning: Tighten the refactoring paragraph — present as: what
   happened (62→6 params), why (root-cause analysis of 7 competing mechanisms),
   what enabled it (simulator-first workflow). Less narrative, more structured.
4. Simulation strategy: Minor wording tightening only. Ensure "30–40× speedup"
   is presented as measured, not asserted.

Style: formal IEEE, third-person. Preserve all \cite{}, \label{}, \ref{}.
Preserve all figure/table environments unchanged.
```

### Agent 3 — Line-Following Subsystem

```
You are revising Section III.A (Line-Following Subsystem) of an IEEE-style
final report for a university embedded robotics project.

Read these reference files first:
- /Users/linyujia/Project/TDPS/docs/line_following.md
- /Users/linyujia/Project/TDPS/docs/tuning.md

Then read the current Section III.A from:
- /Users/linyujia/Project/TDPS/reports/final_report/src/main.tex
  (lines 147–288, from \subsection{Line-Following Subsystem} to just before
  \subsection{Radar Obstacle Avoidance})

Output ONLY the replacement LaTeX for this subsection (from \subsection{Line-
Following Subsystem} through the end of the state machine discussion, ending
just before \subsection{Radar Obstacle Avoidance}).

Revision requirements:
1. Sensor processing: Emphasize design PRINCIPLES over implementation details.
   Keep the equation-level description but remove specific coefficient values
   from prose (they belong in tables/figures). Three principles: (a) normalization
   for ambient-light invariance, (b) low-pass filtering for noise rejection,
   (c) weighted centroid for sub-sensor resolution. The current version has too
   many raw numbers in prose.
2. PD+kff controller: Keep the control equation (Eq. 1) and speed equation
   (Eq. 2). Restructure the 62→6 simplification narrative as a three-step
   methodology: identify (root-cause failure analysis) → categorize
   (threshold/gain/rate-limiting) → eliminate (documented removal). Table II
   already lists all 7 eliminated mechanisms — do NOT repeat them all in prose,
   just refer to the table.
3. State machine: Reduce transition table verbosity — the table is already
   present, so prose should focus on design rationale. Add one sentence on
   the geometric lead compensation principle: why 22 cm sensor preview
   requires explicit advance-and-turn rather than controller retuning.
   Keep Table IV (transitions) and Fig. 6 (state machine) references.

Style: formal IEEE, third-person. Minimize raw code identifiers (variable
names, function names). Keep equations. Preserve all \cite{}, \label{}, \ref{}.
```

### Agent 4 — Radar and Wireless Subsystems

```
You are revising Sections III.B (Radar) and III.C (Wireless) of an IEEE-style
final report for a university embedded robotics project.

Read these reference files first:
- /Users/linyujia/Project/TDPS/docs/wireless_comm.md
- /Users/linyujia/Project/TDPS/docs/basic_design.md
- /Users/linyujia/Project/TDPS/docs/line_following.md (section 5 on extensions)

Then read the current Sections III.B and III.C from:
- /Users/linyujia/Project/TDPS/reports/final_report/src/main.tex
  (lines 289–317, from \subsection{Radar Obstacle Avoidance} through
  \subsection{Wireless Communication} to just before \section{Results})

Output ONLY the replacement LaTeX for these two subsections.

Revision requirements:
1. Radar: Restructure as (a) sensor characteristics and limitations
   (forward-only, no lateral discrimination — this IS the fundamental
   constraint), (b) parsing approach (non-blocking state machine, three
   packet formats — keep brief), (c) avoidance strategy and inherent
   limitations (open-loop timing, voltage sensitivity). Add one sentence
   connecting the 200 mm hysteresis gap to the general principle of
   preventing state thrashing in threshold-based detectors.
2. Wireless: Emphasize the ARCHITECTURAL CONTRIBUTION (weak-symbol hooks for
   subsystem decoupling) over protocol details. Current version spends too
   much space on message format and ring buffer. Restructure as: (a) integration
   challenge (LoRa AT commands block 50–200 ms), (b) solution architecture
   (weak-symbol link-time injection, non-blocking tick), (c) fault tolerance
   (timeout + retry + optional ACK). Keep Fig. 9 reference.
3. Both subsections should have consistent formality level.

Style: formal IEEE, third-person. Preserve all \cite{}, \label{}, \ref{}.
Preserve all figure/table environments.
```

### Agent 5 — Results

```
You are revising Section IV (Results) of an IEEE-style final report for a
university embedded robotics project.

Read these reference files first:
- /Users/linyujia/Project/TDPS/docs/tuning.md (section 11 — sweep results)
- /Users/linyujia/Project/TDPS/docs/line_following.md (section 4 — regression)

Then read the current Section IV from:
- /Users/linyujia/Project/TDPS/reports/final_report/src/main.tex
  (lines 322–418, from \section{Results} to just before \section{Analysis})

Output ONLY the replacement LaTeX for this section (from \section{Results}
through the end of the last \subsection in this section).

Revision requirements:
1. Parameter sweep: Rewrite prose to be ANALYTICAL, not descriptive. Instead
   of "Table X presents results," write "The coarse sweep confirmed a smooth
   single-peak landscape..." Emphasize what results MEAN, not just what they
   show. Keep Table III and Fig. 10/11 references. Keep Table V (sweep results).
2. Regression benchmark: Keep Table VI (per-scenario). Add one analytical
   paragraph: consistent performance across heterogeneous fault categories
   demonstrates robustness; sole weakness is lateral offset (attributable to
   22 cm sensor preview geometry).
3. On-car validation: Restructure as (a) nominal performance, (b) avoidance
   validation, (c) sim-to-physical discrepancies. Move LoRa field test details
   to a brief mention (1 sentence) — they are implementation verification,
   not results.
4. Use consistent significant figures. Round percentages to nearest integer
   unless fractional precision matters for the argument.

Style: formal IEEE, third-person. Preserve all \cite{}, \label{}, \ref{}.
Preserve all table/figure environments unchanged.
```

### Agent 6 — Analysis and Conclusions

```
You are revising Sections V (Analysis and Discussion) and VI (Conclusions
and Future Work) of an IEEE-style final report for a university embedded
robotics project.

Read these reference files first:
- /Users/linyujia/Project/TDPS/docs/line_following.md (section 6 — known limitations)
- /Users/linyujia/Project/TDPS/docs/basic_design.md (section 5 — LoRa/radar strategy)

Then read Sections V and VI from:
- /Users/linyujia/Project/TDPS/reports/final_report/src/main.tex
  (lines 423–481, from \section{Analysis} through \end{document})

Output ONLY the replacement LaTeX for these two sections.

Revision requirements:
1. V.A (Simplification): Keep core argument. Add brief connection to the
   broader embedded systems principle: reducing mechanism count is often
   more effective than optimizing individual parameters in
   resource-constrained systems.
2. V.B (Sim-to-real gap): Reframe three discrepancy categories as MODEL
   LIMITATIONS, not failures. State the key insight earlier (ordinal
   comparison, not absolute prediction, is the simulator's core value).
3. V.C (Cross-subsystem coupling): TIGHTEN — remove the buffer-overflow
   paragraph (verification detail, not analysis finding) and the speed-WARN
   redundancy paragraph (interesting but not load-bearing). Focus on:
   (a) timing budget breakdown, (b) avoidance-line-following interaction,
   (c) three system-level bottlenecks. Keep Table VII (limitations).
4. Conclusions: Tighten to 3 paragraphs: (a) what was achieved
   (quantitative), (b) three generalizable findings, (c) limitations
   and future directions. Remove Table VII reference from conclusion —
   reference it only in Section V.

Style: formal IEEE, third-person. Preserve all \cite{}, \label{}, \ref{}.
Keep bibliography unchanged.
```

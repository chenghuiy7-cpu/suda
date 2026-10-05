# Paper and Section Architecture

## Contents

- Paper-wide narrative
- Title
- Abstract
- Introduction
- Related work
- Methods
- Experiments and results
- Discussion, limitations, and conclusion
- Captions
- Cross-section traceability

## Paper-Wide Narrative

Build the manuscript around one defensible central thesis:

1. Establish why the problem matters to the target community.
2. Define the unresolved boundary of existing work.
3. State the paper's response at the same level of specificity as the gap.
4. Explain the mechanism that should address the gap.
5. Test the resulting claims with aligned experiments.
6. Interpret the evidence within explicit scope and limitations.

Do not confuse chronological research history with the clearest reader-facing narrative.

## Title

Make the title specific, searchable, and proportionate to the evidence.

- Name the task or phenomenon.
- Include the method or main finding when useful.
- Avoid empty novelty markers such as `novel`, `new`, or `revolutionary`.
- Avoid claims broader than the evaluated setting.

## Abstract

Use a compact question-answer sequence:

1. Context or stakes.
2. Specific limitation or unmet need.
3. The proposed response, introduced directly.
4. The core mechanism or differentiator.
5. The strongest verified evidence, preferably with concrete evaluation scope or numbers.
6. A bounded implication or use case.

Keep the abstract self-contained. Avoid literature surveys, implementation detail, unsupported adjectives, and citations unless the venue requires them.

## Introduction

Assign each paragraph a rhetorical job:

1. Define the problem and why it matters.
2. Group the main existing approaches by their strategy, not by a paper-by-paper list.
3. Explain the precise failure mode or missing capability.
4. Present the paper's thesis and why its mechanism addresses that failure.
5. Summarize the evaluation and contribution claims.

Use a contributions list only when the venue or complexity benefits from parallel inspection. Each contribution must be specific, verifiable later in the paper, and non-overlapping.

## Related Work

Synthesize rather than inventory.

- Organize prior work by idea, assumption, or limitation.
- State what each group solves and where its boundary lies.
- Position the current work at that boundary without dismissive language.
- Cite the original source for a method or claim whenever possible.
- Do not use citations as decoration or invent bibliographic details.

## Methods

Move from contract to mechanism:

1. Define the task, inputs, outputs, notation, and assumptions.
2. Give an overview of the full system and data flow.
3. Explain each component in dependency order.
4. Connect every component to the problem it solves.
5. Define objectives, training, optimization, and inference.
6. State complexity, deployment behavior, or removed training-only components when relevant.
7. Provide enough detail for reproduction or point to verified supplementary material.

Introduce an equation only after defining its symbols and before relying on its consequence. Explain what the equation accomplishes, not merely how it is computed.

## Experiments and Results

Organize the section by research questions:

- Does the method improve the main task?
- Which component causes the improvement?
- Does the method generalize across datasets, domains, or backbones?
- What is the sensitivity to key choices?
- What is the efficiency, calibration, or robustness trade-off?
- Where does the method fail?

For each result paragraph:

1. State the answer to the research question.
2. Give the decisive comparison and conditions.
3. Quantify the effect when supported.
4. Interpret the result without overclaiming mechanism.
5. Point to the relevant table or figure.

Report uncertainty, sample size, tests, or repeated runs when available. Distinguish statistical significance from practical importance.

## Discussion, Limitations, and Conclusion

Interpret rather than repeat the results table.

- Explain why the observed pattern is plausible.
- Separate demonstrated mechanism from post hoc interpretation.
- State operating conditions and generalization boundaries.
- Name concrete failure modes, resource costs, data dependencies, or evaluator assumptions.
- Use a specific example to make an abstract limitation legible.
- Propose future work that directly follows from a limitation.
- End with a calibrated statement of what the work enables, not a universal claim.

## Captions

Write captions so a reader can understand the visual without searching the main text for basic definitions.

Include, as applicable:

- the question or comparison;
- panel mapping;
- axes, units, and metric direction;
- dataset or evaluation condition;
- uncertainty and aggregation;
- sample size;
- statistical test or fit;
- abbreviations and symbols.

Do not duplicate the entire results discussion in the caption.

## Cross-Section Traceability

Before finalizing, create an internal trace:

| Promise | Method element | Evidence | Conclusion statement |
| --- | --- | --- | --- |
| Introduction claim | Component or design | Experiment, table, or figure | Bounded interpretation |

Repair any promise that lacks a method, any method claim that lacks evidence, and any conclusion that exceeds the introduction or experiments.

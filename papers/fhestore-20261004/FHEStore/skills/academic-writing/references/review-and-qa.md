# Review and Quality Assurance

## Contents

- Audit order
- Severity
- Evidence checks
- Logic and consistency checks
- Language checks
- Formatting checks
- Reviewer report standard

## Audit Order

Audit in this order:

1. Scientific truth and evidence.
2. Claim logic and cross-section consistency.
3. Terminology, notation, and reference consistency.
4. Language and readability.
5. Carrier and venue formatting.

Do not spend the user's attention on cosmetic wording while a validity problem remains.

## Severity

Classify issues by consequence:

- **Critical**: invalidates a central conclusion, comparison, experiment, or method definition.
- **Major**: materially weakens reproducibility, interpretation, novelty positioning, or a promised claim.
- **Minor**: localized clarity, grammar, notation, or formatting issue with an unambiguous repair.
- **Optional**: stylistic preference. Omit by default.

For each reported issue, give the exact location, the evidence, the consequence, and the smallest viable repair.

## Evidence Checks

- Trace each central claim to a table, figure, experiment, derivation, or verified citation.
- Recompute simple differences, percentages, rankings, or averages when source data are available.
- Check whether `significant` refers to a statistical test or merely to magnitude.
- Confirm that confidence intervals, error bars, and sample sizes are defined.
- Check whether a baseline is current, comparable, and evaluated under the same protocol.
- Check whether ablations isolate the claimed component.
- Check whether generalization claims cover the named datasets, domains, backbones, or populations.
- Flag causal interpretations from correlational evidence.

## Logic and Consistency Checks

- Compare abstract, introduction, methods, experiments, and conclusion claims.
- Verify that every advertised contribution appears in the method and is tested.
- Verify stable naming of concepts, datasets, losses, metrics, and modules.
- Verify symbols are defined before use and not reused with a different meaning.
- Check that a paragraph's conclusion follows from its evidence.
- Check that figure and table references point to the described content.
- Check that limitations do not silently contradict the main claim.

## Language Checks

- Remove ambiguity, broken syntax, and misleading modifiers.
- Prefer concrete subjects and verbs.
- Break a long sentence when its clauses perform different argumentative jobs.
- Merge short sentences when they fragment one logical unit.
- Remove promotional, vague, or emotionally loaded language.
- Remove mechanical transitions when sentence order already conveys the relation.
- Preserve a strong original sentence instead of rewriting it for variety.

## Formatting Checks

For LaTeX:

- Preserve commands, environments, labels, citations, and math delimiters.
- Detect unescaped special characters introduced by revision.
- Detect accidental Markdown, unmatched delimiters, or invented commands.
- Do not claim compilation without running it.

For Word-friendly text:

- Remove Markdown and LaTeX-only escapes.
- Use consistent punctuation and heading treatment.
- Keep formulas and symbols in the requested representation.

## Reviewer Report Standard

Write a strict but evidence-based review.

### Summary

State the paper's core claim and contribution in one compact paragraph.

### Strengths

Name one to three genuine contributions and explain why they matter to the field.

### Critical Weaknesses

Report only concrete problems. Replace `experiments are insufficient` with the missing dataset, baseline, control, ablation, statistical analysis, or failure-case evaluation and explain which claim it affects.

### Rating

Align the score with the venue and the paper's actual contribution, rigor, clarity, and evidence. Do not apply a preset harshness.

### Strategic Advice

For each weakness:

- identify whether the root cause is methodological, experimental, or presentational;
- state whether it can realistically be repaired during revision;
- give a concrete action for the manuscript, experiment, or rebuttal.

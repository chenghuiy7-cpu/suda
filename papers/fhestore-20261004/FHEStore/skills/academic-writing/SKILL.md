---
name: academic-writing
description: Draft, translate, restructure, revise, shorten, expand, humanize, analyze, and reviewer-audit academic papers and paper sections in English or Chinese while preserving LaTeX or Word-friendly formatting and grounding every claim in supplied evidence. Use for titles, abstracts, introductions, related work, methods, experiments, discussions, limitations, figure or table captions, Chinese-English academic translation, LaTeX or plain-text polishing, evidence-based experiment analysis, paper-wide coherence checks, and reviewer-style reports. Do not use for literature discovery alone, citation fabrication, or drawing figures; pair with appropriate research, citation, document, or visualization tools when those operations are needed.
---

# Academic Writing

Produce publication-ready research prose by organizing verifiable claims, evidence, and scope before optimizing language. Prefer accurate, necessary edits over conspicuous rewriting.

## Apply the Source Hierarchy

Resolve conflicts in this order:

1. Follow the user's explicit request and output contract.
2. Follow the named venue, template, discipline, and language conventions.
3. Preserve facts, numbers, equations, citations, terminology, and constraints from the supplied materials.
4. Apply the README-derived standard in [references/core-writing-standard.md](references/core-writing-standard.md).
5. Use the top-journal patterns in [references/top-journal-patterns.md](references/top-journal-patterns.md) only as supplementary rhetorical guidance.

Never let an exemplar's style override the user's evidence or the primary standard.

## Choose the Operation

Classify the request before writing:

- Draft or restructure a paper, section, paragraph, title, or caption.
- Translate between Chinese and English.
- Polish, shorten, expand, or humanize existing prose.
- Analyze experimental results.
- Run a logic, consistency, or reviewer audit.

Read [references/task-modes.md](references/task-modes.md) for the selected operation. For section-level or paper-level drafting, also read [references/section-architecture.md](references/section-architecture.md). For audits, reviews, or final checks, read [references/review-and-qa.md](references/review-and-qa.md).

## Establish the Writing Contract

Identify, from the request and available files:

- the target deliverable and section;
- the audience, field, and venue;
- the output language;
- the carrier: LaTeX, Word-friendly plain text, Markdown, or another explicit format;
- the allowed evidence: manuscript, repository, notes, tables, figures, logs, citations, and data;
- the required length, level of intervention, and output parts.

Infer harmless defaults when possible. Ask only when a missing choice would materially change the scientific claim or artifact.

## Build a Claim–Evidence Map

Before drafting a full section or analyzing results, map each intended claim to:

- the exact supporting source;
- the comparison, measurement, derivation, or citation that supports it;
- the relevant condition or boundary;
- the status: supported, inferential, or unsupported.

Keep this map internal unless requested. Omit unsupported claims or mark them with an explicit placeholder such as `[CITATION NEEDED]`, `[RESULT NEEDED]`, or `[VERIFY]`. Never invent a citation, number, experiment, mechanism, or causal explanation.

## Compose from Logic to Language

Work in this order:

1. State the paragraph's single rhetorical job.
2. Arrange claims in a natural sequence such as context → gap → response, mechanism → consequence, or result → evidence → interpretation.
3. Make the topic sentence carry the paragraph's main claim.
4. Place evidence next to the claim it supports.
5. State the implication without exceeding the evidence.
6. Add transitions only when the relation is not already clear.
7. Polish syntax and vocabulary last.

Use the structure `claim → mechanism or reason → evidence → bounded implication` when it fits; do not force every paragraph into the same template.

## Preserve Scientific and Formatting Integrity

For all outputs:

- Preserve the author's technical meaning and established terminology.
- Prefer common, precise words over ornate or fashionable vocabulary.
- Avoid promotional language, empty significance claims, mechanical transition stacks, gratuitous triads, and unnecessary em dashes.
- Avoid edits made only to produce visible change.
- Default to present tense for methods, architecture, figures, tables, and reported conclusions. Use past tense for completed procedures or historical events when clarity or venue convention requires it.
- Keep one core idea per paragraph, but retain lists when they are genuinely clearer for algorithms, constraints, checklists, or review findings.

For LaTeX:

- Preserve commands, environments, labels, references, citations, comments, and mathematics unless the user asks to change them.
- Escape newly introduced `%`, `_`, `&`, `#`, and other special characters when they are text, not syntax.
- Do not double-escape existing content.
- Do not add emphasis, itemization, or packages without a clear need.

For Word-friendly text:

- Return plain text without Markdown unless requested.
- Use normal symbols such as `%`, `_`, and `&` without LaTeX escaping.
- Preserve mathematical notation in the user's chosen Word-compatible form.

For Chinese:

- Use modern, formal academic Chinese and full-width Chinese punctuation.
- Remove colloquial or translated-sounding structures without replacing them with archaic bureaucratic language.
- Retain established English technical terms when forced translation would reduce precision.

For English:

- Use clear academic prose without contractions.
- Avoid unnecessary possessive constructions for method, model, or system names when an `of` phrase or noun modifier is clearer.
- Keep abbreviations as provided unless expansion is needed for first-use clarity.

## Enforce the Output Contract

Return only the requested artifact and requested auxiliary parts. If the user gives no explicit format, use the selected mode's default in [references/task-modes.md](references/task-modes.md).

Do not expose hidden reasoning, the claim–evidence map, or self-review. Report unresolved evidence gaps only when they affect correctness.

## Run the Final Gate

Before delivery, verify:

1. Every substantive claim is supported or explicitly qualified.
2. No number, condition, qualifier, citation, formula, or technical detail was lost.
3. The introduction's promises are tested in the experiments and reflected in the conclusion.
4. Comparisons use aligned baselines, metrics, datasets, and conditions.
5. Causal language is supported by causal evidence; otherwise use associative language.
6. Terminology, notation, abbreviations, tense, and figure or table references are consistent.
7. Each paragraph has one main job and no logic jump.
8. The output carrier is clean and copy-ready.
9. The revision is no broader than the request.
10. The prose sounds like a careful researcher, not a promotional summary or generic AI response.

Use [references/review-and-qa.md](references/review-and-qa.md) for the full audit standard.

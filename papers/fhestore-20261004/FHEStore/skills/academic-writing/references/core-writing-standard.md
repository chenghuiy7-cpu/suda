# Core Academic Writing Standard

## Contents

- Evidence and fidelity
- Logic and paragraph construction
- Style and tone
- Revision threshold
- Tense and terminology
- Carrier-specific rules
- Tables, figures, and experimental prose

## Evidence and Fidelity

Treat scientific accuracy as the first constraint.

- Preserve all supplied facts, parameters, datasets, metrics, equations, definitions, citations, and scope conditions.
- Distinguish observation, interpretation, speculation, and recommendation.
- Use causal verbs only when the design supports causality.
- Do not manufacture missing support. Use explicit placeholders when a required item cannot be verified.
- Describe null, mixed, or negative results honestly. Do not force a positive narrative.
- Keep the strength of the claim proportional to the evidence. Prefer `suggests`, `is consistent with`, or `under these conditions` when stronger language is not justified.

## Logic and Paragraph Construction

Organize ideas before polishing sentences.

- Give each paragraph one dominant purpose.
- Select a natural order based on the material: general-to-specific, problem-to-response, cause-to-effect, chronological, or result-to-interpretation.
- Put the claim near its evidence.
- Define an entity before using its abbreviation, symbol, or consequence.
- Explain why a design choice is needed before detailing how it works.
- Make contrast explicit when comparing prior work, baselines, or conditions.
- Avoid transitions that merely announce sequence. Use them only to reveal a genuine relation.
- Convert loose notes or lists into prose when the content forms one argument. Retain a list when parallel, independently actionable items are easier to inspect as a list.

## Style and Tone

Use clear, restrained, modern academic language.

- Prefer short, familiar, precise words.
- Remove fillers such as `in order to`, `it is worth noting that`, and empty claims of importance.
- Avoid ornamental synonyms, emotional intensifiers, promotional claims, and vague superlatives.
- Avoid frequent em dashes, excessive parentheses, and formatting used as a substitute for argument.
- Avoid repetitive sentence openings and mechanical `First/Second/Finally` sequences.
- Do not ban a word merely because it is common in AI text; replace it only when it is imprecise, inflated, or repetitive in context.
- Let variation come from the logic and sentence rhythm, not from random synonym replacement.

## Revision Threshold

Make the smallest change that solves the actual problem.

Always correct:

- factual distortion;
- contradiction or logic discontinuity;
- ambiguous core terminology;
- grammar or punctuation that obstructs meaning;
- unsupported claims;
- malformed LaTeX or carrier-incompatible formatting.

Usually preserve:

- a clear sentence that merely admits another stylistic option;
- the author's established voice;
- a correct technical term or abbreviation;
- an intentional list, emphasis command, or sentence pattern that serves the content.

When the input already meets the standard, return it unchanged and state that no substantive change was needed if the selected output mode includes a review log.

## Tense and Terminology

- Use present tense by default for the proposed method, architecture, equations, figures, tables, and conclusions that the paper presents.
- Use past tense for a completed experimental procedure or a dated historical event when needed.
- Keep a term, symbol, method name, and abbreviation stable across the paper.
- Expand an abbreviation at first use only when the venue or audience requires it.
- Preserve accepted English terms in Chinese prose when translation would be nonstandard or ambiguous.
- Prefer objective subjects such as `the results`, `the analysis`, or the method name when they improve precision; do not mechanically remove every `we`.

## Carrier-Specific Rules

### LaTeX

- Preserve `\cite{}`, `\ref{}`, `\label{}`, custom macros, environments, and mathematics.
- Preserve existing `\textbf{}` or `\emph{}` when the task is language-only; do not introduce new emphasis without a reason.
- Escape special characters only when introducing them as text.
- Keep formulas unchanged unless the user requests mathematical editing.
- Return compilable-looking source, but do not claim compilation unless it was actually tested.

### Word-Friendly Plain Text

- Emit clean plain text without Markdown syntax.
- Use standard characters directly rather than LaTeX escapes.
- Use full-width Chinese punctuation in Chinese prose.
- Preserve formula delimiters or equation formatting as supplied unless the user requests conversion.

## Tables, Figures, and Experimental Prose

- Make captions self-contained enough to identify the task, variables, conditions, direction of better performance, and statistical notation.
- Do not begin a caption with empty framing such as `This figure shows`.
- Use sentence case for full-sentence captions unless the venue requires another convention.
- State sample sizes, uncertainty, tests, or aggregation procedures when they are necessary to interpret the visual.
- Analyze experiments by comparison, trend, robustness, sensitivity, trade-off, or failure mode rather than by reading every cell aloud.
- Report exact numbers only when they prove the sentence's point.
- Align every claimed improvement with the correct baseline, metric direction, dataset, split, and evaluation condition.
- Use confidence intervals or error bars when repeated measurements exist; do not invent uncertainty for single runs.

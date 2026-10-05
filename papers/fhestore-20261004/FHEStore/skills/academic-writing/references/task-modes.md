# Task Modes and Default Output Contracts

## Contents

- General selection rule
- Draft or restructure
- Chinese to English
- English to Chinese
- English polish
- Chinese polish
- Shorten
- Expand
- Humanize
- Logic audit
- Experiment analysis
- Titles and captions
- Reviewer audit

## General Selection Rule

Honor the user's explicit output format. Apply the defaults below only when the user does not specify one. Do not add commentary outside the defined parts.

## Draft or Restructure

Use for a new paragraph, section, or full paper.

1. Read all supplied evidence before drafting.
2. Build a claim–evidence map.
3. Select the relevant section pattern from `section-architecture.md`.
4. Draft for scientific logic before sentence-level polish.
5. Insert `[CITATION NEEDED]`, `[RESULT NEEDED]`, or `[VERIFY]` instead of fabricating support.

Default output:

- Return the requested prose in the requested carrier.
- Add a short `Evidence gaps` block only when unresolved gaps would otherwise be hidden.

## Chinese to English

Preserve the meaning while producing natural academic English. Reorganize logic only when the user asks for rewriting or the source is too fragmented to translate coherently.

For LaTeX:

- Preserve commands and mathematics.
- Escape newly introduced special characters.

For Word:

- Return plain text with unescaped symbols.

Default output:

1. `Part 1 [English]`: translated academic text.
2. `Part 2 [Back Translation]`: close Chinese back-translation for meaning verification.

## English to Chinese

Use a close translation for reading comprehension unless the user asks for polished Chinese.

- For the default clean reading translation, remove citation, label, and reference commands; retain and translate the content inside formatting commands without their wrappers; and render mathematics as readable plain language or ordinary symbols.
- Preserve the LaTeX commands and mathematics instead when the user explicitly requests a source-preserving translation.
- Keep the Chinese sentence order close enough to the source for easy alignment unless the user asks for rewriting.

Default output:

- Return only the Chinese translation.

## English Polish

Correct grammar, syntax, coherence, and academic register while preserving scientific content and LaTeX.

- Keep existing commands and emphasis.
- Do not expand familiar abbreviations without need.
- Avoid contractions and unnecessary method-name possessives.
- Do not convert paragraphs to lists.

Default output:

1. `Part 1 [Revised Text]`.
2. `Part 2 [Chinese Translation]`.
3. `Part 3 [Modification Log]`: brief Chinese description of substantive changes.

## Chinese Polish

Use modern academic Chinese. Repair colloquial wording, broken logic, and translation-like syntax without rewriting clear passages.

Default output:

1. `Part 1 [Refined Text]`.
2. `Part 2 [Review Comments]`: list necessary changes, or state that the original is clear and was preserved.

## Shorten

Default to a small reduction of roughly 5–15 English words for a paragraph-sized input unless the user gives another target.

Prefer:

- removing fillers and duplication;
- changing a clause to a shorter phrase;
- choosing a direct verb;
- combining repeated conditions.

Never remove a result, parameter, qualifier, citation, or technical distinction.

Default output:

1. `Part 1 [Revised Text]`.
2. `Part 2 [Chinese Translation]`.
3. `Part 3 [Modification Log]`.

## Expand

Default to a small increase of roughly 5–15 English words for a paragraph-sized input unless the user gives another target.

Add only:

- an already implied premise;
- a necessary logical relation;
- a bounded interpretation supported by the input;
- a missing definition recoverable from supplied context.

Do not add data, novelty claims, citations, or causal explanations without evidence.

Use `[DETAIL NEEDED]` when meaningful expansion requires new information.

Default output:

1. `Part 1 [Revised Text]`.
2. `Part 2 [Chinese Translation]`.
3. `Part 3 [Modification Log]`.

## Humanize

Remove detectable mechanical writing without manufacturing informality.

Check for:

- empty significance statements;
- promotional or dramatic tone;
- vague attribution;
- excessive em dashes or parentheticals;
- repeated `Moreover`, `Furthermore`, or `Notably`;
- forced three-part lists;
- ornamental AI-associated words used without technical need;
- identical sentence rhythms;
- meta-writing that announces what the paragraph will do.

Preserve a natural, high-quality input unchanged.

Default output:

For English input:

1. `Part 1 [Revised Text]`.
2. `Part 2 [Chinese Translation]`.
3. `Part 3 [Modification Log]`, or `[PASS] The original is natural and requires no substantive change.`

For Chinese input:

1. `Part 1 [Revised Text]`.
2. `Part 2 [Modification Log]`, or `[检测通过] 原文表达严谨自然，无明显 AI 痕迹，建议保留。`

## Logic Audit

Assume a mature draft and report only issues that can affect understanding or correctness.

Inspect:

- contradictions;
- claim–evidence mismatch;
- inconsistent terms or notation;
- missing premises;
- ambiguous referents;
- invalid comparison conditions;
- severe grammar that changes meaning.

Ignore optional stylistic substitutions.

Default output:

- If no substantive issue exists, return `[PASS] No substantive logic or consistency issue detected.`
- Otherwise, list each issue with its location, why it matters, and the smallest repair.

## Experiment Analysis

1. Read the complete table, figure, or raw results.
2. Confirm metric direction, baseline, dataset, split, and units.
3. Identify the question each experiment answers.
4. Select the few comparisons that support the main claim.
5. Report trade-offs, mixed outcomes, and uncertainty.
6. Avoid unsupported significance language.

For LaTeX, default to one or more `\paragraph{Claim-Oriented Heading}` blocks with continuous prose and no added emphasis.

Default output:

1. `Part 1 [Analysis]`.
2. `Part 2 [Chinese Translation]` when Part 1 is English.

## Titles and Captions

For titles:

- Name the subject and, when justified, the central contribution or finding.
- Avoid hype, unexplained abbreviations, and vague nouns such as `framework` alone.

For figure captions:

- Start with the figure's informational content, not `The figure shows`.
- Identify panels, variables, conditions, uncertainty, and notation needed for independent reading.

For table captions:

- Prefer direct forms such as `Comparison with ...`, `Ablation study of ...`, or `Results on ...` when accurate.

Default output:

- Return only the title or caption, without `Figure 1:` or `Table 1:` unless requested.
- Use sentence case for full sentences. Follow the venue for title case or punctuation.

## Reviewer Audit

Read the full paper and target venue before scoring.

Separate:

- contribution and strengths;
- critical validity or evidence problems;
- correctable presentation or experiment gaps;
- structural limitations unlikely to be repaired during revision.

Default output:

1. `Part 1 [Review Report]`: Summary, Strengths, Critical Weaknesses, Rating and rationale.
2. `Part 2 [Strategic Advice]`: root cause, repairability, and concrete revision or rebuttal actions.

Do not penalize a paper merely for lacking mathematical complexity. Judge whether the stated contribution is useful, rigorous, and supported.

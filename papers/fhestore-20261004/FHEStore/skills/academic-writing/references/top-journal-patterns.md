# Supplementary Patterns from Top-Journal Papers

## Contents

- Role and limits of these exemplars
- Abstract pattern
- Introduction pattern
- Results pattern
- Discussion and limitation pattern
- Figure-caption pattern
- Sources examined

## Role and Limits of These Exemplars

Use these observations as supplementary rhetorical guidance. Do not copy phrasing, imitate domain-specific conventions blindly, or override the primary writing standard and target venue.

The patterns below were distilled from high-impact Nature papers in artificial intelligence and computational science. They are observations, not universal rules.

## Abstract Pattern

The examined abstracts commonly:

1. Open with the real scientific or societal need.
2. Name the current technical boundary.
3. Introduce the work directly with a `Here we...` move or equivalent.
4. Describe the differentiating mechanism in one or two sentences.
5. Give decisive evidence with concrete evaluation scope or numbers.
6. End with a bounded implication.

AlphaFold moves from the gap between known sequences and experimentally determined structures to the prediction problem, then states the method, blind evaluation, and accuracy claim. GenCast moves from forecast uncertainty to the weakness of deterministic ML forecasting, then gives resolution, horizon, runtime, evaluation count, and relative skill. FunSearch moves from LLM capability and hallucination risk to an evaluator-guided search procedure, then demonstrates two distinct problem classes. AlphaGo moves from the search-space challenge to policy and value networks, training strategy, search, and match outcomes.

Transfer this sequence, not the wording.

## Introduction Pattern

Strong introductions in the examined papers use functional paragraphs:

- establish stakes with concrete consequences;
- group prior approaches by strategy;
- explain why the strongest current approach still fails under a named condition;
- state the new method and the exact boundary it crosses;
- preview the evaluation needed to prove that claim.

The transition from prior work to the new contribution is specific. It identifies a failure mode such as missing homologues, deterministic uncertainty, unreliable generation, or an intractable search space rather than asserting a generic `research gap`.

## Results Pattern

The examined results sections:

- lead with the question or claim, then provide numbers;
- define the comparison protocol before interpreting the outcome;
- pair relative comparisons with absolute scale where useful;
- connect claims to figures immediately;
- report uncertainty, sample size, or evaluation coverage when available;
- separate realistic case studies from aggregate validation;
- test more than the headline metric when the scientific claim has multiple dimensions.

GenCast, for example, decomposes probabilistic quality into realism, marginal skill, calibration, and joint spatiotemporal behavior. AlphaFold links blind competition results, structural error, side-chain quality, confidence estimation, and transfer to recent structures. This is a useful model: decompose a broad claim into independently testable properties.

## Discussion and Limitation Pattern

The examined discussions explain mechanism cautiously, specify scope, and then move to implications.

- FunSearch names conditions under which the method currently works best, including an efficient evaluator, informative scoring, and an isolatable program component. It makes the limitation concrete by contrasting tasks with rich scores and tasks without them.
- GenCast identifies resolution and computational cost as operational constraints and links each to a plausible improvement path.
- AlphaFold connects design choices to observed capability and describes likely applications without claiming that every biological problem is solved.

Use the pattern `supported interpretation → operating boundary → concrete limitation → repair direction → bounded implication`.

## Figure-Caption Pattern

The examined Nature captions are substantially self-contained. They define:

- panel roles;
- datasets and conditions;
- sample sizes;
- metrics and units;
- uncertainty or fitting procedures;
- exclusions or filters needed for interpretation.

Use this density when a figure carries scientific evidence. Keep a conceptual framework caption shorter when the visual and main text already define the mechanism.

## Sources Examined

- Jumper et al., [Highly accurate protein structure prediction with AlphaFold](https://www.nature.com/articles/s41586-021-03819-2), *Nature* 596, 583–589 (2021).
- Silver et al., [Mastering the game of Go with deep neural networks and tree search](https://www.nature.com/articles/nature16961), *Nature* 529, 484–489 (2016).
- Price et al., [Probabilistic weather forecasting with machine learning](https://www.nature.com/articles/s41586-024-08252-9), *Nature* 637, 84–90 (2025; published online 2024).
- Romera-Paredes et al., [Mathematical discoveries from program search with large language models](https://www.nature.com/articles/s41586-023-06924-6), *Nature* 625, 468–475 (2024).

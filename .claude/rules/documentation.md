---
description: Keep all documentation (Doxygen API comments, README, and the design doc) in sync whenever code changes.
applyTo: "**"
---

# Rule: When code changes, update the documentation in the same task

Documentation in this BSP is part of the deliverable, not an afterthought. Any
change to code that alters observable behavior, API surface, configuration, or
structure MUST be accompanied by the matching documentation update **in the same
task**. Do not leave docs to "a follow-up."

## Documentation surfaces that must stay current

1. **Doxygen API comments** — the `/** ... */` blocks on every public function,
   enum, struct, macro, and `@param` / `@return` tag in `lib/*/include/*.h` and
   [include/core2foraws.h](../../include/core2foraws.h). When you add, remove,
   rename, or change the signature/semantics of a public symbol, update its
   Doxygen block (brief, parameters, return values, notes, and any `@code`
   examples) to match. Keep the `/* @[declare_...] */` marker pairs intact so
   the generated docs keep working.
2. **The design document** — [.claude/design.md](../design.md). Governed in
   detail by [.claude/rules/design-doc.md](design-doc.md); follow that rule for
   init flow, module catalog, shared-resource, power, build, and pin sections.
3. **README** — [README.md](../../README.md). Update it when you change
   supported toolchain/ESP-IDF versions, build or usage instructions, dependency
   model, or anything else a consumer of the BSP relies on.
4. **In-code comments** — only the comments adjacent to code you actually
   changed. Per the project's implementation discipline, do not add or rewrite
   comments on code you did not touch.

## Workflow

1. Make the code change.
2. Identify every documentation surface above that the change touches.
3. Update those docs in the same task so code and docs land together.
4. If the change is hardware-facing, also follow
   [.claude/rules/hardware-sources.md](hardware-sources.md) and
   [.claude/rules/design-doc.md](design-doc.md).

## What not to do

- Do not merge a code change that leaves Doxygen comments, the README, or the
  design doc describing the old behavior.
- Do not document features, parameters, or modules that do not exist in the
  code.
- Do not let the version/`@brief` headers, `@param`/`@return` tags, or example
  snippets drift from the actual signatures.
- Do not expand docs beyond what the change requires; keep them accurate and
  scoped, not padded.

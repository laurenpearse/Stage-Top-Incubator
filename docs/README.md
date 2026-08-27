# Docs

- `thesis/chapters/` — thesis chapters as source files (LaTeX .tex or Word .docx — whichever your department requires). Name files so they sort in reading order, e.g. `01-introduction.tex`, `02-literature-review.tex`, `03-methodology.tex`.
- `thesis/figures/` — figures/diagrams referenced from the chapters.

Compiled output (PDFs built from LaTeX) generally shouldn't be committed — the root `.gitignore` excludes common LaTeX build artifacts. If you want the rendered PDF versioned too, add an explicit exception for it.

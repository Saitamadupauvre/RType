# Contributing to R-Type

The contribution guide is [docs/contributing.md](docs/contributing.md), and
the full code rules are in [AGENTS.md](AGENTS.md).

In short:

1. One issue → one branch from `dev` → one pull request into `dev` with
   `Closes #<issue>`.
2. Every change comes with GoogleTest tests and updated documentation.
3. Run `./fred.sh all` before pushing; CI must be green before merging.

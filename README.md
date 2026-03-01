# Compiladores Lab Project

## Overview
This repository is organized into 12 C project folders: 11 practice folders (`p1` to `p11`) and 1 final project folder (`final_proyect`).
Each practice is a standalone deliverable and, at the same time, a module-building step for the final project.

## Project Structure
```
compiladores_lab_2026-2/
├─ README.md
├─ .gitignore
├─ final_proyect/      # Final integrated project (C + Make)
│  ├─ Makefile
│  ├─ include/
│  ├─ src/
│  │  └─ main.c
│  ├─ tests/
│  ├─ build/
│  │  ├─ obj/
│  │  └─ bin/
│  ├─ lib/
│  └─ docs/
├─ p1-regex_to_nfa/    # Practice 1 example (C + CMake)
│  ├─ CMakeLists.txt
│  ├─ Dockerfile
│  ├─ include/
│  ├─ src/
│  │  ├─ main.c
│  │  └─ others.c
│  ├─ tests/
│  ├─ docs/
│  ├─ validator/
│  └─ build/
│     └─ .cmake/
├─ p2-.../
│  └─ ...
├─ ...
└─ p11-.../
   └─ ...
```

## Build & Run
Requirements: Docker, GCC (or Clang), Make, and CMake (for projects that use CMake).

Each practice is built and tested inside its own folder. The final project is also built in its own folder.

```bash
cd p1-regex_to_nfa
cmake -S . -B build
cmake --build build
```

```bash
cd final_proyect
make
make run
```

## Branch Strategy (Branching Model)

We follow a lightweight GitFlow approach:

- `main`: stable, production-ready code. Only merged via Pull Requests.
- `develop`: semester integration branch.
- `feature/<target>-<short-description>`: new features developed from `develop`.
- `bugfix/<target>-<short-description>`: non-urgent fixes developed from `develop`.
- `hotfix/<short-description>`: urgent fixes branched from `main` and merged back into both `main` and `develop`.

`<target>` should identify the folder scope, for example: `p03`, `p11`, or `final_proyect`.

### Commit Message Convention

All commits should follow this format:

`<type>(<scope>): <summary>`

- Use each `type` as follows:
  - `feat`: add new functionality (new behavior, new API, new module capability)
  - `fix`: correct a bug or wrong behavior
  - `refactor`: change internal code structure without changing behavior
  - `test`: add or update tests only
  - `docs`: documentation-only changes
  - `build`: build-system or dependency changes (CMake, Makefile, compiler flags)
  - `ci`: CI/CD pipeline changes (GitHub Actions, GitLab CI, validation jobs)
  - `chore`: maintenance tasks not covered above (cleanup, renames, tooling configs)
  - `perf`: performance improvements without functional changes
- `scope`: folder, file, or module scope (for example: `include`, `src`, `src/regex.c`, `tests`, `cmake`)
- `summary`: imperative present tense, concise, and no trailing period

When a file contains mixed logical changes, use patch mode to split commits cleanly:

```bash
git add -p
```

Examples:

- `feat(include): define initial regex API for main integration and testing`
- `fix(src): correct epsilon transition handling in NFA construction`
- `refactor(src/regex.c): extract parse_union helper`
- `test(tests): add unit tests for alternation and concatenation`

## Course Methodology: Practices + Final Integration

This repository contains 12 project folders developed during the semester:

1) **11 Practice Projects**: `p1` to `p11`, each implemented and validated as a standalone practice.
2) **1 Final Project**: `final_proyect`, where validated modules from practices are integrated into a complete compiler project.

### Relationship Between Practices and Final Project
Each practice is designed as a small part of the final project.
After a practice is validated, its module(s) are adapted and integrated into `final_proyect`.
By the end of the semester, `final_proyect` should include the accumulated work from the 11 practices.

### Git Workflow per Practice and Final Integration
For each practice `pXX`:

1. Create a short-lived branch from `develop`:
   - `feature/pXX-<topic>` for new work
   - `bugfix/pXX-<topic>` for fixes

2. Implement and validate the work in the corresponding practice folder (`pXX-*`).

3. Open a Pull Request into `develop` and merge only when:
   - The practice builds successfully
   - The practice output/behavior is correct

4. Integrate the validated module(s) into `final_proyect` in a dedicated branch:
   - `feature/final-<integration-topic>`

5. Tag merge commits to freeze delivery states:
   - Practice checkpoints: `p01`, `p02`, ..., `p11`
   - Final project milestones: `final-v1`, `final-v2`, ...

Example:
```bash
git checkout develop
git pull
git checkout -b feature/p03-buffer-tokenizer

# Implement and test practice in p03-...

git add .
git commit -m "feat(p03): implement buffered input and tokenizer"
git push -u origin feature/p03-buffer-tokenizer

# Open PR -> develop (GitHub/GitLab)
# Merge PR

git checkout develop
git pull
git tag -a p03 -m "Practice 03: buffer and tokenizer"
git push origin p03

git checkout -b feature/final-integrate-p03-tokenizer
# Integrate p03 module into final_proyect
git add .
git commit -m "feat(final_proyect): integrate p03 tokenizer module into final project"
git push -u origin feature/final-integrate-p03-tokenizer
```

### Deliverables Convention

For each practice folder (`p1` to `p11`), the repository should contain:

- Source code with incremental commit history.
- A short technical PDF report in a project-specific path (for example: `pXX/docs/report/pXX.pdf`) including: activity summary, problems log, and technical solutions.

For `final_proyect`, the repository should contain:

- The integrated source code that consolidates modules from practices.
- Final project documentation and reports in `final_proyect/docs/report/`.

### Unix I/O Contract (CLI Behavior)

All command-line programs across practices and the final project must follow standard Unix conventions:

- Read input from `stdin`
- Print results to `stdout`
- Print errors to `stderr`


### Implementation Language
All practice projects and the final project are implemented in C.

## Language Policy
All repository content (documentation, comments, commit messages, issues) will be written in technical/academic English.

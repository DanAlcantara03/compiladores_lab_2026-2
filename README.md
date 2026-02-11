# Compiladores Lab Project

## Overview
This repository contains the base structure for a C development project used throughout the semester labs.

## Project Structure
```
compiladores_lab_2026-2/
├─ README.md
├─ Makefile
├─ .gitignore
├─ include/            # Public headers (.h)
│  ├─ example.h
│  └─ utils_example.h
├─ src/                # Source code (.c)
│  ├─ main_example.c
│  ├─ utils_example.c
│  └─ modulo_x_example.c
├─ tests/              # Tests for the source code
│  ├─ test_x_example.c
├─ build/              # Generated Fieles (objects, binaies) (don't be included in the github)
│  ├─ obj/
│  └─ bin/
├─ lib/                # Outer or own libraries (opcional)
└─ docs/               # Documentacion (opcional)
   ├─ reports/         # Reports for each practice.
```

## Build & Run
Requirements: GCC (or Clang) + Make

```bash
make
make run
```

## Branch Strategy (Branching Model)

We will follow a lightweight GitFlow approach:

- `main`: stable, production-ready code. Only merged via Pull Requests.
- `develop`: integration branch for ongoing work during the semester.
- `feature/<short-description>`: new features developed from `develop`.
- `bugfix/<short-description>`: non-urgent fixes developed from `develop`.
- `hotfix/<short-description>`: urgent fixes branched from `main` and merged back into both `main` and `develop`.

## Course Methodology: Practices + Semester Project

This repository supports two parallel tracks throughout the semester:

1) **11 Focused Practices**: short, incremental implementations that build individual compiler modules.
2) **Semester Project**: an integrated, continuously evolving compiler (“Design your language”) assembled from the practices.

### How Practices Feed the Project
Each practice is treated as a building block that is merged into the main codebase, so the semester project grows week by week.
By the end of the course, the integrated compiler should be functional and ready for a final practical evaluation.

### Git Workflow per Practice
For each practice `pXX`:

1. Create a short-lived branch from `develop`:
   - `feature/pXX-<topic>` for new work
   - `bugfix/pXX-<topic>` for fixes

2. Commit small changes with clear messages.

3. Open a Pull Request into `develop` and merge only when:
   - The project builds successfully
   - The practice output/behavior is correct

4. Tag the merge commit to freeze the delivery state:
   - `p01`, `p02`, ..., `p11`

Example:
```bash
git checkout develop
git pull
git checkout -b feature/p03-buffer-tokenizer

# work...

git add .
git commit -m "Implement buffered input and tokenizer"
git push -u origin feature/p03-buffer-tokenizer

# Open PR -> develop (GitHub/GitLab)
# Merge PR

git checkout develop
git pull
git tag -a p03 -m "Practice 03: buffer and tokenizer"
git push origin p03
```

### Deliverables Convention

For each practice, the repository should contain:

- Source code with incremental commit history.
- A short technical PDF report (in the path: `docs/reports/pXX.pdf`)  
    including: activity summary, problems log, and technical solutions.
    

### Unix I/O Contract (CLI Behavior)

All command-line programs in this repository must follow standard Unix conventions:

- Read input from `stdin`
- Print results to `stdout`
- Print errors to `stderr`


### Implementation Language
All practices and the semester project are implemented in C.

## Language Policy
All repository content (documentation, comments, commit messages, issues) will be written in technical/academic English.

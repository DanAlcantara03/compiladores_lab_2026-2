# Compiladores Lab Project

## Overview
This repository contains the base structure for a C development project used throughout the semester labs.

## Project Structure
- `src/`     : source code (.c)
- `include/` : headers (.h)
- `tests/`   : unit/integration tests
- `docs/`    : documentation
- `build/`   : build artifacts (ignored)

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

### Workflow Rules
1. Create an Issue (optional but recommended).
2. Create a branch from `develop`:
   - `feature/...` or `bugfix/...`
3. Commit with clear messages in English.
4. Open a Pull Request into `develop`.
5. Merge to `main` only for lab milestones/releases (tagged versions).

## Language Policy
All repository content (documentation, comments, commit messages, issues) will be written in technical/academic English.

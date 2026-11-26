---
name: build
description: >-
  Compiles the project, does the tests, and does static analysis in the
  development environment, then writes a short summary of the result. Use this
  agent for all build, test, or lint operations to keep long output out of the
  primary context.
---

You are the build agent for this project.

## Instructions

1. Start `scripts/build-agent.sh` from the project root.
2. If the script exits with 0, write one line with `BUILD: PASS` and the test
   counts.
3. If the exit code is not 0, write `BUILD: FAIL`. Give the step with the error
   and the possible cause. Include the first related error lines with their file
   paths and line numbers.
4. Do not change project files. Only find the cause and give a report.

For the commands and source layout, refer to the [cmake
skill](../skills/cmake/SKILL.md). For the environment, refer to the
[dev-environment skill](../skills/dev-environment/SKILL.md).

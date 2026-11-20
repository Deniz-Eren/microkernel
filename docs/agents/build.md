---
name: build
description: Builds the project, runs the tests and runs static analysis in the dev environment, then reports a concise summary of the result. Use for all build, test or lint operations so that verbose output stays out of the primary context.
---

You are the build agent for this project.

## Instructions

1. Run `scripts/build-agent.sh` from the project root.
2. If it exits with 0, reply with one line: `BUILD: PASS` followed by the test counts.
3. If the exit code is not 0, reply with `BUILD: FAIL`. Then give the step with the error, the first related error lines with their file paths and line numbers, and the possible cause.
4. Do not change project files. Only find the cause and give a report.

For the commands and the source layout, see the [cmake skill](../skills/cmake/SKILL.md). For the environment, see the [dev-environment skill](../skills/dev-environment/SKILL.md).

---
name: create-skill
description: >-
  Make a new agent skill in this project that obeys the Agent Skills
  specification. Use this skill when you add a new tool, workflow, or task to
  docs/skills/.
---

# Make a Skill

## Before you start

1. Examine `docs/skills/`. Make sure that no active skill has the same task.
2. If the new skill shares more than 30 percent of its instructions with an
   active skill, extend that skill.

## Steps

1. **Name the skill:** Use 1 to 64 characters with lowercase letters, digits,
   and hyphens. Do not put two hyphens together, and do not put a hyphen at the
   start or end. The directory name must be the same as the skill name.
2. **Write the description:** Use 1 to 1024 characters. State what the skill
   does **and** when to use it, because agents read this description to know
   when to load the skill.
3. **Make `docs/skills/<name>/SKILL.md`:**

   ```markdown
   ---
   name: <name>
   description: <what it does>. Use when <trigger>.
   ---

   # <Title>

   <Short instructions for an agent. Keep the body less than 500 lines.>
   ```

4. **Add other files if they are necessary:** Add `scripts/` (executables),
   `references/` (more documents), and `assets/` (templates). Connect to these
   files with relative paths from the skill directory. Keep files in
   `references/` one level deep.
5. **Do a check of the skill:** Make sure that the name is the same as the
   directory name and the frontmatter is correct YAML. Make sure that the
   description obeys the length limit. If `skills-ref` is installed, start
   `skills-ref validate docs/skills/<name>`.
6. **Add the skill to the project:** Add the skill to
   `docs/00-overview/context.md` and to the skill table in `README.md`. Add an
   entry to `CHANGELOG.md` in `## [Unreleased]`. Then make a commit (refer to
   the [git skill](../git/SKILL.md)).

## Specification

- Agent Skills specification: [references/SKILLS.mdx](references/SKILLS.mdx)

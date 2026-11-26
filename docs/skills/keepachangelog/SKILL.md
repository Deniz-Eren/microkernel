---
name: keepachangelog
description: >-
  Keep CHANGELOG.md in the Keep a Changelog format. Use this skill when you
  record a change, make a release, or change the changelog.
license: MIT
---

# Keep a Changelog

The file `CHANGELOG.md` at the project root records the important changes of the
project for its users. Its format obeys [Keep a
Changelog](https://keepachangelog.com).

## Structure

- The file starts with the sections `## Purpose`, `## Structure`, and `##
  Formatting Rules`. The release sections come after them.
- Use one section for each version: `## [VERSION] - YYYY-MM-DD`. Put the newest
  version first.
- Keep an `## [Unreleased]` section at the top. This section contains changes
  that have no release.
- Use level-3 category titles in each section: `### Added`, `### Changed`, `###
  Deprecated`, `### Removed`, `### Fixed`, and `### Security`. Put a title into
  the file only when that section has content.
- Put the entries in each category newest first (git history sequence).

## Procedure

1. Put each important change into `## [Unreleased]`, at the top of the correct
   category. Use one item for each change. Write for project users, not for git.
2. When you make a release, change the title `## [Unreleased]` to `## [VERSION]
   - YYYY-MM-DD`. `VERSION` is the new project version. Then start a new empty
   `## [Unreleased]` section above it.
3. Obey [Semantic Versioning](https://semver.org) for version numbers. If the
   project has a build configuration, make sure that its version is the same as
   the version in the new title.
4. Do not make a copy of the git commit history into the file. One item can
   group more than one commit.

## Attribution

This skill writes the format of keepachangelog.com without copied text. When you
put a copy of this skill into a target project, add this entry to Section 1 of
`docs/THIRD-PARTY-NOTICES.md` of the target:

```markdown
### keepachangelog.com
* **Component(s) Used:**
  [skills/keepachangelog/SKILL.md](skills/keepachangelog/SKILL.md)
* **Source:** [keepachangelog.com](https://keepachangelog.com)
* **Note:** The skill describes the Keep a Changelog format with its own words.
  No text is copied from the source.
```

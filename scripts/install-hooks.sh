#!/usr/bin/env bash
##
# @file     install-hooks.sh
# @brief    Install local git pre-commit hook enforcing format, text, and STE
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

set -euo pipefail
cd "$(dirname "$0")/.."

HOOK=.git/hooks/pre-commit
cat <<'EOF' > "$HOOK"
#!/usr/bin/env bash
set -euo pipefail
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"

DEFAULT_TRACE_DIR="$HOME/.gemini/skills/docs-traceability-setup"
SKILL_DIR="${DOCS_TRACEABILITY_SKILL:-$DEFAULT_TRACE_DIR}"
CHECK_TEXT="$SKILL_DIR/scripts/check_text.py"
if [ ! -f "$CHECK_TEXT" ]; then
    echo "pre-commit: missing check_text.py at $CHECK_TEXT" >&2
    exit 1
fi

./scripts/formatter-agent.sh
python3 "$CHECK_TEXT" --root .
python3 tools/check_ste.py --root .
EOF
chmod +x "$HOOK"
echo "Installed $HOOK"

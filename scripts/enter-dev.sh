#!/usr/bin/env bash
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#
# Opens an interactive shell in the development container. Builds the image
# first if needed. Delegates to scripts/env.sh (see docs/DECISIONS.md ADR-4).
set -euo pipefail
cd "$(dirname "$0")/.."
exec scripts/env.sh shell

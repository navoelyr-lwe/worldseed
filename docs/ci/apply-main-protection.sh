#!/usr/bin/env bash
# Apply the main branch ruleset. Requires a token with repository administration
# write (the default OpenHands integration token returns 403 here).
#
#   GH_TOKEN=<admin token> docs/ci/apply-main-protection.sh navoelyr-lwe/worldseed
set -euo pipefail
REPO="${1:-navoelyr-lwe/worldseed}"
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
gh api -X POST "repos/${REPO}/rulesets" --input "${DIR}/main-ruleset.json"
echo "applied ruleset 'main-protection' to ${REPO} (branch: main)"

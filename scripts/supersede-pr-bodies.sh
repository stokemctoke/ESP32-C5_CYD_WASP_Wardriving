#!/bin/bash
# Generates superseded notice + original body for each fix PR.
# Used with ManagePullRequest tool; gh comment API lacks permission.

BANNER='> **Superseded by integration PR [#85](https://github.com/stokemctoke/ESP32-C5_CYD_WASP_Wardriving/pull/85)** — please review the combined branch `cursor/pre-release-integration-f920` instead of merging this individually. This draft can be closed after #85 lands. Conflict resolutions are documented in `MERGE_PLAN.md` on that branch.

---

'

REPO="stokemctoke/ESP32-C5_CYD_WASP_Wardriving"

for pr in $(seq 44 84); do
  body=$(gh pr view "$pr" --repo "$REPO" --json body --jq -r .body 2>/dev/null)
  if [ -z "$body" ]; then continue; fi
  # Skip if already superseded
  if echo "$body" | grep -q "Superseded by integration PR"; then
    echo "SKIP $pr (already marked)"
    continue
  fi
  # Strip cursor agent footer links for cleaner body
  clean=$(echo "$body" | sed '/^<div>/,$d' | sed '/^<!-- CURSOR_AGENT_PR_BODY_END -->/,$d')
  echo "=== PR $pr ==="
  echo "${BANNER}${clean}"
  echo ""
done

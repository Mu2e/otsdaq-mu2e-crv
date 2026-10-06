# Staged issues — otsdaq-mu2e-crv

Findings from the September 2026 DAQ mega-review (`mu2e-sept-mega-review`,
2026-10-06), staged here because GitHub writes are denied to review agents in
this session (CONTRACT.md §9.1). Bodies are in the CONTRACT.md §6 format, with
fingerprints, ready to file unchanged.

`Mu2e/otsdaq-mu2e-crv` is a usable remote: public, issues enabled, default
branch `develop`. **Dedup:** `gh issue list --state all --limit 300` returned
zero issues on 2026-10-06, so nothing here duplicates an existing issue.

**Fingerprints** are `sha1("Mu2e/otsdaq-mu2e-crv:<path>:<rule_id>")[:8]`.

**Filing:**

```bash
REPO=Mu2e/otsdaq-mu2e-crv
gh label create mega-review -R "$REPO" -c 6F42C1 \
  -d "Filed by the Sept 2026 DAQ mega-review" 2>/dev/null || true
# then one gh issue create per section below, sleeping 4 s between calls
```

---

## CRV-001

**Title:** `[security] a CookieCode is hard-coded in UserWebGUI/js/mu2e.js`

**Severity:** S1 — security exposure; fix now
**Category:** bug
**Location:** `UserWebGUI/js/mu2e.js:21`
**Found by:** mega-review (`mu2e-sept-mega-review`), 2026-10-06

## What
`UserWebGUI/js/mu2e.js:21` assigns a 512-hex-character literal to the global
`CookieCode`. The page's helper functions send it as `CookieCode=<value>` in
the POST body of ten requests to the otsdaq configuration supervisor
(`LID_CONFIG = 281`, `mu2e.js:5`), five of which change the configuration
database. The page has no login or session-acquisition code: `mu2e.js`
contains no reference to a login, a session or `document.cookie`, so every
user of these pages authenticates as whoever's session the literal came from.

The value is deliberately not reproduced here; see the cited line.

## Evidence
```
$ git grep -n 'CookieCode' | sed -E 's/[0-9a-fA-F]{32,}/<REDACTED>/'
UserWebGUI/js/mu2e.js:21:CookieCode = "<REDACTED>"          # 512 hex characters
UserWebGUI/js/mu2e.js:484:  getSpecificTable              (read)
UserWebGUI/js/mu2e.js:497:  saveSpecificTable             (WRITE: saves a table version)
UserWebGUI/js/mu2e.js:507:  saveNewTableGroup             (WRITE: creates a table group)
UserWebGUI/js/mu2e.js:517:  activateTableGroup            (WRITE: activates a group, ignoreWarnings=1)
UserWebGUI/js/mu2e.js:526:  setGroupAliasInActiveBackbone (WRITE: moves a group alias)
UserWebGUI/js/mu2e.js:535:  getActiveTableGroups          (read)
UserWebGUI/js/mu2e.js:545:  getTableStructureStatusAsJSON (read; the source comment at :541 says it "resets the configuration")
UserWebGUI/js/mu2e.js:554:  getActiveTables               (read)
UserWebGUI/js/mu2e.js:563:  getSpecificTableGroup         (read)
UserWebGUI/js/mu2e.js:572:  clearTableTemporaryVersions   (WRITE: discards temporary table versions)
```
(The annotations replace the source text after the line number; every one of
these lines is `data = "CookieCode=" + CookieCode`.)

The writes are reachable from the shipped pages, not only from a console:

- `saveConfig()` (`mu2e.js:583-654`) chains `saveSpecificTable` for every
  temporary table (`:598`), `saveNewTableGroup` (`:630`) and
  `activateTableGroup` (`:644`), then calls `updateBackbone()` (`:647`,
  defined `:656-672`), which runs `setGroupAliasInActiveBackbone` (`:657`)
  and saves and activates a new backbone group (`:661`, `:663`). The
  `saveConfig` button is generated at `mu2e.js:1497`; save buttons also
  appear on `Mu2eCrvConfig.html:77` and `Mu2eDTC.html:205`, and
  `Mu2eIndex.html:18` looks the button up by id.
- `activateTableGroup` is wired to click handlers at `mu2e.js:2138`, `:2147`,
  `Mu2eCrvConfig.html:36` and `Mu2eRunLog.html:25`.
- `Mu2eStatus.html:50-60` exposes one-click test links for
  `saveSpecificTable`, `saveNewTableGroup`, `activateTableGroup('CrvVSTConfig',337)`,
  `setGroupAliasInActiveBackbone(...)` and `saveConfig()`.

History, checked without printing the value (`git log -- UserWebGUI/js/mu2e.js`
and a SHA-256 of the literal at each revision):

```
384409a 2024-12-05 Simon Corrodi    first commit containing the literal
a5cbc23 2025-12-17                  same value
e4b4695 2026-01-23 Eric Flumerfelt  whitespace-only change to the line; same value
354937a 2026-02-17                  same value (current HEAD of develop)
```

## Why it matters
Static or stale? Nothing in this repository shows the code has expired.
Session issuance and expiry are implemented in otsdaq core, which is not in this
tree, so this review cannot say whether otsdaq still accepts it. What the
repository does show:

- one value, unchanged for 22 months across four commits — the pages have
  been used throughout that period with no mechanism to obtain any other
  code, which is consistent with a code that keeps working;
- the comment `// TODO, add cookie code!` above `saveSpecificTable`
  (`mu2e.js:492`) — the author knew session handling was unfinished;
- `CrvVst.html:363`, in the same directory, sends `activateTableGroup`
  through `DesktopContent.XMLHttpRequest` (the otsdaq desktop request helper,
  defined in otsdaq core) rather than the literal — so the normal path exists
  and these pages bypass it.

Severity reasoning: the literal is used exclusively as the credential for
configuration writes and activations on LID 281, the
`mu2edaq-commandline-tools` review independently concluded it grants
configuration writes, and nothing shows it has expired. That is a credential
granting write access to the DAQ configuration database, in a public
repository — S1 under the rubric ("a credential committed to git history"). If
the otsdaq admin confirms it is dead, the residual issue drops to S3 (the
pattern, and pages that authenticate as one fixed identity).

`Mu2e/otsdaq-mu2e-crv` is public and the literal has been in its history
since 2024-12-05, so filing this issue discloses nothing new.

Observation, not part of the fix: `setTreeNodeFieldValues` (`mu2e.js:463-470`),
also a write, sends no `CookieCode` at all; whether otsdaq accepts it without
one depends on the instance's security mode, which is outside this tree.

## Suggested fix
1. otsdaq admin: invalidate this code (and any session it belongs to) on every
   otsdaq instance that may have issued it — this is the only step that
   closes the exposure, because the value stays in git history.
2. Remove the literal from `mu2e.js:21`.
3. Have the pages obtain the session code at login the way other otsdaq
   desktop pages do (e.g. route requests through `DesktopContent.XMLHttpRequest`,
   as `CrvVst.html:363` already does), so each user acts under their own
   session and permissions.
4. Optionally add a secret-scanning check (e.g. a pre-commit hook or the
   `Mu2e/.github` whitespace workflow's sibling) for long hex literals named
   `CookieCode`.

## Related
- `mu2edaq-commandline-tools/doc/otsdaq-command-surface.md` §5.3 (independent
  finding; same line)
- Mu2e/mu2edaq-commandline-tools CLT-010 (CookieCode handling in the CLI)

<!-- mega-review-fingerprint: dd1f1fc4 -->

# Companion Hardening: FUBAR 1.1.51

Status: source-qualified, awaiting FUBAR Actions/public qualification. Parent decision DEC-0223; Town system/UX audit
F06/F07. Town owns native transport, snapshot cache and receiver lifecycle.
FUBAR owns bounded admission, bridge validation and production-browser consumption.
The parent authorized scoped commit/push and FUBAR publication after Town's public
release verification. No public-service configuration, RF or DSP changes are made.
Latest promotion remains a separate gate requiring explicit parent approval.
Historical evidence in ALIGNMENT_20261011.md is not rewritten.

## Control Admission

- FUBAR issues cryptographic 128-bit bearer tickets, stored in browser-tab
  sessionStorage. Caller-chosen clientId values do not grant authority. Tickets
  are stripped before forwarding to Town and never included in public status.
- At most 32 waiting tickets and eight displayed queue names. Anonymous requests
  share a 128-token burst/16-per-second budget; admission uses 33/1-per-second.
  Each issued ticket has a 16-token burst/4-per-second budget. Anonymous floods
  do not consume the active owner's budget. Refusals use 429 and Retry-After.
- Five-minute ownership, at most two three-minute extensions, stale waiting
  expiry after 90 seconds, explicit release and FIFO promotion remain. Disable
  or restart invalidates tickets. Expired mutations are not retried automatically.
- This is bounded anonymous admission, not an account or anti-Sybil system. One
  actor can acquire multiple tickets within the budgets. IP is not human identity.

## Native Paging Contract

Existing primary map/message routes and named session GET views are retained.
FUBAR asks for offset=0, then sends the same route and sessionId/view with
snapshot=<opaque>&offset=<nextOffset>. Small unpaged responses remain supported.

```json
{"ok":true,"page":{"snapshot":"opaque-id","offset":0,"nextOffset":64,"total":512},"tracks":[]}
```

- page.offset/nextOffset/total are JSON integers; nextOffset is null on the last
  page. Snapshot identifiers use 1-96 ASCII letters, digits, hyphens or underscores.
- aircraft, positions, tracks and messages share the same offset. total is the
  maximum array length, not their sum. Each array has at most 64 rows per page.
  Metadata is included on page zero only. Adaptive pages may have fewer rows;
  nextOffset equals offset plus the largest array slice length.
- Town snapshots are immutable for 30 seconds, bound to the route and query
  including sessionId/view, with an 8 MiB total cache and <=192 KiB page budget.
  Query numeric values arrive as strings in Town and must be parsed accordingly.
- FUBAR validates continuity/identity, total <=4096, at most 64 pages, a 20-second
  overall fetch deadline and at most 4,194,304 serialized JavaScript characters.
  The existing 256 KiB native/DLL transport ceiling is not raised. Failed or
  obsolete continuations never replace a complete map or cross session views.
- Aircraft session catalogues need only session metadata; nested track arrays
  belong to the named GET view. Oversized legacy catalogues fail boundedly.
- Map mutations return compact {"ok":true,"status":200,"stateOmitted":true} in
  the current Town source; the earlier proposed {"ok":true,"refreshRequired":true}
  shape is also supported. FUBAR performs one mutation followed by a paged readback,
  without requiring state in the acknowledgment. This also applies to named
  aircraft sessions. Legacy 413/oversize failures retain a visible error and
  still read back; the mutation is never repeated to obtain a response body.

## Response And UI Boundaries

Bridge success requires DLL result zero and parsed top-level boolean ok=true.
Missing, nested-only, numeric, string, null, array or object ok is not success.
Invalid, truncated or oversized responses produce short structured errors, not
raw prefixes. Recognized native errorCode/code values survive: response_too_large,
incomplete_response, invalid_json, response_truncated and snapshot_expired.
Native status-409 snapshot errors without a code remain failures. Short actionable
refusals are retained only without embedded JSON, local Windows paths or tokens.

The browser separates immediate mode switching from the Mode on Tune draft;
session lists replace free-text session IDs, with explicit New session actions.
Unavailable selections do not fall back to a different receiver. Narrow screens
use a task selector. Polls and late ticket responses do not overwrite newer drafts
or admission state. Rejection styling follows failure, not error-message wording.

## Tests And Evidence

Local MSVC Release build, main CTest 4/4, standalone production-object tests 2/2
and extracted production JavaScript pass. The standalone suites test 128 supplied
IDs, 80 concurrent requests, admission churn, queue/status bounds, leases and two
extensions, expiry/promotion, permission checks and ticket privacy. A legacy DLL
fixture deliberately returns zero for malformed ok values to verify strict parsing.

The real DLL loopback suite exercises fragmented 90 KiB responses, 512 Unicode
rows, primary/named pagination, invalid queries, malformed/truncated/oversized
responses, native errorCode/code, metadata-only and oversized nested catalogues,
96 admission IDs, rate limiting, lease ownership, authentication and privacy.
Its browser run uses the actual embedded page at 360x844, 390x844 and 1280x900.
It checks all eight tasks, overflow/page errors, draft retention, session selection,
ticket reload/release, malformed upstream, one-shot large POST/413 and compact
primary/named acknowledgment followed by paged readback.

The complete 1.1.51 loopback/browser matrix passed and FUBAR exited zero:
`D:\FUBAR-qa\20261015-v1.1.51-hardening\result.json`. Browser log, six task
screenshots, the bounded map-error screenshot and sanitized process log are in
that directory. Workflow YAML parses locally; the pinned Playwright version is
available from npm. Hosted FUBAR Actions/public verification remain pending.

After the native transport freeze, the matrix was repeated with the exact
stateOmitted=true acknowledgment, for both primary and named aircraft sessions,
as well as the earlier refreshRequired=true shape. All fixtures/browser checks
passed and the FUBAR process exited zero:
`D:\FUBAR-qa\20261015-v1.1.51-state-omitted\result.json`. Release build, main
CTest 4/4 and production-object CTest 2/2 were repeated successfully. No FUBAR
implementation work remains for this contract. The final native pair is recorded
below; hosted CI and public asset verification remain pending.

Actual paired Town/DLL/FUBAR 180-second run completed with both exit codes zero:
`D:\FUBAR-qa\20261015-final-pair-180\result.json`. The production browser matrix
passed; no streaming device was observed. Exact process IDs and loopback ports,
sanitized logs, browser log and six viewport screenshots are retained alongside
the result. This pair preceded the final 1.1.51 version bump and compact-ack
browser readback adjustment; the subsequent run below covers the final binaries.

The earlier `D:\FUBAR-qa\20261015-pair-180` run passed functional/browser checks
but was interrupted at the parent's request to relink Town. Its forced exit codes
are not graceful-shutdown evidence. The completed run above supersedes it.

Final frozen-transport/common-cleanup Town 0.2.139 + FUBAR 1.1.51 pair:
`D:\FUBAR-qa\20261015-v1.1.51-town0.2.139-final-pair\result.json` is PASS.
The 180-second no-RF run started at 21:53:08 +11:00 on 2026-10-15; Town PID
51900 and FUBAR PID 13624 both exited normally with code zero. All eight browser
tasks at 360/390/1280, drafts, session lists, ticket reload/release, authenticated
requests, rejected invalid radio keys and session/privacy checks passed. Town
logged all streams stopped and normal application exit. Both exact PIDs were
confirmed absent; no other process was stopped and no source rebuild was needed.

Exact SHA256 values, checked before and after the run and unchanged:

- Town EXE: `7563fa005f0acbe9bda9dd9befe9f13886e96fb5e3a028fe7ed8ab6728da23b9`
- Control DLL: `df2a593aa8ee47889570ddd9184da984e6241f7375bf7bad54d049ec03520f14`
- FUBAR EXE: `8efc55ea2cb8780faeeacb44c82a0b046fa651d923284370045bb373d0a37bad`

provenance.json in that directory records absolute binary paths, hashes, PIDs,
ports, results and the physical-test limitation. browser.log, sanitized process
logs and viewport screenshots are retained. Large-map/compact-ack behavior is
qualified separately by the real-DLL loopback fixtures above, not by injecting
live aircraft data into the actual app.

Build/release CI now includes production-object suites, real-DLL failure/browser
fixtures, an explicit-auth 180-second browser pair, automatic pairing, and retained
sanitized diagnostics/screenshots. Local tests do not establish hosted CI success.

## Release Gates And Limits

- Town 0.2.139 public release Actions 38724109668 passed. The immutable ZIP pin is
  `77646cf86282cd00ee4af8634b90469bff607f51022a1b3ed58995e0109b8efd`, source
  `a45b2006db79a17ceff1966caca5131806b793f6`. Staging checks both values, embedded
  executable provenance and the copied DLL/runtime inventory. town-pair.json and
  FUBAR build-info.json carry the dependency provenance. The public Town DLL is
  `3cb79fc1d46f8119700b22a66ab4901705b6c39fe93e35a86605cb8c642013ba`.
- D:-only staging passed under `D:\FUBAR-ReleaseQA\1.1.51\stage-precommit`,
  including DLL loading and provenance checks. A simulated changed ZIP digest
  was rejected before extraction. The public Town DLL loopback/browser matrix
  passed with exit zero under `D:\FUBAR-ReleaseQA\1.1.51\public-town-hardening`.
- The final native build and actual no-RF pair passed locally. Hosted CI and
  source-matched public package verification are still required for publication.
- External browser requests (map tiles, CDNs, directory) are blocked in fixtures.
  This is not a map-tile rendering, public-service or internet-source acceptance.
- No new RF decode accuracy, physical SDR/rotor behavior, live audio listening,
  device contention or independent per-session audio buses are claimed.
- The native FUBAR GUI has no preview/no-start mode; direct same-profile launch
  can capture, prune, migrate files and persist settings. It was not launched for
  this pass. No new GUI preview feature is included in the frozen source.
- The requested physical keyed 98.1 MHz RTL smoke was not run: test_radio_live.py
  explicitly persists the listening selection and has no restore-all or isolated
  settings option. It does not meet the user's no-permanent-settings-change
  condition. No physical tune or settings-restoration claim is made.

## Reproduce Locally

Changed files: src/web_server.cpp/.h (including the production page),
src/sdr_town_bridge.cpp; scripts/test_web_ui.cjs, test_web_browser.cjs,
test_control_loopback.py, test_town_pair.py and test_radio_live.py; the standalone
scripts/control-tests/CMakeLists.txt, test_control_admission.cpp,
test_bridge_response.cpp and legacy_bridge_fixture.cpp. Version-only edits are in
CMakeLists.txt, src/main.cpp, src/app_window.cpp and scripts/publish-github.ps1.
Release notes/qualification are in README.md, CHANGELOG.md, UI_QUALIFICATION.md
and this document; both windows-build.yml and release.yml integrate the suites.
No Town files or unrelated untracked third_party contents were changed.

```powershell
cmake --build build-fubar-msvc --config Release --parallel 2
ctest --test-dir build-fubar-msvc -C Release --output-on-failure
cmake -S scripts/control-tests -B build-fubar-msvc/control-tests -A x64
cmake --build build-fubar-msvc/control-tests --config Release --parallel 2
ctest --test-dir build-fubar-msvc/control-tests -C Release --output-on-failure
node scripts/test_web_ui.cjs
```

Run test_control_loopback.py with --fubar, --dll, --browser and --output; run
test_town_pair.py with the same arguments plus --town and --duration 180. Browser
tests require Node 20+, Playwright 1.62.1 and Microsoft Edge; set NODE_PATH to the
installed Playwright node_modules. Use a dedicated loopback fixture, never an
operator's running station. Each harness cleans up only its own process handles.

# Paired workspace usability qualification

## 1.1.52 Public Qualification (2026-10-15)

Published source `2b419ec61c538b6693aa11b6efd178dff14df5f9` passed release
Actions 38733569638 and Windows build runs 38733568815/38733569167. The public
ZIP, build-info/town-pair provenance, EXE, DLL, runtime inventory and all 84
corresponding-source files were verified against the tag's Windows Git export.

The actual downloaded FUBAR 1.1.52 and public Town 0.2.139 passed the 180-second
production-browser pair (Town PID 51676, FUBAR PID 51180), 60-second automatic
pairing (Town PID 25784, FUBAR PID 52752), and real-DLL malformed-upstream/browser
matrix (FUBAR PID 51576). Every process exited normally with code zero; all five
exact PIDs were confirmed absent and hashes were unchanged after testing.

Combined proof, exact hashes/paths, individual result files, sanitized logs and
screenshots: `D:\FUBAR-ReleaseQA\1.1.52\public-run38733569638\public-proof.json`.
See [public provenance and limits](PAIRED_HARDENING_20261015.md). Published assets
and tags remain immutable; this evidence update is docs-only on main. After proof,
the parent promoted FUBAR 1.1.52 and Town 0.2.139 to Latest; anonymous API checks
confirmed both. Native FUBAR GUI, physical RF and external
map tiles/CDNs remain outside this qualification.

## 1.1.52 Request Freshness

Release run 38730716546 for 1.1.51 failed the malformed-upstream browser check
before publishing assets. A deterministic production-JavaScript regression
reproduced older same-session success overwriting a newer failure. Both Aircraft
and Inmarsat now reject obsolete request completions and continuation pages.
The full Inmarsat pipeline shares one generation across status, messages, map
and catch paths; deferred-function tests also cover A-B-A selection and obsolete
outer requests attempting to overwrite a newer map error.
Reverse success/failure order is tested with delayed real-DLL browser responses,
after completing and draining prior native requests. No sleeps are used to order
these regressions, and malformed-response assertions remain unchanged.
Tag v1.1.51 remains unchanged; 1.1.52 Actions/public-pair verification passed above.
Release/main tests 4/4 and production-object tests 2/2 pass. The final browser
failure matrix passed at `D:\FUBAR-ReleaseQA\1.1.52\final-hardening`; the actual
public Town + source-built 1.1.52 180-second browser pair passed at
`D:\FUBAR-ReleaseQA\1.1.52\source-pair-180`. Town PID 15516 and FUBAR PID 49284
both exited normally with code zero. No RF capture or operator GUI launch occurred.
The earlier local evidence below is retained, not relabeled as 1.1.52 evidence.

## 1.1.51 Local Qualification (2026-10-15)

DEC-0223 / audit F06-F07 companion scope: bounded admission and map transport,
session selection, draft/immediate-mode clarity and narrow-screen task access.
The final Town 0.2.139 executable/DLL and FUBAR 1.1.51 passed a 180-second authenticated no-RF
pair run, including the production browser's eight tasks at 360, 390 and 1280
pixels. Both applications exited normally with code 0. Browser checks cover
draft retention, session lists, ticket reload/release, layout and page errors.
Exact binary hashes, PIDs and exit results are retained under
`D:\FUBAR-qa\20261015-v1.1.51-town0.2.139-final-pair`. Both PIDs are confirmed
exited. The physical RTL tune was skipped because the existing script persists
settings and does not satisfy the requested restore-all condition.

See [paired hardening evidence and limits](PAIRED_HARDENING_20261015.md).
This is local source qualification, not FUBAR published-package qualification.
Town 0.2.139 is public and hash/source verified; FUBAR hosted CI and public-package
pairing remain release gates.
External map tiles/CDNs were blocked in the browser fixture; RF, audio listening,
hardware ownership contention and physical device behavior are not qualified here.

## Historical 1.1.47 Pairing Delta

The qualification target for this historical pass was FUBAR 1.1.47 with SDR Town
0.2.125-experimental. Automated coverage must include the named-session proxy
routes, AES-only Inmarsat map identity, heading-aware Leaflet markers and the
2.4 MS/s ADS-B default. The 1.1.44 / Town 0.2.120 evidence below remains a
historical release record and is not evidence for the current package.

2026-10-03, FUBAR 1.1.44 / SDR Town 0.2.120 (DEC-0166 / T-0097).

Scope: both native applications, local DLL/control bridge and browser UI.
Preserve audio engines, demodulators, server-side permissions, leases and consent.

Confirmed defects: mode dropdown overwritten by polling while lease held;
command errors hidden outside Radio; decoder buttons enabled for observers;
fixed empty audio player covering narrow screens; oversized native satellite
styling; main receiver crowded by inactive repeater; missing Inmarsat map
success flag rejected by the real control DLL. All have scoped repairs.

Automated tests: CTest cli_help/self_test plus test_web_ui.cjs (embedded JS syntax,
unique IDs/tab targets/label references, drafts, permissions, polling overlap,
offline and command-failure recovery). Town runs 16 suites including compact
workspace navigation and an empty-map web-report regression.

Manual browser checks: control lease, preserved WFM/frequency drafts across
polls/tab switches, website 98.1 MHz tune reflected by native Town. Desktop and
phone viewport layout, all workspace tabs, map display and disconnected states.
Native checks: both windows launched; workspace selector/panel navigation,
keyboard focus and app restart. Final execution results recorded with release.

Final local MSVC Release and CTest: Town 16/16, FUBAR 3/3 PASS. Actual empty-map
bridge returns ok=true; all eight web tabs fit 390x844 without page overflow.
Band plans now recover after starting the website before the receiver and keep
the selected channel on refresh. Added coverage for failed bootstrap/retry.
Final rebuild caught MSVC C2026 on the embedded HTML literal after this addition;
split the literal at a JavaScript declaration boundary (same page bytes).
The real recording player advanced through the existing 20-second clip with
no media error. Its pause/resume walkthrough exposed a source reassignment
that restarted paused clips; repaired and regression-tested. Playback list
labels now follow pause/end state and filenames are escaped before rendering.
Keyboard arrow navigation between web tabs and cross-tab validation errors
confirmed. Native pointer sizing was blocked by a separate Windows picker
overlay; do not claim physical short-height scrolling acceptance from that run.

Limits: no claim of new RF decoder accuracy, physical rotor/SDRplay acceptance,
or all-device auditory validation. Test machine currently routes Town to
speakers while FUBAR captures VB-CABLE; website audio requires the intended
output/input pairing. Privacy and hardware routing are not changed silently.

## Published evidence

FUBAR v1.1.44 source d9bcb0a0e819912db5be84a1f7d9ec46077d655c.
Main/tag builds 37081402863 / 37081405864 and release Actions 37081407222 PASS.
https://github.com/blkph0x/FUBAR/releases/tag/v1.1.44
Anonymous ZIP SHA256:
1a52483abacee08a8435072e4f2592c8d5495736113a9a8d7602281dd2bab6af
Public source/version/EXE/DLL hashes verified; shipped self-test PASS. Control
DLL equals Town 0.2.120-experimental (source 6dc66fa, release run 37079654265).
Browser resume confirmed at 9.65 seconds after pause at 9.57, without resetting.
Both native apps left open, browser in Radio with control lease released.
External community-directory GET timed out at 15 seconds; local website works.
Native HTTPS announcement failure requires separate server/network diagnosis,
not an unapproved configuration change during UI qualification.

# Paired workspace usability qualification

## 1.1.47 pairing delta

The current qualification target is FUBAR 1.1.47 with SDR Town
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

# Paired workflow alignment

Status: published as 1.1.49, paired with Town 0.2.137; both Latest.
Release source 2087a8a4b476001d4a3edb6f28780fd818e0a0c5, release Actions
38114493140 and Windows Actions 38114297986/38114488968 all PASS.
Public ZIP SHA256 f8a4a3a16e0f12cfbffd01739d7053d70e8d9b4477f6cbf44e1792601046c166.
Downloaded executable self-test, startup, real-DLL 90 KiB fixture and the full
public-to-public Town/DLL/FUBAR session test pass; both processes exit 0.
DLL SHA256 a3166ce8695e6326be248944d9e6d9ae290f60a3152be2f470245f4234125d20
matches Town's published module exactly. Source/notices/rebuild kit included.

### Failure investigation history

Clean-runner paired startup failed before v1.1.48 publication (Actions
38112905743). DLL loading and native/JS tests passed. Retain sanitized process
logs and exit codes to identify the startup failure; no gate is waived and
there is no published 1.1.48 package. Public Town 0.2.137 pairs locally.
Symbolized clean-runner dump 38113376195 confirms ACCESS_VIOLATION in
ComPtr<IMMDeviceCollection>::reset, called after CoUninitialize in
AudioEngine::enumerateInputDevices. Use a noncopyable apartment scope created
before interface owners, also in captureThread (same early-return defect).
Keep real enumeration in web-only startup; add repeated enumeration/no-capture
process tests so an audio-less runner proves the repair rather than bypassing it.
Local repaired Release build and 4/4 CTest pass (1.21 s). Initial new fixture
used port 0, correctly rejected by the CLI; it now reserves a valid free port.
Release 1.1.49 supersedes the failed, unpublished 1.1.48 tag without rewriting it.
After repair, clean-runner 38113899752 proves a second failure: the 2.5 s bridge
deadline expired on cold satellite session construction, but a direct native
GET 1.063 s later confirmed pair_a existed. Generic workspace calls now have
a bounded 10 s budget and content-free duration/error diagnostics. No automatic
mutation retries; ordinary health/status and dedicated tuning/P25 budgets stay.
The real-DLL fragmented 90 KiB payload fixture also reproduced a 64 KiB caller
buffer truncation: public JSON became "Invalid SDR Town response". Workspace
requests now allocate the DLL's existing 256 KiB budget on the heap. The DLL's
overall bounded transport limit remains; oversized maps still require future
native pagination, not silently removing limits or copying unbounded data.
Clean-runner 38114095528 confirms cold session open succeeds with the measured
budget (2535 ms). It then exposed a fixture assumption: native dry-run correctly
returns success for a resolvable placeholder, unlike this host's stale saved
Listen identity. Match startGuiRuntimeDeviceAt's explicit dry-run contract and
assert exact forwarded settings plus no streaming on every enumerated device.
Town decision DEC-0219 governs this pass. Accepted P25 DSP is unchanged here.

Confirmed defects: 0.2.125 DLL pin, hardcoded ISS downlink, named Inmarsat
messages/map falling back to primary, file-only image formats offered live,
public image route permitting unrelated profile images, unrestricted local
outputDirectory in forwarded JSON. Use pinned MIT nlohmann/json for structured
validation and a bounded reported-image registry; preserve its notice.

Remote scope: receive tuning/mode/filter/volume/gain, P25 monitoring, decoder
status, independent SSTV/Inmarsat/satellite/aircraft sessions, satellite profiles
and pass automation, local/network aircraft source controls, decoded images,
messages, tracking sources and ages. Native engines remain authoritative.

Local-only: observer/home coordinates, hardware installation, rotor movement,
transmit arming, arbitrary local files/IQ replay, diagnostics/recording consent,
account credentials and release/signing configuration. These are deliberate
host-admin boundaries, not silently advertised web controls.

Existing audio transport is the selected Windows capture device. Multiple RF
sessions do not imply independently selectable network audio streams. That
larger routing feature is not claimed by this companion alignment release.

Required evidence: native core/lifecycle/session tests, FUBAR self-tests and
production-JS tests, actual DLL/HTTP pairing, browser desktop/mobile checks,
Actions success, public download hashes/provenance and shipped executable tests.

## Verified local coverage

| Workflow | Companion controls/data | Evidence / boundary |
|---|---|---|
| Listen | Frequency, analog modes, BW, LPF, volume, gain, squelch | Shared native tune; settings forwarding tested without RF |
| P25 | Monitor CC, auto follow, known channels, current TG/alpha tag | Existing native path retained; no DSP modifications |
| SDRplay / RTL | Model-supported controls and direct sampling | Existing capability gates retained; Bias-T for RTL remains host-only |
| SSTV | Auto + 49 live analog modes, RF mode/frequency/device, session, receive/finish/cancel/images | Native directory; no browser file access |
| Satellite | Named sessions, radio, scan/record/stop, catalogue, chosen downlink, Doppler/auto-capture | Exact native planner/status; no home-coordinate edits |
| Inmarsat | Named sessions, radio/channel, watch rows/rates/budget, messages/map | Missing session fails; old response discarded after switching |
| Aircraft | Named sessions, device, capture rate, local/network source controls, map | No fallback to primary; native track/heading/source |
| RDS / tones | Existing read-only decoder status | Native decoder remains authoritative |

Web actions retain host admin permissions and timed control leases. The existing
explicit primary Radio tune/mode operation can leave a P25 follow; it still must
pass native physical-device ownership checks. Satellite/aircraft/Inmarsat actions
never force a different workflow's radio. Arbitrary force requests are rejected.

Local evidence: FUBAR 3/3 CTest, production JS error/draft/permission/late-map/
pure-stop/failed-configure tests, C++ private image and recursive path tests;
real paired token/DLL/HTTP testing and graceful exits. Browser 1280x900 and
390x844: no overflowing Inmarsat controls, correct selected session, no console
errors, readable native RF refusal and preserved frequency edits.
One intermediate link failed because the QA executable was still running;
normal QA exit and relink resolved it, with no source workaround.

Remaining native-only parity: RTL Bias-T, Auto-BW policy, new saved-frequency or
alias-file imports, CW/DTMF decoder configuration, FFT display preferences,
IQ replay/file selection, STT setup, diagnostics/recording consent, device
installation, TX, rotor actuation and station frontend commissioning. These are
not advertised as implemented web features. Multiple RF sessions still share
the configured FUBAR capture input; independent audio-bus selection is future work.

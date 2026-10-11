# Paired workflow alignment

Status: local implementation/verification complete; public package checks pending.
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

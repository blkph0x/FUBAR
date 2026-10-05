# SDR Town workspace alignment

Task T-0098, 2026-10-06. Baseline FUBAR 1.1.46 / Town 0.2.125-experimental.

This pass closes the release-pairing drift present in the previous 1.1.44
package. FUBAR now consumes Town's named Inmarsat/SSTV/Satcom/Aircraft session
routes in addition to the primary workspace, preserves AES identity when ICAO
is not yet available, renders ground-track arrows in both aircraft maps, and
requests the same 2.4 MS/s default used by Town's local 1090 workflow. The
bridge rejects arbitrary paths and emits only endpoint/result/size diagnostics
to the debugger; request bodies and radio data are never logged.

Completed in this pass: versioned workspace capabilities, shared Inmarsat hybrid map,
aircraft RF/network controls, Inmarsat watch/device selection, SSTV RF mode,
permission enforcement, private leases, bounded structured responses and browser
rendering. Native DSP, codecs, FUBAR audio capture and streaming remain unchanged.

Town owns received identities, positions, freshness, lookup consent, device
leases and radio state. FUBAR forwards only explicitly supported routes and
enforces admin permissions plus current browser lease. Website users cannot
enable operator internet consent, set home coordinates, read arbitrary files,
upload IQ automatically, or alter diagnostics consent. No public generic proxy.

Add nlohmann/json, MIT, pinned to Town's existing version, for structured boundary
validation and sanitization. Replace fixed response truncation with a bounded
large read; never retry a mutation to resize its response. Instrument request
failures without recording credentials or radio message contents.

Required gates: C++/CLI tests; hostile/malformed/permission/lease tests; real
Town-DLL-FUBAR loopback smoke; browser desktop/mobile source/expiry/escaping tests;
Town GUI/CLI IQ map and unchanged 8400 audio; CI-built matched public packages.
This document will record the completed feature matrix and remaining limits.

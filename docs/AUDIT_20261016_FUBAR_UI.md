# FUBAR Human Workflow UI Audit

Date: 2026-10-16. Source baseline: `12b46488ab4ce9249dbcaaedcbe8c8919c8f45f7`, FUBAR 1.1.52.

Scope: first-time native and web workflows, terminology, navigation, busy/error feedback, session control, keyboard use and accessibility. This report proposes behavior only; no implementation, release or policy change is authorized by a finding.

## Evidence and Boundaries

- **Observed:** The released FUBAR website was exercised through the packaged DLL and an isolated actual Town GUI; desktop/mobile task views, validation, control access, keyboard navigation and disconnected state were inspected.
- **Source-confirmed:** Explicit behavior in the production source at the baseline above. User consequences and unmeasured timing effects are identified as inferences.
- **Not exercised:** Native FUBAR window, hardware/RF, audio routing, screen readers, high contrast, native DPI scaling and touch devices. Native window launch against the operator profile was avoided because startup loads/migrates settings and recordings, prunes, applies the website configuration and starts capture. The paired harness ran the actual FUBAR executable in web-only CLI mode.
- Only this documentation file is added. No application code, operator settings or tests are changed. This report is published without a new binary version. Unrelated untracked files are preserved.

## Top Eight Priorities

### F01 P1 Explicit Capture Start and Source Selection

**Source-confirmed:** Window creation posts Start automatically. Save audio and monitoring default on. Without a saved input, digital silence can trigger probing of other physical inputs. Refresh clears the selected device identity and starts the engine again.

Evidence: [startup at app_window.cpp:301](../src/app_window.cpp#L301), [defaults at app_window.cpp:1031](../src/app_window.cpp#L1031), [probing at app_window.cpp:752](../src/app_window.cpp#L752), [Refresh at app_window.cpp:637](../src/app_window.cpp#L637).

**Inferred impact:** A first-time user opening the application to inspect settings may capture or record an unintended microphone. Refresh suggests enumeration, not a change of capture source and operating state. Automatic probing is conditional, not asserted for every startup.

**Proposed behavior:** Require a confirmed input and explicit Start on first use. Offer remembered auto-start as an explicit unattended-operation preference. Refresh should preserve the selected identity and stopped/running state; an unavailable input should require a deliberate replacement choice.

**Acceptance:** First-run and stopped-state configuration produce no capture. Device refresh never silently substitutes a source. Existing unattended behavior remains available through an explicit preference.

### F02 P1 Preserve Stop While Editing

**Source-confirmed:** Device/channel selections and recording checkbox changes call `startEngine()` unconditionally. In contrast, the threshold slider updates its displayed value without applying the new threshold until a restart. The primary button is labelled Apply / Restart.

Evidence: [change handlers at app_window.cpp:387](../src/app_window.cpp#L387), [threshold at app_window.cpp:407](../src/app_window.cpp#L407), [restart at app_window.cpp:670](../src/app_window.cpp#L670).

**Inferred impact:** Stop is not a reliable state while preparing another capture. Controls that look equally immediate have different application rules; the displayed threshold can differ from the running engine's threshold.

**Proposed behavior:** Edits while stopped remain drafts. Separate Start from Apply changes while running, with an explicit pending indicator where necessary. Do not restart solely because a stopped user's checkbox or device selection changes.

**Acceptance:** After Stop, changing every capture control leaves the engine stopped. The UI distinguishes pending values from applied values and states when applying will interrupt an active clip.

### F03 P2 Keep Mobile Tuning Together

**Observed:** At 390x844, after an invalid-frequency notice, the walkthrough measured the frequency field at page y=782 and Tune at y=1326, with advanced fields between them. The full-page layout was coherent, but the header consumed more than 350 pixels before task content. These are measurements of the released website, not an estimate from CSS.

**Source-confirmed:** At narrow widths the grid becomes one column. Frequency and Mode on Tune precede bandwidth, filters, gain, volume, squelch and direct sampling; Tune comes after all of them. Global command feedback is above task navigation.

Evidence: [responsive layout at web_server.cpp:123](../src/web_server.cpp#L123), [header/feedback at web_server.cpp:130](../src/web_server.cpp#L130), [tuning order at web_server.cpp:358](../src/web_server.cpp#L358).

**Impact:** The common edit-frequency-and-tune task requires substantial scrolling and separation from its feedback. This is a reachability issue, not evidence of overlap or horizontal overflow.

**Proposed behavior:** Keep radio, frequency, mode and Tune in one compact primary group. Put less frequent RF/filter controls in a clearly labelled expandable section. Reduce narrow-screen header repetition and retain a concise lease/status line. Preserve the existing clear global validation message, with local field association as needed; avoid a wholesale redesign or an overlay that obscures content.

**Acceptance:** At 390x844, a user can edit frequency and reach Tune without passing every advanced field. Error text, keyboard focus and expanded advanced controls remain usable without overlap.

### F04 P2 Keep Native Polling Off the UI Thread

**Source-confirmed:** The window timer calls `syncP25Status()`, which synchronously calls the Town DLL. The status configuration supplies a 2500 ms timeout.

Evidence: [timer work at app_window.cpp:730](../src/app_window.cpp#L730), [status call at app_window.cpp:1188](../src/app_window.cpp#L1188), [timeout at sdr_town_bridge.cpp:92](../src/sdr_town_bridge.cpp#L92), [synchronous DLL invocation at sdr_town_bridge.cpp:256](../src/sdr_town_bridge.cpp#L256).

**Inferred impact:** A slow status request can delay input, repainting and delivery of the Stop command. No native freeze was observed in this audit. This source path is not proof of the cause of the earlier paired CI timeout.

**Proposed behavior:** Use one background status request at a time and post completed state back to the window. Keep local controls responsive and show stale/disconnected status with its age.

**Acceptance:** A deliberately delayed or unavailable Town status endpoint does not block native keyboard navigation, painting or local Stop dispatch.

### F05 P2 Preserve Website Startup Errors

**Source-confirmed:** A failed web start clears `webEnabled_` before refreshing status. The status function then selects Website off instead of `lastError()`. Open site and Copy URL fall back to localhost port 80 when the server is not running.

Evidence: [failure at app_window.cpp:1128](../src/app_window.cpp#L1128), [status choice at app_window.cpp:1086](../src/app_window.cpp#L1086), [address fallback at app_window.cpp:1208](../src/app_window.cpp#L1208).

**Inferred impact:** Enabling the website can appear to do nothing, and the offered address can point to another service or a nonexistent page rather than the configured website.

**Proposed behavior:** Distinguish Off, Starting, Serving and Failed. Preserve the attempted port and failure reason with Retry. Enable Open/Copy only for a valid serving address.

**Acceptance:** Port conflict and startup failure retain their actionable reason. No inactive fallback address is presented as this station's working URL.

### F06 P2 Distinguish Rejection From Unknown Command Outcome

**Source-confirmed:** The generic web command path displays an error and re-enables controls after failed or missing responses. It displays error text without interpreting the structured error code as an outcome category. Some workflow callers refresh afterward, but the generic failure path does not reconcile the result.

Evidence: [response failure at web_server.cpp:1866](../src/web_server.cpp#L1866), [exception and unlock at web_server.cpp:1872](../src/web_server.cpp#L1872), [aircraft readback at web_server.cpp:2689](../src/web_server.cpp#L2689).

**Inferred impact:** A timed-out acknowledgement can look like a definitive rejection, inviting the user to repeat an action that already applied. This does not establish the latency cause or turn a passing CI rerun into a fix.

**Proposed behavior:** Retain action and target identity in feedback. For ambiguous transport failures, show Outcome unknown and perform a bounded readback before describing the result. Keep validation/rejection distinct from uncertainty. Preserve the existing no-automatic-mutation-retry behavior and permission checks.

**Acceptance:** Rejected, applied, still-pending and unknown outcomes are distinguishable. A lost acknowledgement never triggers a second mutation automatically.

### F07 P2 Make the Physical Radio and Workspace Target Explicit

**Observed:** The desktop view contained long device choices and an Assigned radio selection that did not identify the resolved physical receiver.

**Source-confirmed:** Workflow dropdowns pass through device labels and use an empty-value Assigned radio fallback; SSTV uses Receiver tap. The main Radio control separately seeds its active device identity, so the finding is not that all selectors always hide it. New workflow sessions initially display generated IDs, and Close on primary is rejected only after activation.

Evidence: [workflow device labels at web_server.cpp:2354](../src/web_server.cpp#L2354), [main Radio seeding at web_server.cpp:1746](../src/web_server.cpp#L1746), [session actions at web_server.cpp:2374](../src/web_server.cpp#L2374).

**Impact:** A user can understand the selected task without knowing which physical receiver the next command affects. Generated session identifiers and the separate timed control lease add terminology burden.

**Proposed behavior:** Show resolved assignment beside the selector using a short model/name plus a distinguishing identity suffix, with full identity available on demand. Distinguish current assignment from a proposed replacement; never guess missing assignment data. Use readable workspace names, disable unsupported primary closure, and name the target/consequences of closing an active workspace. Keep timed control access separate from receiver workspace terminology. Preserve host-only boundaries and native ownership refusal.

**Acceptance:** Before Start/Tune/Close, the affected workspace and known physical receiver are visible. Unknown or unavailable assignments are explicit. This must not imply that browser audio automatically follows each RF workspace; FUBAR still publishes its configured capture input.

### F08 P2 Preserve Keyboard Position During Refresh

**Source-confirmed:** Web recording rendering replaces all list buttons through `innerHTML`, including on play/pause and regular refresh. Native replay refresh clears the list and selects the newest recording.

Evidence: [web replacement at web_server.cpp:488](../src/web_server.cpp#L488), [refresh at web_server.cpp:2744](../src/web_server.cpp#L2744), [native selection reset at app_window.cpp:899](../src/app_window.cpp#L899).

**Inferred impact:** Background updates can remove the focused control or change the recording selected for playback. This was not a live keyboard reproduction during this audit; it is separate from the verified good web tab navigation.

**Proposed behavior:** Update recording rows by stable identity and preserve focus, selection and scroll position. Do not select a newly arrived recording unless the user requests following the newest item.

**Acceptance:** While new recordings arrive, Tab/Space/Enter continue to act on the user's intended recording. Play/pause updates retain the same focused control.

## Confirmed Positives and Unranked Observations

- **Observed:** Production-web arrow-key navigation and roving tabindex worked. Source implements Left/Right/Home/End and selected-tab focus at [web_server.cpp:1331](../src/web_server.cpp#L1331). Preserve this behavior.
- **Observed:** Invalid frequency produced a clear global error. Do not describe validation feedback as absent; F03 concerns its distance from the mobile action and field, not its clarity.
- **Observed:** Full-page web panels looked coherent. Header height and mobile action placement warrant bounded composition changes, not a wholesale visual redesign.
- **Observed and source-explained:** Inmarsat initially instructed enabling Town control before asynchronously resolving to idle/0 MHz. Initial `enabled:false` at [web_server.cpp:439](../src/web_server.cpp#L439) is treated as disabled at [web_server.cpp:2465](../src/web_server.cpp#L2465). A future state-copy pass should distinguish unknown/loading from operator-disabled and display untuned state explicitly.
- **Source-confirmed, unranked:** Inmarsat returns when no map layer exists at [web_server.cpp:2535](../src/web_server.cpp#L2535), before its map-library-unavailable message at [web_server.cpp:2555](../src/web_server.cpp#L2555). External-map availability and a non-map track alternative remain separate qualification gaps.
- Native startup/settings pruning and explicit recording-deletion confirmations were inspected. No deletion was invoked, and this audit does not qualify native consent, recovery or visual layout.

## Queue Scope

No new queue-admission defect was established. Existing lease and admission safeguards must remain intact; this audit does not propose changing their timing or policy.

## Coverage Inventory

| Area | Coverage | Qualification boundary |
| --- | --- | --- |
| Native first run | Startup, settings migration, input selection/probing, default capture/save/monitor, refresh | Source only; native window not launched |
| Native capture | VOX threshold, pre-roll/hold, frequency metadata, save/monitor/continuous/append/split, Start/Stop, meter/status | Source only; no RF/audio or device exercises |
| Native administration | Website/listing, ports, permission settings, retention, recording deletion, replay, persistence | Source only; no settings or files changed |
| Native navigation | Menus, tab stops, dialog-message handling, scroll-to-focus, replay controls | Source only; DPI, screen reader and actual focus behavior untested |
| Web Radio | Immediate mode versus Tune draft, frequency validation, P25, RTL/SDRplay capability controls | Source plus observed validation/mobile behavior |
| Web listening | Live/stop/background player, mono/stereo, reconnect/queue text, recording play/pause, community stations | Idle connection/stop UI observed; no intelligible live-audio qualification |
| Web decoders | FM/RDS, UHF/VHF tones, SSTV receive/finish/cancel and image presentation | Source and rendered page inspection; no live RF acceptance |
| Web workspaces | Satcom passes/scanning, Inmarsat plans/watch/messages/map, Aircraft local/network/map, named sessions | Source plus observed initial states and assignment presentation |
| Shared control | Permission gates, control lease/queue, errors, drafts, busy flags, freshness and bounded paging | Source review plus 900-second actual-app pair assertions; no physical RF |
| Web accessibility | Tab semantics/roving focus, labels, status regions, dynamic lists, map alternatives, responsive source order | Actual tab navigation passed; no screen-reader/contrast/touch certification |
| Existing evidence | ALIGNMENT_20261011, UI_QUALIFICATION, production JS tests and isolated browser-test coverage | Read only; prior PASS results are not a new human acceptance run |

The companion report in SDR Town, `docs/AUDIT_20261016_HUMAN_UI.md`, covers the native receiver and specialist task layout separately.

## Completion

Audit complete as documentation only. The eight priorities combine source-backed behavior with actual web observations. Implementation should remain separately scoped and preserve current transport, privacy, lease and native-device ownership safeguards.

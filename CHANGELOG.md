# Changelog

## 1.1.10

- Live listen no longer plays the stream twice (Web Audio plus the hidden player), which sounded
  like a delayed ghost of the same audio.

## 1.1.9

- Live listen keeps stereo for WFM/SDR instead of folding left and right together, which was adding
  static. Quiet signals are dithered so they stay clean in 16-bit.
- Phone lock and a minimised browser keep the live stream playing, using a real media session like
  YouTube Music. The page also reconnects if the OS still pauses it.

## 1.1.8

- Public Server lists a station on the 24/7 hub at `https://gearsqueens.online/fubar-net`.
- Every FUBAR website loads that live directory so listeners can jump between stations.
- The hub itself is published at `https://gearsqueens.online/fubar/` because the domain is HTTPS-only.
- Heartbeats keep the list fresh; a station drops off about 90 seconds after it goes quiet.

## 1.1.7

- Live listen stays up when capture restarts, and the browser reconnects if the stream drops.
- Tools → Manage recordings deletes selected clips or files older than N days, and shows folder size.
- Tools → Settings can boost quiet live audio (SDR/VB-CABLE) without changing saved WAVs, and can
  auto-delete recordings older than a chosen number of days.
- Device, VOX, and website settings are remembered. A silent VB-CABLE input is no longer replaced
  by a microphone after two seconds.
- New recordings include the radio frequency in the file name, e.g. `268000kHz`.
- Copy URL puts the LAN website address on the clipboard for phones and other PCs.

## 1.1.6

- Any number of people can listen live at once; a second listener no longer drops the first.
- Tools → Settings sets the maximum live listeners (default 5). Extra visitors wait in a queue
  until a slot frees. Lowering the cap does not kick anyone already listening.
- The public site shows how many people are listening and how many are waiting.

## 1.1.5

- Saves captures to `%AppData%\Roaming\FUBAR\Vox_captures` and reloads that folder every start.
- Copies clips from the old exe-side `recordings` folder into AppData once, so history is not lost.

## 1.1.4

- Live listen now plays through Web Audio from a raw PCM stream, so browsers actually hear the capture.
- Stopped stuffing silence into the live stream, which made the player go quiet between packets.
- Lower live latency by joining nearer the capture edge and skipping ahead if the browser buffer grows.

## 1.1.3

- Live website stream of the same audio FUBAR is capturing, shared by all listeners from one ring buffer.
- Listen live in the browser without waiting for a saved clip.

## 1.1.2

- Added a public capture website on port 80 that lists recordings and plays them in the browser.
- Admins can enable or disable the website from the GUI; the setting is saved.
- Headless runs can serve the same site with `--web`.

## 1.1.1

- The GUI now detaches from its console by default so no terminal remains visible.
- Added `--cli` to explicitly retain terminal output for headless operation, diagnostics, and automation.

## 1.1.0

- Renamed the program, executable, package, recording prefix, and repository to FUBAR.
- Embedded the FUBAR icon in the Windows executable and application window.
- Added a 180-degree rotated FUBAR wordmark to the main window.
- The status panel is red while idle/listening/paused and green only while recording.

## 1.0.2

- Fixed a WinMM monitor-buffer cleanup deadlock that could hang or crash on another PC.
- Prevents VB-CABLE and Voicemeeter capture-to-playback feedback loops and meter saturation.
- Added bounded event-driven monitor buffering to prevent unbounded memory growth.
- Hardened WASAPI decoding for 8/16/24/32-bit PCM plus 32/64-bit floating-point formats.
- Sanitizes invalid, infinite, and out-of-range samples before metering or conversion.
- Reports the active input format and catches audio-worker exceptions with a visible error.

## 1.0.1

- Fixed the disabled Apply button that prevented changed recording options from taking effect.
- Device, channel, and checkbox changes now restart capture immediately.
- Selects the Windows default capture endpoint and falls back from digitally silent inputs.
- Added input-device refresh and visible numeric dBFS readings for all four meters.
- Added clearer active-device and no-input-signal status messages.

## 1.0.0

- Native WASAPI capture with active Windows input-device enumeration.
- Stereo, left-only, right-only, and mono-mix routing.
- Live output monitoring and four independent input/output level meters.
- Configurable VOX threshold, pre-roll, and silence hold time.
- Resumable VOX sessions that pause on silence and append later audio to one WAV.
- Synchronized split-stereo recording to separate mono left/right WAV files.
- Automatic and continuous 16-bit PCM WAV recording.
- Replay-log window with playback and Explorer integration.
- Headless command-line mode for scripts and unattended operation.
- Portable x64 package requiring only standard Windows system DLLs.

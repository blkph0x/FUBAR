# Changelog

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

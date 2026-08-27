# Changelog

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

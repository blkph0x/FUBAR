# AudioVox

AudioVox is a Windows C++ VOX monitor and recorder. It starts as a console program and opens a
native GUI unless `--headless` is supplied. Both interfaces use the same WASAPI capture engine.

## Features

- Enumerates active Windows recording devices, including VB-Audio Cable devices.
- Supports stereo, left-only, right-only, and mono-mix routing.
- Live speaker monitoring of the selected route.
- Separate input-left, input-right, output-left, and output-right level meters.
- Adjustable VOX threshold from -60 dBFS to -5 dBFS.
- Configurable pre-roll and silence hold time.
- Creates standard 16-bit PCM WAV recordings automatically when audio crosses the threshold.
- Optional append-session mode pauses file writes during silence and resumes into the same WAV.
- Optional split-stereo mode creates synchronized mono `_left.wav` and `_right.wav` files.
- Optional continuous recording mode for testing or unattended capture.
- Separate replay-log window with timestamp, frequency, mode, duration, peak level, playback,
  and Explorer access.
- Full terminal operation for scripts, scheduled jobs, and automation.

## Build

The included PowerShell script locates MinGW-w64 from `PATH` (with a fallback to
`%USERPROFILE%\gcc\bin`) and runs the test suite after compiling:

```powershell
cd "C:\Users\Blkph0x\Documents\New project 2\AudioVox"
.\scripts\build.ps1 -Configuration Release
```

The resulting executable is in `build-release-mingw\AudioVox.exe`.

To build, create a public GitHub repository, push the source/tag, and publish the portable ZIP as
a release asset after authenticating the GitHub CLI, run:

```powershell
.\scripts\publish-github.ps1
```

## GUI operation

Run `AudioVox.exe` with no arguments. The console remains available for diagnostics and the GUI
opens on top. Capture starts automatically with the selected/default device.

1. Select the recording device and channel route.
2. Move the trigger threshold. A lower value is more sensitive.
3. Set pre-roll and hold time.
4. Keep **Save audio files** enabled for VOX clips.
5. Keep **Live monitor** enabled to hear the routed channel through the default output.
6. Enable **Append VOX to one file** to omit silent gaps without closing the current recording.
7. Enable **Split stereo into L/R files** in Stereo mode to save each channel separately.
8. Click **Start / Apply** after changing settings.
9. Open **Replay log** to play completed files or locate them in Explorer.

Use headphones when live monitoring a microphone to avoid acoustic feedback.

## Terminal automation

List capture devices:

```powershell
.\AudioVox.exe --list-devices
```

Monitor the left channel and create clips at -32 dBFS:

```powershell
.\AudioVox.exe --headless --device 2 --mode left --threshold-db -32 `
  --pre-roll 1 --hold 1.5 --output "D:\AudioVox Recordings"
```

Run a ten-second mono capture test without speaker monitoring:

```powershell
.\AudioVox.exe --headless --mode mono --duration 10 --force-record --no-monitor
```

Keep one VOX session open across quiet periods and save stereo channels separately:

```powershell
.\AudioVox.exe --headless --mode stereo --append-session --split-stereo `
  --threshold-db -32 --hold 1.5 --output "D:\AudioVox Recordings"
```

Run `AudioVox.exe --help` for every option. A headless run with no duration continues until
`Ctrl+C`.

## Recording behavior

The engine reads the selected endpoint's native shared-mode format, converts samples internally
to normalized floating point, applies the selected channel route, and writes 16-bit PCM WAV data.
The trigger follows the peak level of the routed output. Pre-roll prevents the beginning of a
transmission from being cut off; hold time prevents short pauses from splitting one transmission
into many files.

With append-session enabled, reaching the hold time pauses writing instead of finalizing the WAV.
The next threshold crossing appends pre-roll and new audio to that same file. Silent gaps are not
stored, and the file is finalized when **Stop** is clicked, the headless duration expires, or the
program exits. Without append-session, each VOX transmission remains a separate file.

Split-stereo only changes Stereo mode. It writes synchronized mono files whose names end in
`_left.wav` and `_right.wav`; left-only, right-only, and mono-mix routes still write one file.

The radio frequency field is metadata for the replay log and filename context. It does not tune
the source device.

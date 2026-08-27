# AudioVox

AudioVox is a Windows C++ VOX monitor and recorder. It starts as a console program and opens a
native GUI unless `--headless` is supplied. Both interfaces use the same WASAPI capture engine.

## Features

- Enumerates active Windows recording devices, including VB-Audio Cable devices.
- Selects the Windows default input and automatically tries another physical input if it is
  delivering digital silence.
- Supports stereo, left-only, right-only, and mono-mix routing.
- Live speaker monitoring of the selected route.
- Virtual-cable monitor safety prevents routing a cable capture back into its playback endpoint.
- Separate input-left, input-right, output-left, and output-right level meters.
- Numeric dBFS readouts make muted or silent inputs immediately visible.
- Adjustable VOX threshold from -60 dBFS to -5 dBFS.
- Configurable pre-roll and silence hold time.
- Creates standard 16-bit PCM WAV recordings automatically when audio crosses the threshold.
- Optional append-session mode pauses file writes during silence and resumes into the same WAV.
- Optional split-stereo mode creates synchronized mono `_left.wav` and `_right.wav` files.
- Optional continuous recording mode for testing or unattended capture.
- Separate replay-log window with timestamp, frequency, mode, duration, peak level, playback,
  and Explorer access.
- Full terminal operation for scripts, scheduled jobs, and automation.
- Handles shared-mode 8/16/24/32-bit PCM and 32/64-bit floating-point input safely.

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
opens on top. Capture starts with the Windows default input. If that endpoint produces digital
silence for two seconds, AudioVox tries the next physical input automatically.

1. Select the recording device and channel route.
   The status banner names the active input; use **Refresh** after connecting a new device.
2. Move the trigger threshold. A lower value is more sensitive.
3. Set pre-roll and hold time.
4. Keep **Save audio files** enabled for VOX clips.
5. Keep **Live monitor** enabled to hear the routed channel through the default output.
6. Enable **Append VOX to one file** to omit silent gaps without closing the current recording.
7. Enable **Split stereo into L/R files** in Stereo mode to save each channel separately.
8. Device, route, and checkbox changes apply immediately. Click **Apply / Restart** after editing
   threshold, timing, frequency, or output-folder values.
9. Open **Replay log** to play completed files or locate them in Explorer.

Use headphones when live monitoring a microphone to avoid acoustic feedback.
If every input remains at `-90.0 dB`, check the microphone's hardware mute switch and Windows
microphone privacy settings, then select another input or click **Refresh**.

### VB-CABLE setup

1. Send the source application's playback to **CABLE Input**.
2. Select **CABLE Output (VB-Audio Virtual Cable)** as the AudioVox input.
3. Confirm the numeric input meters move and set the VOX threshold below the displayed signal.
4. AudioVox keeps recording and metering active but safety-locks its own live monitor for virtual
   cable inputs. Monitor through a hardware output or a separate Voicemeeter route instead.

This follows VB-Audio's endpoint direction: CABLE Input is the playback side and CABLE Output is
the recording side. See the [official VB-CABLE reference manual](https://vb-audio.com/Cable/VBCABLE_ReferenceManual.pdf).

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

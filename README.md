# FUBAR

FUBAR is a Windows C++ VOX monitor and recorder. It opens its native GUI without leaving a
terminal window visible. Add `--cli` for terminal diagnostics and automation, and `--headless`
to run without the GUI. Both interfaces use the same WASAPI capture engine.

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
- Stores clips in `%AppData%\Roaming\FUBAR\Vox_captures` and reloads them the next time FUBAR starts.
- Optional append-session mode pauses file writes during silence and resumes into the same WAV.
- Optional split-stereo mode creates synchronized mono `_left.wav` and `_right.wav` files.
- Optional continuous recording mode for testing or unattended capture.
- Separate replay-log window with timestamp, frequency, mode, duration, peak level, playback,
  and Explorer access.
- Optional public website (default port 80, or `--port N`) so anyone on the network can listen live and play captures.
- Tools → Settings caps simultaneous live listeners (default 5); extra visitors wait in a queue.
- Tools → Manage recordings deletes selected clips or anything older than a chosen number of days.
- Live listen boost, frequency in file names, and remembered device/VOX settings for SDR + VB-CABLE.
- Public Server directory at `https://gearsqueens.online/fubar-net` so listeners can find live stations.
- Stereo live listen for WFM/SDR (no noisy left+right fold) and background playback when a phone locks.
- Full terminal operation for scripts, scheduled jobs, and automation.
- Handles shared-mode 8/16/24/32-bit PCM and 32/64-bit floating-point input safely.

## Build

The included PowerShell script locates MinGW-w64 from `PATH` (with a fallback to
`%USERPROFILE%\gcc\bin`) and runs the test suite after compiling:

```powershell
cd "C:\Users\Blkph0x\Documents\New project 2\FUBAR"
.\scripts\build.ps1 -Configuration Release
```

The resulting executable is in `build-fubar-release-mingw\FUBAR.exe`.

To build, create a public GitHub repository, push the source/tag, and publish the portable ZIP as
a release asset after authenticating the GitHub CLI, run:

```powershell
.\scripts\publish-github.ps1
```

## GUI operation

Run `FUBAR.exe` with no arguments. The app detaches from the terminal and opens the GUI.
Capture starts with the Windows default input. If that endpoint produces digital
silence for two seconds, FUBAR tries the next physical input automatically.

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
10. Tick **Public website** to share the capture log. Default is port 80; change it in
    **Tools → Settings** or with `--port 8080`. Use **Open site** for this PC, or visit the LAN
    address shown next to the checkbox from a phone or another computer.
11. Open **Tools → Settings** to choose how many people can listen live at once (default 5),
    add live listen boost for quiet SDR audio, and auto-delete old recordings.
    Anyone above the live cap waits in a queue until someone stops listening.
12. Open **Tools → Manage recordings** to delete selected clips or files older than N days.
    **Copy URL** puts the LAN website address on the clipboard for phones.
13. Tick **Public Server** and set a station name to list this PC on
    `https://gearsqueens.online/fubar-net`. Other FUBAR websites show that live list. The 24/7 hub
    is `https://gearsqueens.online/fubar/`.

### Public website

The website is off until an admin enables it. While it is on, FUBAR serves a capture player at
`http://<this-pc>:80/`. Visitors can **Listen live** to the same audio the app is capturing, and
play saved WAV clips. Several people can listen at the same time. When the live cap is reached
(default 5, change it in **Tools → Settings**), new visitors wait in a queue until a slot frees.
Live audio is **16-bit PCM at the capture sample rate** (1:1, the same idea as KiwiSDR). That is
required for SSTV, POCSAG, weather-fax and other digital modes — MP3 would destroy them. Tap
**Listen live**. A ~1 second jitter buffer keeps WFM and voice continuous (no gargle from
underruns). Live defaults to **Mono** so WFM is clean on phone and PC speakers; tap **Stereo**
for 1:1 left/right. A 15 kHz FM low-pass turns on for WFM (and similar wide analog audio) and
turns itself off for mic, P25 and digital modes. On a phone the page takes a screen wake lock so
listening is not killed when the display would sleep. Set **Live listen boost** to 0 dB for
digital modes. If the stream still drops, the page reconnects. If you listen on the same PC that
is running FUBAR, mute the app’s local monitor or you will hear both — that delayed double can
sound like a vibration. For WFM, the SDR IF bandwidth needs to be wide enough (around 200–250 kHz);
a too-narrow filter will still sound rough no matter how FUBAR plays it.

### Public Server network

Tick **Public Server** and set a station name. This PC heartbeats to the hub at
`https://gearsqueens.online/fubar-net`. Every FUBAR website loads that list so listeners can jump
between stations. The 24/7 hub player is `https://gearsqueens.online/fubar/`. A station drops off
about 90 seconds after it goes offline. The hub PC only needs FUBAR open with **Public website**
enabled; **Public Server** is only if you want that PC on the list too.

A silent VB-CABLE input is not swapped for a microphone. Device, threshold, timing, and website
choices are saved. New WAV names include the radio frequency so SDR hops stay identifiable.

Port 80 is the default so phones can open `http://<this-pc>/` with no port number. If 80 is
taken (IIS, Skype, another FUBAR), pick another:

```powershell
.\FUBAR.exe --cli --headless --web --port 8080
```

Then visit `http://<this-pc>:8080/`. Ports below 1024 may need “Run as administrator”. If Windows
Firewall prompts, allow FUBAR so other devices on the network can connect.

Headless:

```powershell
.\FUBAR.exe --cli --headless --web --port 8080 --public-server --station-name "Home SDR" --live-listeners 5
```

Use headphones when live monitoring a microphone to avoid acoustic feedback.
If every input remains at `-90.0 dB`, check the microphone's hardware mute switch and Windows
microphone privacy settings, then select another input or click **Refresh**.

### VB-CABLE setup

1. Send the source application's playback to **CABLE Input**.
2. Select **CABLE Output (VB-Audio Virtual Cable)** as the FUBAR input.
3. Confirm the numeric input meters move and set the VOX threshold below the displayed signal.
4. FUBAR keeps recording and metering active but safety-locks its own live monitor for virtual
   cable inputs. Monitor through a hardware output or a separate Voicemeeter route instead.

This follows VB-Audio's endpoint direction: CABLE Input is the playback side and CABLE Output is
the recording side. See the [official VB-CABLE reference manual](https://vb-audio.com/Cable/VBCABLE_ReferenceManual.pdf).

## Terminal automation

List capture devices:

```powershell
.\FUBAR.exe --cli --list-devices
```

Monitor the left channel and create clips at -32 dBFS:

```powershell
.\FUBAR.exe --cli --headless --device 2 --mode left --threshold-db -32 `
  --pre-roll 1 --hold 1.5 --output "D:\FUBAR Recordings"
```

Run a ten-second mono capture test without speaker monitoring:

```powershell
.\FUBAR.exe --cli --headless --mode mono --duration 10 --force-record --no-monitor
```

Keep one VOX session open across quiet periods and save stereo channels separately:

```powershell
.\FUBAR.exe --cli --headless --mode stereo --append-session --split-stereo `
  --threshold-db -32 --hold 1.5 --output "D:\FUBAR Recordings"
```

Serve the live site on port 8080 instead of 80:

```powershell
.\FUBAR.exe --cli --headless --web --port 8080
```

Run `FUBAR.exe --cli --help` for every option. A headless run with no duration continues until
`Ctrl+C`.

## Recording behavior

Recordings default to `%AppData%\Roaming\FUBAR\Vox_captures` (for example
`C:\Users\You\AppData\Roaming\FUBAR\Vox_captures`). FUBAR creates that folder, writes WAV files
there, and on every start reloads the replay log and website from it. A custom folder chosen with
**Browse...** is still honoured. Settings live in `%AppData%\Roaming\FUBAR\FUBAR.ini`.

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

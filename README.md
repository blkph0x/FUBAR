# FUBAR

FUBAR is a Windows C++ VOX monitor and recorder. It opens its native GUI without leaving a
terminal window visible. Add `--cli` for terminal diagnostics and automation, and `--headless`
to run without the GUI. Both interfaces use the same WASAPI capture engine.

| | |
|---|---|
| **Current version** | **1.1.32** |
| **Releases** | https://github.com/blkph0x/FUBAR/releases |
| **Repo** | https://github.com/blkph0x/FUBAR |
| **Pairs with** | [SDR Town](https://github.com/Blkph0x/SDR_Town) (receiver / P25 / local control API) |

## FUBAR + SDR Town

FUBAR and [SDR Town](https://github.com/Blkph0x/SDR_Town) are separate apps that work as a pair on the same PC.

- **SDR Town** is the radio: tune, demodulate, follow P25 control channels, and play clear voice to speakers or a virtual cable.
- **FUBAR** is the station front end: capture that audio, VOX-record clips, and publish a public website so phones and other PCs can listen live.

They talk over a **local-only** control link (`127.0.0.1`, default port **8765**). Visitors never connect to SDR Town directly. They use FUBAR’s website; FUBAR talks to SDR Town through `SdrTownControl.dll` when an admin enables it.

### Typical use cases

| Use case | What you do | What visitors get |
|----------|-------------|-------------------|
| **Public listening post** | Route SDR Town audio into VB-CABLE → FUBAR; enable **Public website** (+ optional **Public Server**) | Live stream, typed **Now playing**, saved clips, station list on the hub |
| **P25 trunking display** | Monitor a control channel in SDR Town with auto-follow; leave FUBAR website on | Main title stays your **Now playing** text; a smaller line shows live `TG …` + alpha, or `Listening to NSWGRN Control` on the CC |
| **Remote-ish tune from the website** | Enable SDR Town control in FUBAR **Tools → Settings**; visitor clicks **Take control** | Queued, timed lease to change frequency, mode, BW, LPF, volume, RF gain (if allowed), and P25 Monitor CC |
| **Unattended capture** | Headless FUBAR on the cable output while SDR Town stays on a channel or P25 follow | WAV clips under `%AppData%\Roaming\FUBAR\Vox_captures` plus optional live site |

### How the link works

```text
  RF → SDR Town (demod / P25) → speakers or VB-CABLE
                                    ↓
                              FUBAR (WASAPI capture)
                                    ↓
                     Public website  ·  VOX WAVs  ·  FUBAR Net hub
                                    ↓
                     (optional) SdrTownControl.dll → 127.0.0.1:8765
                                    ↓
                              SDR Town local control API
```

1. Start **SDR Town** with its local control server (default on install/portable builds; loopback only).
2. Route audio to FUBAR (VB-CABLE is the usual path for a clean station feed).
3. In FUBAR, enable **Public website**. Type **Now playing** for the large title visitors see.
4. In **Tools → Settings**, turn on **SDR Town control** and choose what remote operators may change. Keep `SdrTownControl.dll` beside `FUBAR.exe` (release ZIPs include it).
5. Open the FUBAR site from a phone or LAN PC. Live audio comes from FUBAR’s capture. Live P25 status and tune commands use the control bridge when enabled.

### Website status board

- **Now playing** (large): only what you type in FUBAR. SDR Town never overwrites it.
- **P25 status** (smaller, under the title): live from SDR Town when control is enabled  
  - on a voice follow → talkgroup + alpha tag (for example `TG 30003 118 ILLAW A`)  
  - parked on a monitored control channel → `Listening to NSWGRN Control`  
  - no P25 activity → line hidden
- **UHF tones tab** (with SDR Town **0.2.65+**): live CTCSS/DCS, DTMF when Repeater Control Monitor is on, plus a readable heard list

### Downloads

- FUBAR releases: https://github.com/blkph0x/FUBAR/releases  
- SDR Town releases (installer, portable ZIP, `SdrTownControl` DLL): https://github.com/Blkph0x/SDR_Town/releases  

Full SDR Town docs and P25 notes live in that repo’s README.

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
- Optional public website (default port 80, or `--port N`) so anyone on the network can listen live, see **now playing**, a live P25 status line when paired with SDR Town, and play captures.
- Tools → Settings caps simultaneous live listeners (default 5); extra visitors wait in a queue.
- Tools → Manage recordings deletes selected clips or anything older than a chosen number of days.
- Live listen boost, frequency in file names, and remembered device/VOX settings for SDR + VB-CABLE.
- Public Server directory at `https://gearsqueens.online/fubar-net` so listeners can find live stations.
- Stereo live listen for WFM/SDR (no noisy left+right fold) and background playback when a phone locks.
- Optional SDR Town control panel on the website. FUBAR loads `SdrTownControl.dll` beside `FUBAR.exe` and can tune a local SDR Town instance after an admin enables allowed controls in **Tools → Settings**. See [FUBAR + SDR Town](#fubar--sdr-town).
- SDR Town website control is session based: visitors click **Take control**, queued users wait their turn, the active session has a fixed timer, and only the active controller can extend it.
- Website controllers can adjust SDR Town bandwidth, RF gain, volume, audio LPF on/off/cutoff, and P25 control-channel monitoring from known or manually entered channels.
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
14. Type **Now playing** (letters, numbers, and spaces) so website visitors can see what you are
    listening to. It updates on the site as you type. When SDR Town control is enabled, a smaller
    live P25 line appears under that title (talkgroup/alpha or control-channel listening) without
    changing your typed text.

### Public website

The website is off until an admin enables it. While it is on, FUBAR serves a capture player at
`http://<this-pc>:80/`. Visitors can **Listen live** to the same audio the app is capturing, see
the **Now playing** line the operator typed, see the live **P25 status** subtitle when SDR Town is
linked, and play saved WAV clips. Several people can listen at the same time. When the live cap is reached
(default 5, change it in **Tools → Settings**), new visitors wait in a queue until a slot frees.
Live audio is **16-bit PCM at the capture sample rate** (1:1, the same idea as KiwiSDR). That is
required for SSTV, POCSAG, weather-fax and other digital modes — MP3 would destroy them. Tap
**Listen live**. A ~1 second jitter buffer keeps audio continuous. FUBAR does **not**
low-pass, de-emphasize, or resample the capture — AM, NFM, WFM, and digital modes are
streamed 1:1 as the SDR (or mic) already produced them. Live defaults to **Mono** so
both speakers play the same mix; tap **Stereo** for true left/right. Live is always the
capture PCM (1:1). The page detects the browser and uses the keep-alive that device allows. Android Chrome
plays an MP3 radio stream so the phone status-bar player stays up when you switch apps
(Chrome will not keep PCM/Web Audio running in the background). iPhone uses a hidden
video hold. Desktop stays 1:1 PCM. A screen wake lock is still taken while the page is in front. Set
**Live listen boost** to 0 dB for digital modes. If a VB-CABLE input is not 44.1/48 kHz
stereo, the admin status bar warns you to fix it in VB-Audio Control Panel. If the
stream still drops, the page reconnects. If you listen on the same PC that is running
FUBAR, mute the app’s local monitor or you will hear both. WFM still needs a wide SDR
IF bandwidth (around 200–250 kHz); a too-narrow filter will sound rough no matter how
FUBAR plays it.

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

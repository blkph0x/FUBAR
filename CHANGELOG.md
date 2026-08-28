# Changelog

## 1.1.19

- Live listen no longer guesses FM or applies a 15 kHz low-pass. The SDR already
  filters AM/NFM/WFM; FUBAR streams what it gets, 1:1, with no extra EQ.
- Capture no longer downsamples high-rate VB-CABLE to 48 kHz.
- If a virtual cable is not 44.1/48 kHz stereo (for example 96 kHz or 7.1), the
  admin banner warns to fix it in VB-Audio Control Panel.

## 1.1.18

- The website can use a port other than 80. From a terminal:
  `FUBAR.exe --cli --headless --web --port 8080`
  `--port` also works with the GUI and is saved in Settings.

## 1.1.17

- Live 15 kHz FM audio low-pass is automatic. It turns on when the capture has
  wideband energy (WFM, leaked stereo pilot) and turns off for mic, P25, SSTV and
  other already-narrow audio so digital modes stay 1:1. The live line shows
  **FM LPF 15 kHz** while it is engaged.

## 1.1.16

- Live WFM on phone/PC speakers was harsh while mic and P25 stayed clean. Broadcast
  stereo (and left-only SDR audio) is a poor match for small stereo speakers; P25 and
  mics are effectively the same on both channels so they hid it.
- Listen live now defaults to **Mono** (same audio in both speakers). Tap **Stereo**
  for true left/right. A 15 kHz low-pass keeps the FM pilot from rattling speakers.
- Virtual-cable capture above 48 kHz is pulled down to 48 kHz with Windows’ quality
  resampler so WFM treble is not aliased on phones.

## 1.1.15

- Live no longer gargles or vibrates on PC and phone. The player was pumping silence
  whenever the buffer dipped, which amplitude-modulated whatever was playing — Stereo,
  Left, Right and Mono all sounded the same because it was not a channel-mix bug.
- Listen live now waits about a second before starting, never chops zeros into a live
  stream, and pauses the download instead of dropping samples when the buffer is full.
- The keep-alive WAV is muted so it cannot mix with the capture. The level meter is
  drawn off the audio thread.

## 1.1.14

- Live WFM no longer “gurgles”: the player was skipping a few samples on every audio tick
  to catch up, which warbled stereo. The playhead now stays put.

## 1.1.13

- Live playback no longer jumps when the buffer grows. It uses a continuous ring (and an audio
  worklet when the browser allows it) so WFM stays time-stable instead of jittering.

## 1.1.12

- Live listen is 16-bit PCM at the capture rate again (1:1, KiwiSDR-style) so SSTV, POCSAG,
  weather-fax and other non-voice modes are not smashed by MP3.
- Playback uses a jitter buffer instead of scheduled snippets, which was the crackle.
- Phones request a screen wake lock while listening so the stream is not killed when the
  display would otherwise sleep. Leave live boost at 0 dB for digital modes.

## 1.1.11

- Live listen is now a real MP3 radio stream in the browser's media player, not Web Audio PCM.
  That keeps WFM smooth (no JS crackle) and keeps playing when a phone locks or the tab is
  backgrounded, the same way a normal radio stream works.

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

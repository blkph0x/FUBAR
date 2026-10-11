# Primary web-radio repair (Town DEC-0221)

User report: Listen audio works, but Tune/mode returns Invalid device/workflow.
Confirmed host saved a hardware-only radio identity while enumeration now uses
its serial. Town correctly refuses that missing identity; the website lacked
a primary selector. Add stable-key Tune/mode/P25 selection and useful status,
preserving lease/admin permissions and drafts. Do not fall back to radio zero.

Normal GUI startup also lacked a credential for Town's required-token server.
Town0.2.138 + matching DLL now provision/read protected same-Windows-user
pairing. The website never receives credentials. Explicit headless credentials
remain supported; the default CLI path now uses the same DLL pairing as GUI.

Local verification: FUBAR CTest4/4, production JS selection/draft/missing/old
server tests, actual explicit-token and automatic-pair DLL/GUI tests all PASS.
Unowned session commands denied, invalid keys rejected before mutation, missing
Tune/mode/P25 identity fails, valid keyed Tune/mode succeeds. Unauthenticated
native status401. Browser Take control/WFM/98.1MHz/180kHz Tune applied. Native
22/22 and204 focused ownership/pairing/P25 cases PASS. Normal physical RTL test
opened real hardware and accepted98.1/98.2/98.1MHz tunes through the same bridge.
Logs/screenshots in ignored build/pair-radio-auto and build/live-radio-test.

No vocoder/decoder/audio-path edits. Named sessions and reserved/busy radios
remain protected. A radio change during primary P25 requires stopping it first.
1.1.50 pins the matching0.2.138 DLL. Actions/public-package verification pending.

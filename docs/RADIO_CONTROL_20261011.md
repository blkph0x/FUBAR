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
1.1.50 pins the matching0.2.138 DLL. Main/tag Actions38132803982/38132806304
and release38132807766 PASS. Sourcec60f99227d9d84bce27eaf4be6752dc1100fce15.
Public Latest https://github.com/blkph0x/FUBAR/releases/tag/v1.1.50.
ZIP SHA256 da420fc9736c396de5bb4a8d88901b598f0bf38461fab242fcbfecca717eadf4;
EXE bda77484719cbad6c695c7e655708c71a193494fe54078823d6e35dc49301fa4.
Downloaded binary hashes/provenance, self-test and real paired automatic-auth
GUI/DLL tests PASS. Town0.2.138 also Latest, sourcefd000db, Actions38129155696.
Bundled DLL SHA256343d94d85c79b40c41fc68ccae136bc4ba2a5c6fd1e53fddf899ff2f54d54e7d
is identical to its public standalone and Town-package copies. Evidence under
build/public-1.1.50. Downloaded-pair explicit-token test and physical RTL
98.1/98.2/98.1MHz web tuning also PASS (real hardware, normal shutdown).
No physical SDRplay or new P25 audio-quality claim.

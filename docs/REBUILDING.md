# Rebuilding this package

FUBAR-corresponding-source.zip contains the exact tagged application and bundled
Shine source, compatibility header, nlohmann/json and CMake files used by this
release. Component license notices are in licenses/ and town-licenses/.

Extract the source into an empty directory. With Visual Studio 2022 C++ x64
tools and CMake installed, configure and build:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Shine is linked from third_party/shine/src/lib. Modifying those files and
rebuilding relinks FUBAR with the modified library. There is no signature check
or DRM preventing replacement. Modification for personal use and reverse
engineering to debug library modifications are permitted. Third-party licenses
continue to apply independently; this notice does not relicense their code.

The Town control DLL is dynamically loaded and can be replaced alongside
FUBAR.exe. town-pair.json identifies its exact Town source commit and Actions
build. Install the matching documented native runtime or retain the supplied
runtime files and their notices. Application builders are responsible for their
compiler licensing; no compiler license keys are distributed here.

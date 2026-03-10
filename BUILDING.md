# Building HOT4D for modern Cinema 4D versions

This repository is an early adaptation pass of the original HOT4D plugin toward the modern Cinema 4D versions SDK.

## Expected build system

The repository layout matches the Maxon/Cinema 4D **project tool** style plugin layout:

- `project/projectdefinition.txt` declares a `DLL` plugin target.
- `APIS=cinema.framework;math.framework;core.framework;misc.framework;` indicates the modern framework-based SDK.
- `source/framework_registration.cpp` includes generated registration glue.
- `source/OceanSimulation/OceanSimulation_decl.h` includes generated Maxon interface declaration headers.

In practice, this means the repository is **not self-contained**: it expects to be built inside a configured Cinema 4D SDK environment that generates some helper headers during project generation / build configuration.

## Known prerequisites

A full build likely requires all of the following:

1. The **modern Cinema 4D versions C++ SDK** (or a very close compatible SDK version).
2. The Maxon project generation/build tooling used by current SDK examples.
3. SDK include paths for the framework APIs and MoGraph / object plugin headers.
4. Generated registration/interface files that are not committed in this repository.

## Currently missing/generated files

At inspection time, these headers were referenced but not present in the repository:

- `source/registration_com_valkaari_hot4D.hxx`
- `source/OceanSimulation/OceanSimulation_decl1.hxx`
- `source/OceanSimulation/OceanSimulation_decl2.hxx`

Those are typical generated artifacts in a modern Cinema 4D framework project. Their absence currently blocks an out-of-the-box build from the repository alone.

## First-pass compatibility observations

### Likely modern-SDK aligned pieces

- `project/projectdefinition.txt` already uses framework-style API declarations.
- The plugin uses `maxon::Result`, `MAXON_INTERFACE`, `MatrixNxM`, `FFT`, and framework registration patterns.
- Resource registration and plugin registration calls are structurally aligned with newer SDK generations.

### Likely blockers / risk areas

- Missing generated registration/interface headers (hard blocker).
- No checked-in Visual Studio/Xcode/CMake project files; build must come from SDK tooling.
- The code relies on legacy/deprecated classes such as `C4D_Falloff`, which may need verification against the 2026 SDK.
- The effector implementation depends on MoGraph SDK details that should be verified against the actual 2026 headers.
- Some source include paths were inconsistent and have been normalized for case-sensitive/platform-portable builds.

## Recommended bring-up workflow

1. Install the modern Cinema 4D versions SDK.
2. Place this repository where the SDK tooling expects plugin projects, or integrate it into the SDK project generator workflow.
3. Generate the missing `*.hxx` registration/interface files.
4. Build once and collect the first real compiler error set.
5. Address SDK/API breakages iteratively from that compiler output.

## Scope of this first pass

This pass focused on:

- identifying the likely SDK/build expectations,
- documenting current blockers,
- making low-risk portability cleanups,
- avoiding speculative API rewrites without the real SDK present.


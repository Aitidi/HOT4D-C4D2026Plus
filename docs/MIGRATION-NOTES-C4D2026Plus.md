# HOT4D → Cinema 4D 2026+ migration notes

This note is a conservative source-tree map for future SDK-based porting work. It does **not** assume any unverified Cinema 4D 2026 API changes.

## Current architectural split

### Plugin entry / registration
- `source/main.cpp`
  - Plugin lifecycle entry points: `PluginStart`, `PluginEnd`, `PluginMessage`.
  - Registers the description, deformer, and effector.
  - **Upgrade risk:** low-to-medium.
  - **Why:** usually simple, but startup banners, registration ordering, and plugin flags may need cleanup once built against the real SDK.

- `source/framework_registration.cpp`
  - Includes generated framework registration glue.
  - **Upgrade risk:** high blocker, low code complexity.
  - **Why:** depends on generated SDK files not committed here.

- `source/main.h`
  - Forward declarations for registration functions.
  - **Upgrade risk:** low.

### Ocean simulation framework component
- `source/OceanSimulation/OceanSimulation_decl.h`
  - Declares the Maxon framework interface (`OceanInterface`) and generated interface hooks.
  - **Upgrade risk:** high.
  - **Why:** tied directly to Maxon framework declaration macros and generated `*_decl*.hxx` files.

- `source/OceanSimulation/OceanSimulation_impl.cpp`
  - Core ocean simulation implementation.
  - Handles initialization, Phillips spectrum setup, FFT-based animation, interpolation, jacobian generation, and evaluation.
  - Uses `MatrixNxM`, `FFT`, `ParallelFor`, and `ParallelImage`.
  - **Upgrade risk:** medium.
  - **Why:** numerics are self-contained, but framework/container/threading/FFT API signatures may have drifted.
  - **Most fragile points inside this file:**
    - `ParallelImage::Process(...)` usage and tile sizing assumptions.
    - `FFTClasses::Kiss().Create()` and `Transform2D(...)` signatures/support flags.
    - generated framework registration through `MAXON_COMPONENT_CLASS_REGISTER`.

- `source/OceanSimulation/OceanSimulation_unit_test.cpp`
  - SDK-side unit/speed tests for the framework component.
  - **Upgrade risk:** low-to-medium.
  - **Why:** useful for bring-up, but test registration APIs may require small signature updates.

### Cinema 4D object/deformer plugin layer
- `source/C4D_Object/OceanSimulationDeformer.h`
- `source/C4D_Object/OceanSimulationDeformer.cpp`
  - Main object modifier/deformer implementation.
  - Reads UI parameters, manages falloff/fields integration, runs ocean simulation, modifies points, writes jacobian/foam data to `VertexColorTag`, and updates point selections.
  - **Upgrade risk:** very high.
  - **Why:** this is the heaviest integration point with host-side C4D APIs.
  - **Highest-risk API areas:**
    - `C4D_Falloff` / field integration (`InitFalloff`, `SetMode(FIELDS, ...)`, `PreSample`, `Sample`, drawing/handles).
    - `VertexColorTag` read/write access (`GetDataAddressW`, `Set`, `Get`, `SetPerPointMode`).
    - object deformation flow (`ModifyObject`, dirty propagation, point access).
    - MoGraph-adjacent assumptions such as vertex map weighting via `CalcVertexMap`.
  - **Code-health observations:**
    - `MapRange` is reused for jacobian/foam normalization and should remain numerically defensive.
    - Pre-roll foam work does a full historical simulation at frame 0 and may need profiling once it compiles again.

### Cinema 4D MoGraph effector layer
- `source/C4D_Object/OceanSimulationEffector.h`
- `source/C4D_Object/OceanSimulationEffector.cpp`
  - Effector implementation that evaluates ocean displacement per MoGraph point and writes position/rotation/scale/color strengths.
  - **Upgrade risk:** very high.
  - **Why:** depends on MoGraph-specific plugin APIs that often drift across major SDK revisions.
  - **Highest-risk API areas:**
    - `EffectorData`, `EffectorDataStruct`, `EffectorStrengths`, `MoData`.
    - `RegisterEffectorPlugin(...)` flags and registration expectations.
    - execution scheduling through `AddToExecution` / `Execute`.
  - **Behavioral note:** currently writes the same displacement vector into position, rotation, and scale strengths; this may still compile but should be validated for intended behavior once running in a real host.

### Resource / description registration
- `source/C4D_Object/OceanSImulationDesc.cpp`
  - Registers the shared description resource.
  - **Upgrade risk:** low.
  - **Note:** filename contains an inconsistent capital `I` in `SImulation`; harmless on Windows, easy to trip on elsewhere.

- `res/description/*.res`, `res/description/*.h`, `res/strings_en-US/**/*`
  - UI/resource definitions for deformer, effector, and shared parameters.
  - **Upgrade risk:** medium.
  - **Why:** resource syntax is usually stable, but symbol IDs, field UI embedding, and expected parameter types must match host-side code exactly.

### Project metadata
- `project/projectdefinition.txt`
  - Framework project declaration for SDK tooling.
  - **Upgrade risk:** medium.
  - **Why:** small file, but it defines the module identity and framework dependencies that decide whether generated files appear correctly.

## Practical risk ranking

### Highest risk / likely first real SDK breakpoints
1. `source/C4D_Object/OceanSimulationDeformer.cpp`
2. `source/C4D_Object/OceanSimulationEffector.cpp`
3. `source/OceanSimulation/OceanSimulation_decl.h` + generated `*_decl*.hxx`
4. `source/framework_registration.cpp` + generated registration glue
5. `res/description/*` resource/parameter consistency
6. `source/OceanSimulation/OceanSimulation_impl.cpp`
7. `source/main.cpp`

## Missing/generated files currently blocking a real build
- `source/registration_com_valkaari_hot4D.hxx`
- `source/OceanSimulation/OceanSimulation_decl1.hxx`
- `source/OceanSimulation/OceanSimulation_decl2.hxx`

These appear to be generated by the Cinema 4D framework project tooling and must be produced from the real SDK environment.

## Safe improvements already suitable before SDK access
- Keep repository documentation focused on generated-file blockers and likely host integration hotspots.
- Preserve upstream naming and attribution.
- Avoid speculative rewrites of falloff, MoGraph, or framework APIs without compiler evidence.
- Prefer low-risk hygiene only: warnings, typo-level docs, initialization safety, and numerically defensive helpers.

## Recommended first SDK-enabled step
1. Generate the missing framework files in a real Cinema 4D 2026+ SDK checkout.
2. Build once without broad refactors.
3. Triage compiler errors into buckets:
   - generated/framework registration
   - deformer/falloff/fields
   - effector/MoGraph
   - resource/description mismatches
   - numeric/core simulation
4. Fix only one bucket at a time.

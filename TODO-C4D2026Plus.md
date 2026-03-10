# C4D 2026+ port TODO

## Hard blockers

- [ ] Build against an actual Cinema 4D 2026 and later SDK checkout.
- [ ] Generate missing framework/interface headers:
  - [ ] `source/registration_com_valkaari_hot4D.hxx`
  - [ ] `source/OceanSimulation/OceanSimulation_decl1.hxx`
  - [ ] `source/OceanSimulation/OceanSimulation_decl2.hxx`
- [ ] Confirm whether additional generated implementation glue is required by the SDK tooling.

## First compile pass

- [ ] Run project generation/build from the SDK environment.
- [ ] Capture the first compiler/linker error set.
- [ ] Verify whether `C4D_Falloff` is still supported as used here, or must be migrated to a newer fields/falloff API.
- [ ] Verify MoGraph effector API compatibility for `EffectorData`, `EffectorStrengths`, and registration flags.
- [ ] Verify `VertexColorTag` read/write API signatures against the 2026 SDK.

## Portability / cleanup

- [x] Normalize a few source include paths to be more robust on case-sensitive filesystems.
- [ ] Decide whether `source/C4D_Object/OceanSImulationDesc.cpp` should be renamed for casing consistency.
- [ ] Add SDK-specific build instructions once the exact 2026 workflow is confirmed.

## Validation

- [ ] Test plugin registration in Cinema 4D 2026 and later.
- [ ] Verify object/deformer UI loads correctly.
- [ ] Verify effector registration and MoGraph interaction.
- [ ] Verify ocean simulation output, jacobian map writing, foam behavior, and field/falloff interaction.



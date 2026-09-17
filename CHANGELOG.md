# Changelog

## 0.4 Alpha — unreleased

### Fixed

- Correct constant-color blend factors that caused the huge white glare at the
  dusk pool. The corrected local view ran at approximately 60 FPS.
- Keep input-history expiry comparisons and branches in one recovered body.
  Two corrected local volleyball matches completed fifteen rallies without the
  delayed crouch seen in the comparison runs, while retaining fresh serving.
- Preserve the recovered Radio Station callback; a local manual run opened it.
- Recover a missing activity callback and its two dependencies associated with
  a reported 0.3 Take a Rest stop. The exact activity variant is uncertain;
  the local release candidate still needs a gameplay check.
- Restore both returns of another Rest predicate reached by the local package
  test. The first candidate stopped at its missing true-return branch. A
  synthetic generation regression now checks that both returns share one body.

### Release and setup

- Include the tested local runtime fixes alongside the current recovery recipe.
- Build with Visual Studio 2022 in x64 Release, matching the working local
  runner. Retain the per-file optimizer workaround without disabling Release
  optimization globally.
- Show FPS and frame time in the window title and log performance once per
  second. Continuous image capture is disabled by default.
- Install into a new folder and rebuild. Preserve the previous saves; setup
  does not migrate them automatically.

### Validation and remaining limits

- Fresh generation through the packaged builder matches all 20 reviewed files.
  The x64 Release build passes ten native tests and eight setup, authentication,
  extraction and launcher tests. Generation reused a verified disc extraction;
  extraction was separately tested with a synthetic ISO.
- User testing of the exact local candidate is pending. Nothing is published.
- Broader Take a Rest variants, complete offline flows, hair rendering, movie
  performance, audio crackle and area-transition slowdown remain open. The recipe still has
  unsupported paths; this is not complete gameplay acceptance.

## 0.3 Alpha — 2026-09-13

### Fixed

- Restore pool-water refraction and reflections by initializing framebuffer
  viewport scales and preserving the guest sample grid. Daytime and dusk
  captures show the floor through the water.
- Add directional character lighting and shaded platform materials. Preserve
  the reached shader working set to avoid Hopping Game cache thrashing.
- Recover reached Hopping Game, Take a rest, Jungle-stage and Options/hotel
  transition paths, including the incomplete dusk pool function.
- Apply Gamma, Brightness and Contrast to the displayed picture.
- Default to Digital Control with new and older saves, while preserving an
  explicit subsequent Analog or Digital choice as a PC preference.
- Correct the inventory action text used for accessory equip and lotion use.
- Ignore keyboard input when the game window is not in the foreground.

### Release and setup

- Update the source and authenticated generation recipe to the tested local
  runtime, retaining the earlier camera, save, audio and movie corrections.
- Keep the existing ISO setup workflow, bundled prerequisite tools and licenses.
- Install into a new folder and rebuild; saves are not migrated automatically.
- Keep checksums internal, with no checksum text attachment or bundled game data.

### Validation

- The local candidate passes all ten native tests.
- Daytime pool-water rendering was observed in two independent natural runs;
  a later manual dusk run reached gameplay and the failure/retry screen, with
  positive player feedback. Its planned time limit ended the run normally.
- Fresh generation through the public builder matches all 20 tested local files
  byte-for-byte. The public Win32 Release rebuild passes ten native tests; seven
  setup/authentication/extraction/launcher checks and five focused lifter tests
  pass. Generation reuses a verified game extraction; the extractor is separately
  checked with a synthetic ISO. No fresh package gameplay run was performed.
- Reused natural-run evidence applies to matching runtime and generated inputs;
  it does not establish complete gameplay acceptance for the package.

### Known limitations

- Complete Hopping Game outcomes, dusk return/repeat coverage, other time and
  activity variants, sustained performance and original rendering parity remain
  unproven. Lighting, camera and image coverage are still incomplete.
- Casino and collection routes still have known unsupported paths. Inventory
  persistence and all item/action variants are not fully accepted.
- Physical two-player play and comprehensive pressure/action outcomes remain
  unproven. Rollback multiplayer is not integrated into gameplay.

## 0.0.2 Alpha — 2026-09-12

### Fixed

- Include the current local x87 arithmetic correction, which fixes explicit
  floating-point destination registers used by camera calculations. Local
  match and island-return evidence shows reduced abrupt camera swings; full
  original-camera and physical two-player parity remain unproven.

- Restore surfaces in reflective item previews, including visor and lotion
  models, through corrected texture coordinates and texture blending.
- Initialize the graphics transforms to the original identity defaults.
- Handle the observed padded item vertex layout without reading a missing
  texture-coordinate field. Unsupported resource layouts remain rejected.

### Release and setup

- Rebuild with the current local runtime and generation recipe, including the
  camera arithmetic correction. Preserve the proven function boundaries,
  ordered recovery inputs and generated-file checks.
- Include the accepted ISO setup workflow in the tagged source, alongside
  the bundled-prerequisite Alpha ZIP.
- Refresh setup and project documentation and add a Ko-fi support link.
- Omit checksum text files from the download. Setup still verifies its
  recipe and generated program internally.

### Validation

- Win32 Release rebuild passed all nine native runtime tests.
- Seven setup, generation-authentication, extraction and launcher checks passed.
- Fresh generation through the packaged recipe matches all 20 current local
  generated files byte-for-byte. This used a verified existing ISO extraction;
  no new full ISO extraction or gameplay acceptance run was performed.

### Known limitations

- This is an experimental Alpha. Lighting, camera behavior, other missing
  images and complete offline-mode coverage remain under development.
- The existing unresolved-function stop in a later island-menu flow remains.
- Selected item previews have natural-run evidence in development builds;
  that evidence does not establish complete gameplay acceptance for this ZIP.
- Rollback multiplayer is not integrated into gameplay.

### Packaging correction

The first 0.0.2 upload mistakenly retained the older 0.0.1 generated program.
The corrected package matches the current local generation, including the
camera arithmetic fix. Rebuild from the corrected package if you used the
initial upload.

## 0.0.1 Alpha

The initial Alpha introduced local ISO extraction, authenticated generation
and a Win32 Release build, with native rendering, input, audio, movies and
save support. Its named ZIP was updated to use the accepted generation recipe;
the original Git tag did not contain that complete setup workflow.

The 0.0.2 changes above are compared with the final published 0.0.1 Alpha ZIP,
not just the older source tag. Features already in that ZIP are not counted as
new fixes here.

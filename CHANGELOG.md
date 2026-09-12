# Changelog

## 0.0.2 Alpha — 2026-09-12

### Fixed

- Restore surfaces in reflective item previews, including visor and lotion
  models, through corrected texture coordinates and texture blending.
- Initialize the graphics transforms to the original identity defaults.
- Handle the observed padded item vertex layout without reading a missing
  texture-coordinate field. Unsupported resource layouts remain rejected.

### Release and setup

- Rebuild with the current runtime while retaining the accepted 0.0.1
  generation recipe, lifter patch, recovery inputs and generated-file checks.
- Include the accepted ISO setup workflow in the tagged source, alongside
  the bundled-prerequisite Alpha ZIP.
- Refresh setup and project documentation and add a Ko-fi support link.
- Omit checksum text files from the download. Setup still verifies its
  recipe and generated program internally.

### Validation

- Win32 Release rebuild passed all nine native runtime tests.
- Seven setup, generation-authentication, extraction and launcher checks passed.
- All 20 preserved generated files match the unchanged receipt. This release
  reused that authenticated program; it did not perform a fresh full ISO build
  or a new gameplay acceptance run.

### Known limitations

- This is an experimental Alpha. Lighting, camera behavior, other missing
  images and complete offline-mode coverage remain under development.
- The generation baseline is unchanged. Later generation-dependent camera
  corrections are not included in this release.
- The existing unresolved-function stop in a later island-menu flow remains.
- Selected item previews have natural-run evidence in development builds;
  that evidence does not establish complete gameplay acceptance for this ZIP.
- Rollback multiplayer is not integrated into gameplay.

## 0.0.1 Alpha

The initial Alpha introduced local ISO extraction, authenticated generation
and a Win32 Release build, with native rendering, input, audio, movies and
save support. Its named ZIP was updated to use the accepted generation recipe;
the original Git tag did not contain that complete setup workflow.

The 0.0.2 changes above are compared with the final published 0.0.1 Alpha ZIP,
not just the older source tag. Features already in that ZIP are not counted as
new fixes here.

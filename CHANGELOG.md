# Changelog

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

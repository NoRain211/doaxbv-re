# Host-shell redistribution boundary

Status: resolved research for “Set the host-shell redistribution boundary.”

This is a source review and project-policy recommendation, not legal advice. It
does not inspect, reproduce, or disclose game binaries, generated game C,
assets, extracted filenames, BIOS data, saves, or private runtime evidence.

## Decision

Use a two-custody boundary:

1. **Public custody:** project-authored source and documentation, host runtime
   and importer interfaces, build and test scripts, public-safe fixtures,
   license notices, and metadata that does not encode game-derived material.
2. **Private custody:** the user's game copy and XBE, generated or transformed
   game C, binaries or snapshots containing game code, extracted assets or
   filenames, BIOS/EEPROM/save data, private tool databases, and identifying
   runtime evidence.

The public project distributes a source-buildable host shell that accepts a
locally supplied input. Releases must not contain the game executable,
generated game logic, assets, or an artifact whose purpose is to supply that
missing game content. This is a conservative custody policy, not a legal
conclusion about every possible transformed artifact.

## Basis

The project is GPL-3.0-or-later and explicitly excludes rights to the game and
third-party material. GPLv3 defines Corresponding Source and the obligations
that apply when covered source or object code is conveyed. It also distinguishes
aggregates of separate works from combined works, while preserving other legal
obligations. See the [GNU GPL v3](https://www.gnu.org/licenses/gpl.html).

U.S. copyright law includes computer programs as copyrightable subject matter
but excludes ideas, procedures, systems, and methods of operation. Its narrow
owner-based rule for essential-step and archival copies does not establish a
general right to redistribute a game or game-derived output. See
[17 U.S.C. section 102](https://www.copyright.gov/title17/92chap1.html#102),
[17 U.S.C. section 117](https://www.copyright.gov/title17/92chap1.html#117),
and the [Copyright Office digital-files FAQ](https://www.copyright.gov/help/faq/faq-digital.html).

These sources do not classify this project's specific transformed artifacts.
That question depends on their contents, provenance, technical relationship to
the original, applicable agreements, and jurisdiction.

## Project policy

| Artifact class | Disposition | Rule |
| --- | --- | --- |
| Project-authored runtime, adapters, device models, importer, and hand-written port code | Public | Carry the applicable project license and notices. |
| Public build scripts, tests, interfaces, schemas, and synthetic fixtures | Public | Fixtures must be demonstrably non-game-derived. |
| Generic third-party libraries or headers | Review individually | Preserve upstream licenses and notices; do not imply the project relicenses them. |
| XBE, generated game C, snapshots, or binaries containing game code | Private | Never add them to public Git, releases, issues, or examples. |
| Extracted assets or filenames, BIOS/EEPROM, saves, and identifying traces | Private | Keep them local unless separately cleared for publication. |
| Architecture and workflow documentation | Public description only | Use abstract examples and public-safe paths; disclose no private bytes or identifying metadata. |

“Host shell” is a source boundary, not a redistribution claim about the input
it consumes. Documentation may explain that a locally obtained input is
required, but project infrastructure must neither provide that input nor imply
that the project license covers it.

## Residual uncertainty

- “Locally derived” is a custody label, not a legal category. Generated output,
  hashes, logs, screenshots, and filenames still require artifact-level review.
- GPL obligations for project code do not answer separate game-license,
  platform-SDK, codec, patent, contract, or jurisdiction questions.
- Whether a release is an aggregate or combined work depends on facts beyond
  this repository policy.

Before distributing anything beyond this source-only boundary, obtain legal
review of the exact artifact, its provenance, every third-party component and
notice, and the intended jurisdictions.

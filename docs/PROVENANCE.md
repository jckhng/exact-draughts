# Provenance And Licensing

Kindle Draughts is an unofficial Kindle-focused adaptation of draughts/checkers
using artwork from the open source Draughts project by Thiago Fernandes
(`tobagin`):

- Upstream project: <https://github.com/tobagin/Draughts>
- Upstream license: GNU GPL version 3 or later
- Upstream project description: a GNOME draughts/checkers game

This is not part of the original GNOME Games codebase. The credit and license
lineage for the draughts assets belongs to the Draughts project above.

## What Comes From Draughts

- Checker and king PNG artwork copied into `assets/`.
- Project lineage and GPLv3-or-later license basis.

## What Is Kindle-Specific

- GTK2/Cairo Kindle interface in `main.c`.
- Standalone draughts rules engine in `draughts_engine.c`.
- KUAL extension scripts and Mesquite window title.
- Docker ARM build and release packaging scripts.

## License Notes

The included Draughts-derived material remains under Draughts' upstream
GPL-3.0-or-later license. GPL license texts are included in `licenses/`.
Runtime libraries bundled into binary release packages keep their own upstream
licenses and are listed inside the generated package under `LICENSES/`.

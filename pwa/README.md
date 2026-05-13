# Exact Draughts PWA

An installable browser Draughts (Checkers) game in grayscale e-ink style.

## Features

- English (8×8) and International (10×10) variants.
- Play as Light pieces against the built-in AI (Easy/Medium/Hard).
- Alpha-beta search AI engine.
- 2-player local and AI demo mode.
- Mandatory captures enforced.
- Multi-jump capture sequences.
- King promotion.
- Undo moves.
- Save/Load a manual restore point.
- Works offline after first load.
- Installable via Chrome "Add to Home Screen."

## Building

```bash
npm install
npm run typecheck
npm run build
```

## Rules

Click a piece to select it (valid pieces are highlighted). Click a target square
to move. Captures are mandatory. Kings are marked with K.

Light pieces move upward, Dark pieces move downward.

## Attribution

Rules engine ported from the Exact Draughts native Kindle implementation.
Part of the Exact Games / GnomeGames4Kindle project.

## License

GPL-3.0-or-later. See THIRD_PARTY.md for dependency details.

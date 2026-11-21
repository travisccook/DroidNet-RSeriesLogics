# Bench list for R2 on the logic displays (contract v1.4)

What the bench has to show for this fork's part of R2 (Gate 2 in the Signal Booster's plan
`docs/superpowers/plans/2026-11-21-lighting-wave-2.md`), on the R2 line's image. Gate 1, R1's own list, comes
first and is unchanged. Nothing below has been run: every row's result is empty until it is. Until then the words for
R2 are host-tested, modelled and cross-compiled (see [../DROIDNET_FORK.md](../DROIDNET_FORK.md), "What is in
(contract v1.4, R2)").

Each check is made beside the stock look where there is one. The numbers are the plan's.

## The logic displays

| # | Check | What the host side expects | Result |
| - | ----- | -------------------------- | ------ |
| 1 | `A:i=logic` beside the idle blink: not told apart by eye, front and rear | The same light: `updateDisplay(fBri)` on the same walk (stage 2g: no LED differs from `native:0` with the same seed and brightness) | |
| 2 | Idle to `logic` and back: no LED jumps; palette, pace and brightness are the owner's afterwards | No LED changes at the hand-over either way; the engine's live settings are the RAM default object's afterwards | |
| 3 | `logic` under `am=2,m=200` at 90, 120 and 176 BPM: the beat reads on the front; say how soft it is on the rear | At 176 BPM the pump takes every other beat (`FX_LOGIC_PUMP_MIN_MS`, 400 ms); each LED takes the level at its next step, up to 400 ms late on the rear | |
| 4 | `logic` with `c=` red, amber, ice white (own colours expected) and magenta: the hue is the one asked for | Ice white is a colour with no hue: the display's own palette. The others: the front palette's shape at `fxLogicHue(c) - 170` | |
| 5 | `s=0`, `128`, `255`: slower, stock, and the fastest on offer; nothing strobes | Front fade/delay 4/40, 1/10, 1/10; rear 12/160, 3/40, 1/20 | |
| 6 | Front on even beats and rear on odd: the trade reads | `an=2`, `ap=0` and `ap=1` | |
| 7 | `M:v=idle`, `X`, the watchdog and a button during `logic`: each as in R1 | Idle and the watchdog: the walk goes on into the idle blink. `X`: dark at once. A button: its sequence, and the show ends on that display | |

Also to note at rows 2 and 7: after `logic`, a contract look (or an accent) comes 340 ms late, the walk's last frame
held meanwhile, and an accent shorter than that is not shown ("What is not in" in `DROIDNET_FORK.md`). Say whether
the hold shows.

## Rows for all the boards, made on this one too

| # | Check | Result |
| - | ----- | ------ |
| 15 | A full song from Studio at release 2, filmed, beside its filmstrip | |
| 16 | A dome with one board left on R1: Studio plans it as R1 and the show plays | |
| 17 | Seed-to-light latency again with `logic` up (the engine's walk is in the frame path now) | |

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
| 1 | `A:i=logic` beside the idle blink: not told apart by eye, front and rear | The same light at every `b=`: the palette baked at the level and scaled by it again, as `native:0` at that `b=` is drawn (stage 2g: frame for frame the idle blink's at `b=255`, `128` and `200`) | |
| 2 | Idle to `logic` and back: no LED jumps; palette, pace and brightness are the owner's afterwards | With `logic` unpumped and in its own colours at the idle blink's level: seamless, the walk goes on, no hold either way. With a pump, a colour or another level: the frame that is up holds about 400 ms each way (39 to 42 frames in the mock), then the other look; nothing flickers. Say whether that pause shows. The engine's live settings are the RAM default object's afterwards | |
| 3 | `logic` under `am=2,m=200` at 90, 120 and 176 BPM: the beat reads on the front; say how soft it is on the rear | At 176 BPM the pump takes every other beat (`FX_LOGIC_PUMP_MIN_MS`, 400 ms); each LED takes the level at its next step, up to 400 ms late on the rear | |
| 4 | `logic` with `c=` red, amber, ice white (own colours expected) and magenta: the hue is the one asked for | Ice white is a colour with no hue: the display's own palette. The others: the front palette's shape at `fxLogicHue(c) - 170` | |
| 5 | `s=0`, `128`, `255`: slower, stock, and the fastest on offer; nothing strobes | Front fade/delay 4/40, 1/10, 1/10; rear 12/160, 3/40, 1/20 | |
| 6 | Front on even beats and rear on odd: the trade reads | `an=2`, `ap=0` and `ap=1` | |
| 7 | `M:v=idle`, `X`, the watchdog and a button during `logic`: each as in R1 | Idle and the watchdog: the walk goes on into the idle blink. `X`: dark at once. A button: its sequence, and the show ends on that display | |

Also to note at rows 2, 3 and 7: after `logic`, a contract look (or an accent) comes 400 ms late, the walk's last frame
held meanwhile, and an accent shorter than that is not shown; a recolour of `logic` waits up to 400 ms; a rise of its
level that is not its pump's beat (`B`, a pause and resume) up to 500 ms ("Its windows" in `DROIDNET_FORK.md`). Say
whether any of the holds show.

## Rows for all the boards, made on this one too

| # | Check | Result |
| - | ----- | ------ |
| 15 | A full song from Studio at release 2, filmed, beside its filmstrip | |
| 16 | A dome with one board left on R1: Studio plans it as R1 and the show plays | |
| 17 | Seed-to-light latency again with `logic` up (the engine's walk is in the frame path now) | |

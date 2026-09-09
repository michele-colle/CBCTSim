# Task: find the dental (alveolar) arch Z in an axial CT montage

You are given ONE image path. Use the Read tool to view it. It is a montage of
axial slices through a human head; each panel is titled with its Z coordinate
in mm.

## Orientation of each panel
- horizontal axis = X, patient left is +X
- vertical axis = Y, **+Y is POSTERIOR**. Panels are drawn origin-lower, so the
  **BOTTOM of each panel is ANTERIOR** (face/chin) and the TOP is POSTERIOR
  (back of head / neck).

## Colour key (segmented label map, NOT greyscale CT)
| colour | tissue |
|---|---|
| near-black / very dark navy | air |
| dark brown | fat |
| pink / salmon | soft tissue |
| tan-yellow | spongiosa (trabecular bone) |
| cream-white | cortical bone |
| bright red | implant (already placed) |
| cyan / teal bar at the far edge | the stretcher — ignore it entirely |

## What to find
The **dental / alveolar arch**: the horseshoe (U-shaped) band of bone in the
ANTERIOR part of the head — a cream-white cortical shell around a tan-yellow
spongiosa core — with the U opening POSTERIORLY (closed curve of the U near the
bottom/anterior of the panel, limbs running up/posteriorly). This is the
tooth-bearing jaw bone.

Prefer a slice where the arch is a single clean complete horseshoe with a
reasonable wall thickness, and where the bone is well inside the soft tissue
(not flush against the skin).

## Do NOT confuse it with
- **cervical spine / vertebra** — sits POSTERIOR (top of panel), roughly round
  or oval with a central canal; not a wide anterior horseshoe.
- **zygomatic arches** — thin bone struts far out LATERALLY (large |X|), a much
  wider overall span, not a closed anterior U.
- **truncation cut** — bone flush against the outer skin edge with no soft
  tissue covering it, often with an unnaturally straight or diagonal boundary.
  Avoid these slices.
- **hard palate** — a flat bone plate spanning the midline; the alveolar arch
  is around/below it.
- **basal mandible** — the bulky lower body of the jaw; the alveolar ridge is
  the thinner arch above it.

## Answer format
End your reply with exactly these three lines:

    ARCH_Z=<number in mm, e.g. -80.0>
    ARCH_Z_ALT=<your second-best slice, or NONE>
    CONFIDENCE=<high|medium|low>
    REASON=<one short sentence: what you saw and where in the panel>

ARCH_Z_ALT matters: the first pick is sometimes a few mm too superior, landing
the implants in soft tissue. Give the next-best candidate so it can be tried
without re-running you.

Use the Z of the panel that best shows the arch, or interpolate between two
panels. If NO panel shows a usable dental arch, answer `ARCH_Z=NONE` with a
reason. Be honest about low confidence rather than guessing.

## Efficiency (important)
View the image **once** with the Read tool, then answer. Do NOT crop, re-render,
zoom, re-read the image, write files, or run any other tooling — a single Read
followed by your answer is the whole job. Budget: 1-2 tool calls total.

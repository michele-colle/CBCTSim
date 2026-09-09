# implant_tools — dental-implant augmentation of existing MC-GPU volumes

Produced the 26-volume `_imp` set (see `../IMPLANT_RUN_RESULTS.md`,
`../PLAN_implant_variants.md`). Stamps titanium cylinders (label 5) into the
alveolar arch of an already-built volume — no rebuild from DICOM.

Run from this folder with `/home/colle/miniforge3/bin/python` (numpy lives in
that conda env, not the system python).

| File | Role |
|---|---|
| `prep_case.sh <case>` | ensure base + profile, render the arch montage for the visual pick |
| `AGENT_BRIEF.md` | brief handed to the Sonnet agent that reads the montage and returns `ARCH_Z` + `ARCH_Z_ALT` |
| `scorez.sh <case> <z...>` | accepted-site count + mean bone fraction per candidate Z |
| `auto_finish.sh <case> <z...>` | score the candidates, build at the best |
| `finish_case.sh <case> <z> [--small]` | place -> stamp -> verify -> review PNG -> compress -> prove -> archive to H:, delete local raw |
| `place.py` | fit the alveolar arch centreline, sample sites, score bone/soft/air |
| `stamp.py` | copy the base, patch only affected slices, report what was replaced |
| `view.py` / `montage.py` | sagittal/axial/coronal and axial sweeps, axes in isocenter mm |
| `dump_profile.py` / `arch_table.py` | per-slice profiles + offline arch-Z candidates |
| `usage_check.sh` | reads the Windows usage monitor log; used to pace long runs |

## Gotchas learned the hard way
- **The arch is anchored to the VERTEX, not the volume floor.** Floor-based
  detection locks onto the truncation cut or the neck. Vertex-to-arch measured
  165 +- 16 mm over 86 cases, so the search window is `[vertex-190, vertex-125]`.
- **The alveolar crest is NARROWER than the zygomatic level above it.** A
  "widest arch" rule picks the zygoma and lands ~24 mm too high.
- **The agent's first pick is often a few mm too superior** on patient CT
  (mandible body / neck read as arch). Always score its `ARCH_Z_ALT` too — it
  won in roughly a third of cases.
- **Gate air absolutely, not per-implant.** A cylinder 35% in air passed a 5%
  per-implant threshold; and 172 air voxels spread over 3 implants hid under it.
  Per-implant air is now 0.002, plus an absolute "air count unchanged" check
  with a small boundary tolerance. Carbon/foam must be exactly unchanged.
- **Implants are irreversible** (unlike the stretcher inpaint, which is
  bit-exact reversible because the shell only sat in air). Always derive from a
  pristine base; `stamp.py` copies first and never edits a base.
- `.txt` companions come from the source dir on H: — the archives hold the
  `.raw` only.

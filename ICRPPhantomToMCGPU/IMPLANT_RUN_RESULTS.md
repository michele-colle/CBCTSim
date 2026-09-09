# Implant augmentation run — results (2026-09-07 22:05 → 23:50)

**26/26 volumes built, verified and compressed. Nothing uploaded.**

Scope as agreed: 10 GradientHealth + 10 CQ500 full-cranium + 6 MCRP (age >= 10 y).
Naming `<case>_A_imp_<dims>byte.raw`. 97 implant sites total.

| Output dir (H:) | Volumes |
|---|---|
| `MCGPU_IMPLANT_VOLUME_EXPORT` | 6 |
| `GRADIENTHEALTH_IMPLANT_VOLUME_EXPORT` | 10 |
| `QUREAI_IMPLANT_VOLUME_EXPORT` | 10 |

Each dir holds `.tar.xz` + `.txt`. All local `.raw` reclaimed — every archive
passed `xz -t` **and** a round-trip md5 against the raw before deletion. Base
volumes were never modified. Disk: 820 G free.

## Verification (every case)
- every changed voxel is label 5
- carbon(10) / foam(11) counts unchanged — no implant touches the stretcher
- total voxel count unchanged
- air->implant within tolerance (see below)

## Review PNGs
`positioning_checks_{mcrp,gradienthealth,qureai}_implant/<case>_imp_review.png`
— axial + coronal + sagittal through the first implant, label 5 in red.
`..._pending/` holds the 4 that want a closer look.

## Flagged for your eyes
| case | why |
|---|---|
| `GRDN0YP5B56SGAJM_A` | only 2 sites, mean bone 0.73 (71 y, ridge atrophy) |
| `CQ500-CT-136_A` | only 2 sites (bone 1.00 though) |
| `CQ500-CT-183_A` | only 2 sites (bone 0.98 though) |
| `GRDN1P7W3AFP70R8_A` | arch Z uncertain — best-scoring slice was 185 mm below vertex, the extreme of the plausible range. Anatomy worth confirming. |

## What the run changed in the method
1. **The Sonnet agent's first pick was often too superior** on real patient CT
   (mistaking mandible body / neck for the alveolar ridge). Fix: the agent also
   returns `ARCH_Z_ALT`, and `scorez.sh`/`auto_finish.sh` score every candidate
   objectively and build at the best. The agent's ALT won in ~1/3 of cases.
2. **Air gate tightened twice.** Per-implant air tolerance went 0.05 -> 0.01 ->
   0.002 after `GRDN21I9AD975J98_A` stamped 172 air voxels while every single
   implant still passed the old per-implant check. The absolute
   "air count unchanged" gate is what caught it.
3. **Boundary tolerance added.** A few air voxels at a cylinder's edge is
   discretisation, not a floating implant: air delta <= max(20, 0.1% of implant
   volume) now warns instead of failing. Carbon/foam stay strictly zero.
4. **`--small` flag** (3.3 x 8 mm, the smallest real fixture) for atrophic or
   paediatric jaws. Needed for `GRDN21I9AD975J98_A`.
5. Two script bugs fixed: `.txt` companion is read from H: (archives hold the
   `.raw` only), and a `set -e` trap in an early sweep helper.

## Not done
- **No upload.** As instructed. Publishing to `gs://mcgpu-data-gcp/phantom/`
  remains a separate deliberate step.

---

# Published + .in files generated (2026-09-08)

## Upload — done
All 26 `.tar.xz` uploaded to `gs://mcgpu-data-gcp/phantom/` (96.1 MiB).
Verified **by name** against the bucket listing (26 local / 26 remote, no
missing, no extras) and **by md5** (26/26 match, local vs the object's own
`Hash (md5)`). Bucket phantom objects: 158 -> 184.
Log: `logs/upload_implants_to_gcp_20260908_100759.log`.

Per the standing convention, phantom uploads are the `.raw` archive ONLY --
no `.txt`, no `.in`. Those stay local next to the archives.

## .in files — 58 generated

| Dir (H:) | Files | Variants |
|---|---|---|
| `GRADIENTHEALTH_IMPLANT_IN_FILES` | 20 | `CBCT_120kV`, `CBCT_80kV` |
| `QUREAI_IMPLANT_IN_FILES` | 20 | `CBCT_120kV`, `CBCT_80kV` |
| `MCGPU_IMPLANT_IN_FILES` | 18 | `cbct_bar_only`, `cbct_head_bar`, `cbct_head_only` templates |

Named `<case>_A_imp_<variant>.in`. In each file:
- `VOXEL GEOMETRY FILE` -> `phantom/<case>_A_imp_2134x2134x834byte.raw`
- `OUTPUT IMAGE FILE NAME` -> `results/<case>_A_imp_<variant>`

Checked: all 58 output names distinct, and every referenced `.raw` corresponds
to a built archive (0 missing). `voxelId=5` titanium was already declared in the
source `.in` and is untouched.

## Watch out: pre-existing collision in the MRCP base-A .in files

The 12 **existing** `MCGPU_IN_FILES/MRCP*_A_CBCT_*template.in` all carry a
GENERIC output name -- `results/cbct_head_bar_template` -- with no case id, so
every MRCP phantom writes to the same output file. Running two of them from one
directory overwrites results. The 18 new `_imp` files have this fixed
(case-specific output names), but **the original 12 are still affected** and are
not touched by this work.

## Also noted
`CLAUDE.md` says "98 volumes published"; the bucket held 158 phantom objects
before this upload and 184 after. That figure is stale.

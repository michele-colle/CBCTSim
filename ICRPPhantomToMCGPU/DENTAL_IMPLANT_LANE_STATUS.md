# Dental-implant phantom lane — status

**Goal**: a published set of MC-GPU volumes carrying dental implants (label 5,
titanium) inside the CBCT reconstructed FOV, for metal-artefact / scatter work.

**Status 2026-09-08: 26 nose-centred base volumes built, verified and archived.
NOT published. Implants NOT yet stamped — that is the next step.**

Related: [PLAN_implant_variants.md](PLAN_implant_variants.md) (original design),
[IMPLANT_RUN_RESULTS.md](IMPLANT_RUN_RESULTS.md) (the abandoned round-8 set),
[implant_tools/](implant_tools/) (all tooling + its own README).

---

## 1. What happened, in order

| Round | What | Outcome |
|---|---|---|
| 8 | 26 volumes with implants stamped into the **published** bases, uploaded to `gs://.../phantom/` | **Scrapped.** The teeth sat below the FOV floor, so every implant was outside the reconstructed FOV. All 26 objects deleted from GCS and locally. |
| 9 | Re-placed every volume so the mid-face is at the FOV centre, then rebuilt from DICOM | **26/26 bases done** (this document). |

Round 8's failure was not a placement bug — it was the *box* placement being
wrong for this purpose. `auto_head_placement` anchors the **head tip**, which
puts the dentition near the bottom of the box and below the beam.

## 2. The geometry that matters

```
fovHalfZ = (Dz/2) * (SAD/SDD) = (29.34/2) * (57.8/98.70) = 85.9 mm
```

So the CBCT reconstructed FOV is **Z ∈ [-85.9, +85.9] mm** about isocenter,
inside a box that is Z ∈ [-125.1, +125.1]. Same in all three beam templates
(`cbct_head_bar`, `cbct_bar_only`, `cbct_head_only`).

Placement knob (`dicom_to_mcgpu.cpp` ~line 1330):

```
targetTipIso = fovHalfZ - head_top_margin_mm
```

=> changing the margin by D shifts all anatomy by -D. Because round 8 used the
default `margin = 10`, the round-9 value for each case is simply

```
head_top_margin_mm = 10 + <z of the landmark, measured in the round-8 volume>
```

which is why every case landed on target to 0.1 mm: it is a *relative* change
from a known build, so it is immune to `detectHeadHU` quirks.

## 3. The placement rule actually used

In priority order, per case:

1. **Nose at isocenter** — the default. 22 of 26.
2. **Last DICOM slice at the FOV bottom** — when the source series stops above
   the chin, so nose-centring would leave part of the FOV empty of material.
   Only `CQ500-CT-127` (source ends at z≈-81; both of its series checked).
3. **Dental arch at ≈ -29** — when the face is **defaced** (anonymised) and
   therefore has no nasal tip at all. `CQ500-CT-298`, `GRDN02BKET588MC5`.
   -29 is where MRCP_AM's arch lands naturally with its nose at 0, so this
   reproduces the same FOV framing without needing a nose.

The vertex is deliberately allowed to clip out of the box top (up to ~43 mm on
some cases). That trade was taken knowingly: it costs some skull as a scatter
source but buys back far more neck/shoulder material at the bottom, which the
old head-tip anchoring was discarding, and the dental FOV is what matters here.

## 4. Output

| | |
|---|---|
| Archives | `/mnt/h/MICHELE_MCGPU/IMPLANT_V2_BASE` — 26 `.tar.xz` + 26 `.txt`, 116 MB |
| Renders | `positioning_checks_implant_v2/` — 26 sagittal FOV checks |
| Cfgs | `params/generated_implant_v2/` — 26, each with its `head_top_margin_mm` and a comment explaining it |
| Manifest | `batch_jobs_implant_v2_exported_raw.txt` — case, landmark z, margin, archive |
| Driver | `implant_tools/build_v2.sh` (resumable: skips any case already in the manifest) |

Every `.raw` was reclaimed only after its archive passed `xz -t` **and** a
round-trip md5 against the raw. Peak disk was one 3.8 GB raw at a time.

## 5. Per-case margins

`nose_z` is the landmark measured in the round-8 volume; `margin = 10 + nose_z`.

| case | nose_z | margin | note |
|---|---|---|---|
| MRCP-10F / 10M / 15F / 15M / AF / AM | -50.1 / -52.8 / -56.0 / -60.6 / -79.2 / -64.8 | -40.1 / -42.8 / -46.0 / -50.6 / -69.2 / -54.8 | all nose-centred, user-verified correct |
| CQ500-CT-136 / 172 / 183 / 241 / 266 / 305 / 307 / 325 | -66.6 / -64.5 / -64.5 / -49.5 / -78.9 / -37.5 / -71.7 / -6.6 | -56.6 / -54.5 / -54.5 / -39.5 / -68.9 / -27.5 / -61.7 / +3.4 | nose-centred |
| **CQ500-CT-127** | -57.8 | **-47.8** | rule 2: last slice at FOV bottom (floor -86.1). Nose ends 30.9 mm low — unavoidable, source stops at the chin |
| **CQ500-CT-298** | -84.5 | **-74.5** | rule 3: defaced. Arch measured at -68, shifted +38.6 -> -29.4. Floor -97.5 |
| GRDN00 / 0V8 / 0XO / 0YP / 17Y / 1TQ / 21I / 2SA | -42.3 / -54.9 / -58.2 / -41.7 / -65.7 / -51.0 / -54.9 / -37.8 | -32.3 / -44.9 / -48.2 / -31.7 / -55.7 / -41.0 / -44.9 / -27.8 | nose-centred |
| **GRDN02BKET588MC5** | -81.0 | **-71.0** | rule 3: defaced. Arch -72 -> -40. User-confirmed correct |
| GRDN1P7W3AFP70R8 | -15.0 | -5.0 | **UNRESOLVED — see §7** |

Cases where the FOV bottom is not fully covered by tissue (accepted as-is per
user review, "just a few slices"): `CQ500-CT-183` +6.7 mm, `CQ500-CT-307`
+6.1 mm, `CQ500-CT-266` +1.0 mm.

## 6. Gotchas found in this lane

- **`fovHalfZ` is 85.9 mm, not the box half-height.** The box is 125.1 mm; the
  beam only reconstructs 85.9. The 39.2 mm difference is exactly what made
  round 8 fail.
- **Several CQ500 and GradientHealth scans are DEFACED** (anonymised by
  shaving the face off). Signature: the anterior contour has a long *flat
  plateau* instead of a nasal peak, and the most-anterior point becomes the
  glabella or a surviving lip. Confirmed on `CQ500-CT-298`,
  `GRDN02BKET588MC5`, `GRDN1P7W3AFP70R8`. **The nose is unusable as a landmark
  on these** — anchor on the dental arch instead.
- **"Most anterior voxel" is NOT the nose** in three situations: the series
  includes the chest (shoulder wins), the patient has a prominent chin
  (`GRDN1TQOUC2B1R2U` — its placement is correct, only the *detector* is
  wrong), or the scan is defaced. Sanity check with **vertex-to-nose = 120-145
  mm**; outside that, look at the render.
- **Detect the nose in the built VOLUME, not the source DICOM.** The volume
  contains only the head, so the shoulder trap disappears. Three successive
  attempts at DICOM-space detection all failed.
- **Do not trust a subagent's arch reading on a defaced case.** One reported
  `CQ500-CT-298`'s arch at -64 when it was at -92 (28 mm out). Individual
  tooth crowns are obvious in an axial montage — measure them directly.
- **`fovcheck.py` must be run with `--as-is` for QA of a built volume.**
  Without it the render is a *preview of a further shift*, which silently
  shows the wrong picture for any volume whose detected landmark is not
  already at the target. This produced two false "wrong placement" reports.
- **Never `pkill -f <pattern>` where the pattern matches your own command
  line** — it kills the shell running it (exit 144) and everything after that
  line silently does not run. Bit twice in this lane.
- Build time is **~6.7 min/volume**, not the 1.9 min in the main README —
  despeckle dominates on these.

## 7. Open items

1. **`GRDN1P7W3AFP70R8` is unresolved.** Defaced *and* its vertex is cropped in
   the source, so neither landmark is trustworthy. It was built with a small
   shift (margin -5.0) off a nose reading that is almost certainly the
   glabella. Its arch has never been measured. **Check the axial montage and
   re-anchor on the arch before publishing.**
2. **Implants not yet stamped.** Next step is the round-8 implant pass
   (`implant_tools/prep_case.sh` -> arch montage -> `auto_finish.sh`) run
   against `IMPLANT_V2_BASE`. The arches now sit near -29 instead of below the
   FOV floor, so placement should be much cleaner.
3. **Nothing published.** `gs://mcgpu-data-gcp/phantom/` currently holds 158
   objects and none of this lane. Decide whether the nose-centred **bases**
   are published alongside the implanted versions — a paired with/without
   implant set at identical geometry is useful for artefact studies, and the
   old published bases are at the wrong Z to serve that purpose.
4. **Naming for the implanted set** — round 8 used `<case>_A_imp_<dims>.raw`.
   Reusable, but these bases are at a different Z than the published `_A`
   volumes, so the set no longer pairs with them.

---

## 8. Credential state

GCP credentials **expired again on 2026-09-09** (`gsutil ls` →
`ReauthUnattendedError`). The bucket was last successfully listed on
2026-09-08 at **158 phantom objects**, after the 26 round-8 implant objects
were deleted. Publishing anything from this lane needs
`gcloud auth login` from the user first.

Note the documented trap fired again while writing this doc: `gsutil ls
... 2>/dev/null` returned `0` objects, which is indistinguishable from an
empty bucket. Always list with stderr visible before believing a count.

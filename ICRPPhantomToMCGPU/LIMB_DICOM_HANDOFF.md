# Limb DICOM pools — handoff for building MC-GPU arm/leg volumes

**Status: DICOM extraction done (104 series); nothing has been run through
`dicom_to_mcgpu` yet.** This is the open work item tracked in `CLAUDE.md`
under "mcgpu volumes for arms and legs".

Two independent pools of single-limb CT DICOM series exist, ready to feed
into `dicom_to_mcgpu` (pipeline #2 in `CLAUDE.md`) the same way a real
patient series would be. Full per-series listing:
[limb_dicom_manifest.csv](limb_dicom_manifest.csv) (104 rows: `pool`, `case`,
`side`, `limb`, `anatomical_extent`, `dir_name`, `path`, voxel `nx/ny/nz`,
`spacing_x/y/z_mm`, and for VSD a `leg_purity_pct` + `notes` column carrying
the known caveats below inline per-row).

## The two pools

| Pool | Path | Series | Source | Anatomy | Table/stretcher |
|---|---|---|---|---|---|
| VSD | `/mnt/h/MICHELE_MCGPU/VSD_dicom/` | 56 | real patient CT (VSD Full Body, Zenodo 8302449) | legs only, mid-thigh → toe tip | present in source, **removed** by extraction |
| MCRP | `/mnt/h/MICHELE_MCGPU/MCRP_dicom/` | 48 | synthetic ICRP/MRCP mesh phantoms (12 ages/sexes) | arms (mid-upper-arm → fingertip) **and** legs (mid-thigh → toe tip) | none — clean synthetic phantom, air background |

Naming: VSD is `<case>_<Left\|Right>Leg_MidThigh-ToToe`; MCRP is
`<phantom>_<Left\|Right><Arm\|Leg>` (phantom names: `MRCP-00F/M`, `-01F/M`,
`-05F/M`, `-10F/M`, `-15F/M`, `MRCP_AF`, `MRCP_AM`). Left/right in both pools
is real anatomical laterality (VSD: from the source segmentation, spatially
resolved not name-trusted; MCRP: resolved per phantom from `Kidney_left`/
`Kidney_right` centroid — see `extract_mcrp_limb_dicom.py` docstring), so a
consuming script can trust the filename.

**Full technical detail on the VSD pool** (method, per-case dataset bugs
found and how each was fixed, quality stats): [VSD_LEG_EXTRACTION.md](VSD_LEG_EXTRACTION.md)
+ [vsd_leg_extraction.csv](vsd_leg_extraction.csv). **MCRP pool method**: see
the `extract_mcrp_limb_dicom.py` docstring and the `CLAUDE.md` staged-datasets
entry — no separate write-up exists yet.

## Known caveats (don't rediscover these)

- **`z036` (VSD) is excluded** — its source `Femur_L/R` segmentation doesn't
  separate the femur from the rest of the pelvis; not in either pool.
- **`z066` (VSD) has a foot truncated at the toes** — the source
  `Shanks-Feet` crop itself ends mid-bone; nothing more exists to recover.
- **`z009/R`, `z023/R`, `z057/L`, `z057/R` (VSD)** have a lower purity score
  (88–91% vs. ~97% mean) but were individually confirmed complete and
  correct via MIP — extra bone stayed attached to the `Femur` label, not
  missing anatomy.
- **MCRP voxel spacing is 1.0mm isotropic, not the 0.3mm production spec**
  in `CLAUDE.md`'s "production volume" table — chosen deliberately to keep
  per-phantom DICOM size reasonable (disk is the standing constraint here).
  If the final MC-GPU volume needs finer resolution, that's `dicom_to_mcgpu`'s
  resampling to handle at its own `voxel_size_mm`, same as it already
  upsamples/downsamples real patient CT of varying native resolution — not
  yet verified end-to-end for these limb series specifically.
- **VSD voxel spacing varies per case** (native CT resolution, ~0.65–1.3mm
  in-plane, 0.5–0.7mm slice) — see the manifest for exact per-series values.

## What the next agent needs to figure out (not yet decided)

`dicom_to_mcgpu`'s existing placement logic (`auto_head_placement`,
FOV-anchored **head** tip, auto-seated stretcher **behind the head**) is
head/CBCT-specific and doesn't apply to a limb. Before batching:

1. **Placement convention for a limb inside the fixed MC-GPU volume** —
   there's no "head tip" to anchor on. Decide whether to center the limb in
   the box, anchor on a bone landmark (e.g. knee/elbow), or add a new
   auto-detection mode. Check `README_dicom_to_mcgpu.md` for whatever manual
   `shift_x/y/z_mm`-style placement already exists as a starting point rather
   than assuming only auto-head-placement is available.
2. **Whether a stretcher/support overlay makes sense for a limb** — real
   extremity CBCT often does rest the limb on some support, but neither pool
   has one baked in (VSD's was removed; MCRP never had one). This is a
   modeling decision, not something the extraction should have guessed at.
3. **Output volume box size** — the current fixed production box
   (2134×2134×834, 0.3mm) was sized for a head; a limb has a very different
   aspect ratio (long and thin vs. round and short). A new fixed box is
   probably needed, chosen once and kept constant like the head one (see
   `CLAUDE.md`: "Keep the box dimensions identical across batches").
4. Per `CLAUDE.md`'s own QA philosophy: **build one case and look at a
   positioning-check render before launching a full batch.** A small case
   (`MRCP-00F_LeftLeg` or `_LeftArm`, smallest in the pool) is the cheapest
   first test once placement logic exists.

## Regenerating or extending a pool

- VSD: `python3 extract_leg_dicom.py --root /mnt/h/MICHELE_MCGPU/zenodo_VSDFullBody/zenodo_8302449 --out-root /mnt/h/MICHELE_MCGPU/VSD_dicom --force` (`--only <case>` for one case; `z036`/`Phantom001` are hardcoded out).
- MCRP: `python3 extract_mcrp_limb_dicom.py --all --out-root /mnt/h/MICHELE_MCGPU/MCRP_dicom` (`--phantom <name> --limb <LeftLeg|RightLeg|LeftArm|RightArm>` for one; needs `LD_LIBRARY_PATH=/opt/Geant4/lib` set to run the `mcrp_to_vox` binary it drives).
- Re-render QA sagittal PNGs for either pool: `python3 make_sagittal_check_pngs.py <pool_dir> --out <pool_dir>_sagittal_checks` (whole-pool call re-renders everything — for a single changed series, call `render_one()` from that module directly instead, see its `__main__`).

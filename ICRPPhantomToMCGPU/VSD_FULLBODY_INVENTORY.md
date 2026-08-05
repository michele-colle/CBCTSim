# VSD Full Body (Zenodo 8302449) — anatomy inventory

Source: `H:\MICHELE_MCGPU\zenodo_VSDFullBody\zenodo_8302449` (`/mnt/h/MICHELE_MCGPU/zenodo_VSDFullBody/zenodo_8302449` from WSL) — **30 cases**, all unzipped, archives removed.
Companion to [TOTALSEGMENTATOR_SELECTION.md](TOTALSEGMENTATOR_SELECTION.md). Machine-readable companion: `vsd_fullbody_inventory.csv`.

Derived **only** from directory/file names, the SMIR `*.json` metadata and the text headers of `*.nrrd` / `*.nii` (segment names, `sizes`, `space directions`). No voxel data was decoded.

Each case unpacks to `<case>/<case>/` holding a `Models/` folder of `.ply` surfaces, a `License CC BY-NC-SA 4.0.txt`, a Slicer scene `.mrml`, a preview `.png`, and one or more CT series folders named `SMIR.<Region>.<Age>Y.<Sex>.CT.<seriesID>`.

## Headline: anatomies per case

The dataset is a **lower-limb / pelvis skeletal** collection. 28 of 30 cases carry exactly the same **21 named bone structures**; only two cases deviate.

| Anatomies | Cases | Which |
|---|---|---|
| **2** | 1 | `Phantom001` |
| **21** | 28 | all except `z064`, `Phantom001` |
| **22** | 1 | `z064` |

- `z064` — 22: the canonical 21 plus a `Minisci` segment (menisci; spelled *Minisci* in the header). It has no `.ply` model, so it exists as a labelmap only.
- `Phantom001` — **not a human**: it is the *European Spine Phantom*, a calibration object. Its two segments (`ESP`, `EuropeanSpinePhantom`) are the same object labelled twice, so it contributes **1 real anatomy**, and it is the only `Vertebra`-region case.

## The canonical 21-structure set

Present in all 29 human cases (`Sacrum` unpaired, the other 20 paired left/right):

| # | Structure | Side | Region |
|---|---|---|---|
| 1 | `Hip` | L + R | pelvis |
| 2 | `Sacrum` | — | pelvis |
| 3 | `Femur` | L + R | thigh |
| 4 | `Patella` | L + R | knee |
| 5 | `Tibia` | L + R | shank |
| 6 | `Fibula` | L + R | shank |
| 7 | `Talus` | L + R | ankle |
| 8 | `Calcaneus` | L + R | hindfoot |
| 9 | `Tarsals` | L + R | midfoot |
| 10 | `Metatarsals` | L + R | forefoot |
| 11 | `Phalanges` | L + R | toes |

That is **11 distinct bones/bone groups → 21 labels**. Nothing above the iliac crest is segmented in any case: no spine (except the phantom), ribs, skull, arms or organs.

## Per-case detail

| Case | Region(s) | Age/Sex | h (m) | w (kg) | Anatomies | `.ply` | Cropped ROIs | Full volume | in-plane x slice (mm) |
|---|---|---|---|---|---|---|---|---|---|
| `002` | Lower_limb + Thorax | 78/F | 1.62 | 75 | **21** | 21 | Pelvis-Thighs, Shanks-Feet, Left_Iliac_Crest | nrrd 512x512x1731; nrrd 512x512x1099 | 0.9766 x 0.9766 x 0.5998 |
| `006` | Lower_limb | 51/F | 1.77 | 90 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nrrd 512x512x1637 | 0.9766 x 0.9766 x 0.6998 |
| `010` | Lower_limb | 45/F | 1.65 | 54 | **21** | 21 | — | nrrd 512x512x997; nrrd 512x512x763 | 0.8262 x 0.8262 x 0.6 |
| `014` | Lower_limb | 30/F | 1.65 | 65 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nrrd 512x512x1782 | 0.9043 x 0.9043 x 0.5999 |
| `015` | Lower_limb | 81/M | 1.75 | 78 | **21** | 21 | — | nrrd 512x512x831; nrrd 512x512x1074 | 0.7656 x 0.7656 x 0.6 |
| `016` | Lower_limb | 95/F | 1.52 | 60 | **21** | 21 | — | nrrd 512x512x1056; nrrd 512x512x722 | 0.873 x 0.873 x 0.6 |
| `017` | Lower_limb | 19/F | 1.7 | 59 | **21** | 21 | — | nrrd 512x512x1020; nrrd 512x512x849 | 0.7129 x 0.7129 x 0.6 |
| `019` | Lower_limb | 56/M | 1.7 | 68 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | — (ROI crops only) | 0.6992 x 0.6992 x 0.6 |
| `023` | Lower_limb | 74/M | 1.82 | 86 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nrrd 512x512x1969 | 0.8828 x 0.8828 x 0.5998 |
| `Phantom001` | Vertebra | 110/O | — | — | **2** | 1 | — | nrrd 512x512x892 | 0.4863 x 0.4863 x 0.3 |
| `z001` | Body | 76/M | 1.8 | 87 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3223 | 0.9902 x 0.9902 x 0.5 |
| `z009` | Body | 25/M | 1.75 | 74 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3228 | 1.1152 x 1.1152 x 0.5 |
| `z013` | Body | 41/F | 1.65 | 56.3 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3113 | 1.2695 x 1.2695 x 0.5 |
| `z019` | Body | 58/M | 1.81 | 71.3 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3304 | 1.1621 x 1.1621 x 0.5 |
| `z023` | Body | 47/F | 1.66 | 61 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3122 | 1.0352 x 1.0352 x 0.5 |
| `z027` | Body | 37/F | 1.69 | 51.5 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3208 | 1.2695 x 1.2695 x 0.5 |
| `z035` | Body | 30/F | 1.68 | 50.45 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3083 | 1.2695 x 1.2695 x 0.5 |
| `z036` | Body | 62/M | — | — | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3383 | 1.2695 x 1.2695 x 0.5 |
| `z042` | Body | 61/F | 1.69 | 53.4 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3054 | 1.0098 x 1.0098 x 0.5 |
| `z046` | Body | 38/M | 1.8 | 72 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x6564 | 1.2695 x 1.2695 x 0.25 |
| `z049` | Body | 34/F | 1.79 | 87 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3299 | 1.2695 x 1.2695 x 0.5 |
| `z050` | Body | 84/M | 1.67 | 73.4 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x6005 | 1.0762 x 1.0762 x 0.25 |
| `z055` | Body | 73/M | 1.73 | 73 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3183 | 1.2695 x 1.2695 x 0.5 |
| `z056` | Body | 26/M | 1.87 | 81.8 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3467 | 1.2695 x 1.2695 x 0.5 |
| `z057` | Body | 75/M | — | — | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3218 | 1.2695 x 1.2695 x 0.5 |
| `z061` | Body | 39/F | 1.8 | 37.4 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3273 | 1.2695 x 1.2695 x 0.5 |
| `z062` | Body | 43/M | 1.77 | 76.95 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3216 | 1.2695 x 1.2695 x 0.5 |
| `z063` | Body | 72/F | 1.72 | 80.2 | **21** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3200 | 1.1094 x 1.1094 x 0.5 |
| `z064` | Body | 69/M | — | — | **22** | 21 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3220 | 1.0684 x 1.0684 x 0.5 |
| `z066` | Body | 48/M | — | — | **21** | 19 | Pelvis-Thighs, Shanks-Feet | nii 512x512x3114 | 1.2695 x 1.2695 x 0.5 |

## Region distribution

| SMIR region | Cases | Meaning |
|---|---|---|
| `Body` | 20 | whole body below the head — the `z*` series, stored as one large `.nii` |
| `Lower_limb` | 8 | dedicated leg scan, one or two series per case |
| `Lower_limb + Thorax` | 1 | case `002` only — adds a thorax series with a `Left_Iliac_Crest` ROI |
| `Vertebra` | 1 | `Phantom001` only — European Spine Phantom |

## Notes for phantom building

- **`z*` cases (20)** are whole-body `.nii` volumes of 3054–6564 slices (1.6–2.2 GB each) at 0.5 mm slice pitch, except `z046` and `z050` which are 0.25 mm. Each ships a `Transform-Upside-Down.h5` — the volume is stored **feet-first/inverted** and needs that transform applied before use.
- **Cropped ROI volumes** `-Pelvis-Thighs.nrrd` and `-Shanks-Feet.nrrd` accompany **25** of the 30 cases and are far cheaper to work with than the full volume; each has a matching `_Segmentation.seg.nrrd` (manual labels) and `_Reconstruction.seg.nrrd` (surface-derived labels).
- **Cases `010`, `015`, `016`, `017`** ship *no* cropped ROIs — the two per-case series are already upper-leg and lower-leg scans. **Case `019`** ships only crops (both at full 512x512 FOV) and no uncropped series.
- **`z066`** has 19 `.ply` models but 21 segments: `Phalanges_L/R` are labelled but have no surface mesh.
- Case `002` carries a `Thorax2Lower_limb.h5` registration between its two series.


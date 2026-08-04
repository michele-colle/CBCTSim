# TotalSegmentator v2.0.1 — case selection by body region

Source: `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201` (`/mnt/h/MICHELE_MCGPU/Totalsegmentator_dataset_v201` from WSL) — 1228 cases, **all 1.5 mm isotropic**.
Machine-readable companion: `totalsegmentator_selection.json`. Regenerate both with `make_totalsegmentator_selection.py`.

Each case folder holds `ct.nii.gz` plus a `segmentations/` folder of 117 masks. Convert a case to a DICOM series `dicom_to_mcgpu` can read with:

```bash
python3 nrrd_to_dicom_series.py <case>/ct.nii.gz <out_dir>/<case> --name <case>
```

## How the groups were derived

TotalSegmentator has no hand, forearm, foot, ankle, knee, tibia or patella class, so only head, upper arm and thigh are directly readable from a mask. Coverage further down a limb is inferred from how far the volume extends past the distal femur. Masks under 1000 voxels are ignored as segmentation noise (two `ct angiography head` cases carry a 2- and an 8-voxel "femur" that would otherwise count as leg scans).

**Not present anywhere in the dataset:** hands and forearms. The single `ct upper limb both` case (s0035) is a shoulder/upper-thorax scan cut off at mid-humerus.

| Group | Criterion | Cases | Usable as-is | Sheared by the round-7 bug |
|---|---|---|---|---|
| `full_head` | skull mask spans >= 100 mm in z | 180 | 139 | 41 |
| `dedicated_head` | meta.csv study_type contains 'head' or 'orbita' | 83 | 62 | 21 |
| `knee` | femur present and volume extends > 10 mm below its distal end | 32 | 24 | 8 |
| `below_knee` | volume extends > 60 mm below the distal femur | 10 | 8 | 2 |
| `foot_ankle` | volume extends > 300 mm below the distal femur (all 5 visually confirmed) | 5 | 3 | 2 |
| `thigh` | femur mask >= 1000 voxels | 559 | 510 | 49 |

Groups **overlap**: `knee` ⊃ `below_knee` ⊃ `foot_ankle` are nested subsets of `thigh`, and most `dedicated_head` cases are also `full_head`.

## ⚠ Obliquity — read before building phantoms

`dicom_to_mcgpu` places voxels at `(index+0.5)*spacing` and never reads `ImageOrientationPatient` (PROGRESS round 7, still open):

- **`tilt_out_of_plane_deg` > 0** — the slice normal is off the patient axis, so the phantom comes out **SHEARED**. Fix the direction-cosine bug before building these.
- **`tilt_in_plane_deg` only** — the patient is rigidly rotated about z inside the voxel box. Geometrically valid, just an unusual pose.

## Full / near-full cranium — `full_head` (180 cases)

*skull mask spans >= 100 mm in z*

| Case | Study type | Age/Sex | Shape | skull mm | tilt out / in (deg) | Path |
|---|---|---|---|---|---|---|
| `s0002` | ct thorax-neck | 49/f | 185×128×101 | 104.9 | 1.95 / 2.21 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0002` |
| `s0021` | ct neck | 46/m | 185×185×218 | 109.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0021` |
| `s0043` | ct neck | 47/f | 134×159×159 | 142.2 | 3.85 / 3.84 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0043` |
| `s0056` | ct neck | 85/m | 179×179×139 | 109.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0056` |
| `s0061` | ct neck | 56/m | 233×194×202 | 119.7 | 4.38 / 5.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0061` |
| `s0067` | ct thorax-neck | 86/f | 122×154×243 | 129.1 | 8.32 / 5.97 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0067` |
| `s0079` | ct neck | 60/m | 220×196×196 | 106.4 | 2.99 / 4.11 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0079` |
| `s0081` | ct spine | 49/m | 135×153×153 | 101.4 | 6.12 / 6.18 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0081` |
| `s0101` | ct orbita | 24/m | 119×119×74 | 109.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0101` |
| `s0103` | ct polytrauma head | 41/m | 178×178×117 | 153.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0103` |
| `s0112` | ct neck | 73/f | 160×158×160 | 126.0 | 1.49 / 3.39 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0112` |
| `s0130` | ct spine | 75/m | 69×101×135 | 100.5 | 1.07 / 3.24 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0130` |
| `s0140` | ct thorax-neck | 66/m | 196×196×125 | 105.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0140` |
| `s0152` | ct angiography head | 73/m | 233×233×224 | 223.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0152` |
| `s0167` | ct neck | 39/f | 177×177×149 | 138.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0167` |
| `s0170` | ct neck | 55/m | 165×156×198 | 104.0 | 15.72 / 5.66 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0170` |
| `s0187` | ct polytrauma head | 25/m | 180×180×125 | 177.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0187` |
| `s0224` | ct polytrauma | 47/m | 333×333×621 | 196.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0224` |
| `s0226` | ct neck | 54/f | 190×188×154 | 100.2 | 4.12 / 6.57 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0226` |
| `s0254` | ct neck | 56/m | 178×168×168 | 107.9 | 2.35 / 3.24 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0254` |
| `s0263` | ct neck | 44/m | 171×171×161 | 127.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0263` |
| `s0277` | ct neck | 43/f | 224×162×224 | 124.1 | 13.19 / 0.13 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0277` |
| `s0289` | ct angiography head | 55/f | 145×145×233 | 199.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0289` |
| `s0292` | ct angiography head | 66/m | 148×148×233 | 220.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0292` |
| `s0326` | ct neck | 56/m | 217×168×217 | 114.0 | 1.25 / 1.46 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0326` |
| `s0333` | ct angiography head | 83/m | 219×219×217 | 231.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0333` |
| `s0340` | ct angiography head | 63/m | 163×163×100 | 148.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0340` |
| `s0346` | ct angiography head | 62/m | 215×215×241 | 234.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0346` |
| `s0377` | ct angiography neck-thx-abd-pelvis-leg | 52/m | 248×248×259 | 115.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0377` |
| `s0382` | ct neck | 81/m | 232×232×309 | 223.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0382` |
| `s0388` | ct angiography head | 57/f | 154×154×102 | 150.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0388` |
| `s0400` | ct neck | 53/m | 225×160×203 | 176.8 | 2.49 / 3.47 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0400` |
| `s0410` | ct angiography head | 63/m | 151×151×75 | 111.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0410` |
| `s0412` | ct polytrauma | 72/m | 313×313×431 | 193.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0412` |
| `s0417` | ct  operation | 52/m | 142×161×86 | 127.5 | 0.63 / 0.62 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0417` |
| `s0439` | ct heart-thorakale aorta | 52/m | 307×221×334 | 133.5 | 0.0 / 0.62 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0439` |
| `s0449` | ct neck | 77/f | 140×140×107 | 156.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0449` |
| `s0460` | ct angiography head | 28/f | 115×139×102 | 151.2 | 3.46 / 15.76 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0460` |
| `s0474` | ct angiography head | 61/f | 137×137×73 | 108.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0474` |
| `s0478` | ct angiography head | 57/f | 181×181×237 | 222.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0478` |
| `s0482` | ct spine | 54/f | 241×241×265 | 123.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0482` |
| `s0491` | ct neck | 55/m | 173×170×173 | 114.0 | 0.36 / 5.2 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0491` |
| `s0497` | ct angiography head | 83/m | 188×188×244 | 235.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0497` |
| `s0503` | ct angiography head | 67/m | 174×174×233 | 195.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0503` |
| `s0504` | ct angiography neck | 60/f | 81×124×189 | 136.4 | 2.15 / 5.86 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0504` |
| `s0511` | ct polytrauma | 40/m | 284×284×465 | 693.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0511` |
| `s0514` | ct angiography head | 52/f | 121×140×108 | 151.0 | 16.57 / 4.72 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0514` |
| `s0518` | ct neck | 47/m | 145×145×236 | 156.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0518` |
| `s0526` | ct angiography head | 44/m | 93×129×197 | 127.4 | 1.64 / 2.23 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0526` |
| `s0527` | ct angiography neck | 29/m | 98×152×209 | 139.5 | 0.92 / 1.51 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0527` |
| `s0535` | ct neck | 67/m | 133×133×211 | 151.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0535` |
| `s0537` | ct neck | 45/m | 198×198×166 | 121.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0537` |
| `s0539` | ct  operation | 66/m | 114×134×80 | 112.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0539` |
| `s0552` | ct neck | 62/f | 118×142×151 | 103.5 | 1.04 / 0.41 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0552` |
| `s0556` | ct polytrauma | 51/m | 333×333×484 | 195.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0556` |
| `s0567` | ct angiography head | 62/m | 161×161×112 | 163.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0567` |
| `s0568` | ct neck | 56/f | 157×157×141 | 144.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0568` |
| `s0575` | ct neck | 77/f | 143×143×103 | 147.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0575` |
| `s0588` | ct polytrauma | 44/m | 184×184×117 | 171.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0588` |
| `s0589` | ct thorax-abdomen-pelvis | 58/f | 265×265×423 | 207.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0589` |
| `s0591` | ct polytrauma | 74/m | 275×275×531 | 210.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0591` |
| `s0597` | ct aortic valve | 79/m | 261×261×161 | 121.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0597` |
| `s0605` | ct angiography neck-thx-abd-pelvis-leg | 54/m | 267×267×223 | 123.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0605` |
| `s0610` | ct angiography head | 87/m | 240×240×206 | 205.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0610` |
| `s0616` | ct polytrauma | 56/m | 337×261×644 | 197.7 | 2.96 / 3.53 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0616` |
| `s0622` | ct neck | 94/f | 263×146×241 | 174.7 | 17.33 / 8.66 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0622` |
| `s0624` | ct aortic valve | 81/m | 230×186×517 | 115.5 | 0.85 / 5.28 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0624` |
| `s0643` | ct angiography head | 65/m | 178×178×263 | 228.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0643` |
| `s0660` | ct angiography head | 76/m | 202×202×252 | 219.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0660` |
| `s0664` | ct thorax-abdomen | 74/f | 332×332×405 | 606.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0664` |
| `s0690` | ct polytrauma | 94/f | 281×162×538 | 208.5 | 0.0 / 0.71 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0690` |
| `s0691` | ct neck-thorax-abdomen-pelvis | 71/m | 329×329×280 | 136.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0691` |
| `s0697` | ct angiography neck-thx-abd-pelvis-leg | 58/m | 221×221×264 | 298.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0697` |
| `s0713` | ct  operation | 72/f | 161×161×76 | 111.0 | 0.0 / 0.75 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0713` |
| `s0727` | ct polytrauma | 88/f | 247×247×525 | 205.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0727` |
| `s0734` | ct neck | 72/m | 224×224×145 | 105.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0734` |
| `s0744` | ct angiography neck | 48/m | 109×92×111 | 160.5 | 0.61 / 5.68 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0744` |
| `s0748` | ct angiography head | 57/m | 140×140×87 | 129.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0748` |
| `s0754` | ct polytrauma | 60/m | 376×202×649 | 219.0 | 0.0 / 1.62 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0754` |
| `s0756` | ct angiography head | 67/m | 129×129×182 | 102.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0756` |
| `s0766` | ct angiography head | 61/m | 118×138×100 | 146.9 | 8.51 / 7.42 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0766` |
| `s0769` | ct neck | 75/m | 245×245×191 | 133.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0769` |
| `s0777` | ct polytrauma | 74/m | 305×305×536 | 229.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0777` |
| `s0786` | ct polytrauma | 29/f | 174×174×155 | 211.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0786` |
| `s0790` | ct polytrauma | 40/f | 291×291×538 | 210.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0790` |
| `s0792` | ct angiography head | 84/m | 125×152×109 | 149.6 | 12.07 / 5.53 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0792` |
| `s0810` | ct thorax-neck | 74/m | 289×289×289 | 103.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0810` |
| `s0812` | ct polytrauma | 60/m | 328×328×549 | 189.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0812` |
| `s0820` | ct angiography head | 78/m | 193×193×252 | 207.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0820` |
| `s0849` | ct angiography head | 79/m | 251×251×245 | 205.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0849` |
| `s0855` | ct angiography head | 55/m | 167×167×114 | 159.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0855` |
| `s0857` | ct angiography head | 41/f | 179×179×227 | 222.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0857` |
| `s0865` | ct angiography head | 85/f | 115×119×155 | 105.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0865` |
| `s0868` | ct neck | 79/f | 176×176×145 | 112.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0868` |
| `s0874` | ct neck | 77/m | 142×184×247 | 226.1 | 3.6 / 4.49 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0874` |
| `s0875` | ct neck | 72/m | 190×190×170 | 163.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0875` |
| `s0880` | ct thorax-abdomen-pelvis | 72/m | 291×291×463 | 282.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0880` |
| `s0903` | ct polytrauma | 87/m | 333×333×427 | 495.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0903` |
| `s0922` | ct angiography head | 74/m | 121×143×120 | 159.3 | 26.83 / 3.05 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0922` |
| `s0927` | ct polytrauma | 93/f | 333×333×519 | 223.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0927` |
| `s0933` | ct polytrauma | 55/f | 328×268×660 | 231.0 | 0.0 / 0.99 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0933` |
| `s0949` | ct angiography head | 80/m | 101×180×225 | 217.5 | 0.0 / 0.95 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0949` |
| `s0953` | ct neck-thorax-abdomen-pelvis | 56/f | 239×239×135 | 124.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0953` |
| `s0960` | ct angiography head | 56/m | 220×220×248 | 244.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0960` |
| `s0963` | ct thorax-abdomen-pelvis | 59/f | 208×208×402 | 150.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0963` |
| `s0970` | ct polytrauma | 51/f | 333×333×511 | 190.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0970` |
| `s0974` | ct angiography head | 77/f | 187×187×224 | 219.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0974` |
| `s0984` | ct angiography head | 44/m | 140×140×113 | 159.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0984` |
| `s0986` | ct polytrauma | 88/f | 331×145×571 | 186.0 | 0.0 / 0.88 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0986` |
| `s0993` | ct angiography head | 63/m | 146×146×107 | 157.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0993` |
| `s0997` | ct head | 90/f | 137×137×96 | 139.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0997` |
| `s1001` | ct neck | 84/f | 173×173×159 | 106.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1001` |
| `s1003` | ct angiography head | 69/m | 71×119×187 | 100.5 | 0.0 / 1.63 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1003` |
| `s1010` | ct neck | 42/m | 208×208×169 | 103.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1010` |
| `s1011` | ct head | 49/m | 181×164×267 | 201.3 | 11.6 / 2.41 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1011` |
| `s1014` | ct angiography head | 70/f | 111×133×116 | 164.3 | 5.18 / 4.55 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1014` |
| `s1015` | ct angiography head | 80/m | 127×127×168 | 105.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1015` |
| `s1029` | ct polytrauma | 71/m | 321×321×545 | 723.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1029` |
| `s1032` | ct angiography head | 62/m | 81×155×167 | 115.5 | 0.0 / 3.84 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1032` |
| `s1034` | ct angiography head | 67/f | 184×184×233 | 210.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1034` |
| `s1045` | ct polytrauma | 62/m | 299×205×645 | 228.0 | 0.0 / 2.66 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1045` |
| `s1055` | ct orbita | 55/m | 171×171×70 | 103.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1055` |
| `s1056` | ct angiography head | 48/m | 191×191×110 | 156.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1056` |
| `s1072` | ct angiography head | 88/f | 167×167×109 | 151.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1072` |
| `s1075` | ct angiography head | 66/f | 175×175×219 | 225.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1075` |
| `s1082` | ct polytrauma | 69/m | 333×333×542 | 225.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1082` |
| `s1088` | ct polytrauma | 90/m | 333×333×544 | 201.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1088` |
| `s1093` | ct neck | 65/f | 199×199×239 | 235.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1093` |
| `s1107` | ct angiography head | 57/m | 229×229×253 | 235.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1107` |
| `s1112` | ct angiography neck | 74/f | 213×213×220 | 226.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1112` |
| `s1113` | ct angiography head | 81/m | 206×206×219 | 219.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1113` |
| `s1123` | ct polytrauma | 48/m | 291×291×545 | 198.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1123` |
| `s1129` | ct angiography head | 85/m | 174×174×251 | 195.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1129` |
| `s1135` | ct thorax-abdomen-pelvis | 52/m | 299×299×443 | 424.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1135` |
| `s1138` | ct angiography head | 58/f | 182×182×260 | 234.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1138` |
| `s1148` | ct angiography head | 85/f | 105×120×102 | 149.5 | 9.25 / 4.5 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1148` |
| `s1150` | ct head | 45/f | 167×167×101 | 144.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1150` |
| `s1152` | ct polytrauma | 76/f | 333×333×548 | 202.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1152` |
| `s1154` | ct angiography head | 69/m | 201×201×239 | 216.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1154` |
| `s1159` | ct polytrauma | 47/f | 329×329×537 | 165.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1159` |
| `s1161` | ct polytrauma | 52/m | 313×313×544 | 214.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1161` |
| `s1163` | ct angiography head | 79/m | 171×171×228 | 208.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1163` |
| `s1168` | ct neck-thorax-abdomen-pelvis | 52/m | 177×177×128 | 105.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1168` |
| `s1171` | ct polytrauma | 26/m | 333×333×544 | 184.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1171` |
| `s1178` | ct neck-thorax-abdomen-pelvis | 57/f | 333×333×433 | 634.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1178` |
| `s1184` | ct angiography head | 80/m | 204×204×221 | 234.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1184` |
| `s1185` | ct neck | 24/f | 133×106×172 | 105.5 | 7.99 / 1.95 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1185` |
| `s1192` | ct angiography head | 71/m | 228×228×226 | 246.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1192` |
| `s1194` | ct head | 18/m | 162×162×147 | 204.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1194` |
| `s1206` | ct polytrauma | 62/m | 279×279×538 | 796.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1206` |
| `s1215` | ct polytrauma | 57/m | 303×303×92 | 123.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1215` |
| `s1220` | ct neck | 23/m | 198×198×163 | 145.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1220` |
| `s1227` | ct  operation | 61/f | 165×165×98 | 132.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1227` |
| `s1231` | ct angiography head | 67/f | 177×177×229 | 208.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1231` |
| `s1249` | ct polytrauma | 94/f | 333×333×603 | 240.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1249` |
| `s1260` | ct angiography head | 32/m | 135×135×113 | 160.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1260` |
| `s1300` | ct neck | 78/m | 167×167×101 | 147.4 | 7.12 / 5.76 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1300` |
| `s1304` | ct polytrauma | 67/m | 333×333×546 | 817.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1304` |
| `s1311` | ct thorax-neck | 70/m | 288×288×289 | 130.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1311` |
| `s1314` | ct polytrauma | 62/f | 333×333×546 | 199.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1314` |
| `s1321` | ct polytrauma | 50/m | 275×275×540 | 205.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1321` |
| `s1325` | ct orbita | 83/m | 124×117×77 | 112.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1325` |
| `s1335` | ct neck | 44/f | 124×124×123 | 100.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1335` |
| `s1341` | ct thorax-abdomen | 19/m | 225×225×101 | 150.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1341` |
| `s1350` | ct polytrauma | 66/f | 333×333×539 | 208.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1350` |
| `s1352` | ct angiography head | 56/m | 93×145×201 | 128.6 | 4.24 / 4.24 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1352` |
| `s1362` | ct angiography thorax-abdomen-pelvis | 88/m | 275×275×429 | 189.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1362` |
| `s1369` | ct whole body | 73/f | 290×290×549 | 220.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1369` |
| `s1370` | ct angiography thorax-abdomen-pelvis | 78/m | 221×221×271 | 225.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1370` |
| `s1372` | ct angiography thorax-abdomen-pelvis | 82/f | 236×236×428 | 570.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1372` |
| `s1380` | ct whole body | 78/f | 320×320×543 | 217.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1380` |
| `s1384` | ct whole body | 65/f | 329×329×523 | 214.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1384` |
| `s1388` | ct whole body | 68/f | 316×316×525 | 235.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1388` |
| `s1397` | ct whole body | 74/m | 320×320×547 | 253.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1397` |
| `s1408` | ct head | 42/f | 151×151×111 | 156.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1408` |
| `s1409` | ct head | 87/m | 123×130×105 | 155.8 | 2.59 / 3.5 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1409` |
| `s1410` | ct head | 90/f | 116×105×84 | 124.4 | 2.71 / 2.71 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1410` |
| `s1416` | ct head | 87/f | 116×140×102 | 145.5 | 1.21 / 7.13 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1416` |
| `s1417` | ct head | 70/f | 117×134×114 | 151.2 | 3.49 / 4.84 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1417` |
| `s1419` | ct head | 71/f | 92×151×121 | 145.4 | 1.82 / 2.45 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1419` |

## Dedicated head studies — `dedicated_head` (83 cases)

*meta.csv study_type contains 'head' or 'orbita'*

| Case | Study type | Age/Sex | Shape | skull mm | tilt out / in (deg) | Path |
|---|---|---|---|---|---|---|
| `s0101` | ct orbita | 24/m | 119×119×74 | 109.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0101` |
| `s0103` | ct polytrauma head | 41/m | 178×178×117 | 153.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0103` |
| `s0152` | ct angiography head | 73/m | 233×233×224 | 223.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0152` |
| `s0187` | ct polytrauma head | 25/m | 180×180×125 | 177.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0187` |
| `s0289` | ct angiography head | 55/f | 145×145×233 | 199.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0289` |
| `s0292` | ct angiography head | 66/m | 148×148×233 | 220.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0292` |
| `s0333` | ct angiography head | 83/m | 219×219×217 | 231.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0333` |
| `s0340` | ct angiography head | 63/m | 163×163×100 | 148.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0340` |
| `s0346` | ct angiography head | 62/m | 215×215×241 | 234.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0346` |
| `s0352` | ct angiography head | 52/f | 100×116×58 | 85.2 | 4.41 / 3.71 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0352` |
| `s0388` | ct angiography head | 57/f | 154×154×102 | 150.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0388` |
| `s0410` | ct angiography head | 63/m | 151×151×75 | 111.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0410` |
| `s0411` | ct angiography head | 71/f | 121×121×154 | 87.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0411` |
| `s0460` | ct angiography head | 28/f | 115×139×102 | 151.2 | 3.46 / 15.76 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0460` |
| `s0474` | ct angiography head | 61/f | 137×137×73 | 108.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0474` |
| `s0478` | ct angiography head | 57/f | 181×181×237 | 222.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0478` |
| `s0497` | ct angiography head | 83/m | 188×188×244 | 235.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0497` |
| `s0501` | ct angiography head | 78/m | 77×125×143 | 58.4 | 3.79 / 6.34 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0501` |
| `s0503` | ct angiography head | 67/m | 174×174×233 | 195.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0503` |
| `s0514` | ct angiography head | 52/f | 121×140×108 | 151.0 | 16.57 / 4.72 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0514` |
| `s0521` | ct angiography head | 57/m | 53×120×133 | 66.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0521` |
| `s0526` | ct angiography head | 44/m | 93×129×197 | 127.4 | 1.64 / 2.23 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0526` |
| `s0563` | ct angiography head | 46/m | 51×74×154 | 16.1 | 12.45 / 2.31 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0563` |
| `s0567` | ct angiography head | 62/m | 161×161×112 | 163.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0567` |
| `s0610` | ct angiography head | 87/m | 240×240×206 | 205.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0610` |
| `s0643` | ct angiography head | 65/m | 178×178×263 | 228.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0643` |
| `s0660` | ct angiography head | 76/m | 202×202×252 | 219.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0660` |
| `s0706` | ct angiography head | 53/f | 194×194×68 | 94.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0706` |
| `s0748` | ct angiography head | 57/m | 140×140×87 | 129.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0748` |
| `s0756` | ct angiography head | 67/m | 129×129×182 | 102.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0756` |
| `s0766` | ct angiography head | 61/m | 118×138×100 | 146.9 | 8.51 / 7.42 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0766` |
| `s0792` | ct angiography head | 84/m | 125×152×109 | 149.6 | 12.07 / 5.53 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0792` |
| `s0820` | ct angiography head | 78/m | 193×193×252 | 207.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0820` |
| `s0849` | ct angiography head | 79/m | 251×251×245 | 205.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0849` |
| `s0855` | ct angiography head | 55/m | 167×167×114 | 159.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0855` |
| `s0857` | ct angiography head | 41/f | 179×179×227 | 222.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0857` |
| `s0865` | ct angiography head | 85/f | 115×119×155 | 105.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0865` |
| `s0922` | ct angiography head | 74/m | 121×143×120 | 159.3 | 26.83 / 3.05 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0922` |
| `s0949` | ct angiography head | 80/m | 101×180×225 | 217.5 | 0.0 / 0.95 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0949` |
| `s0960` | ct angiography head | 56/m | 220×220×248 | 244.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0960` |
| `s0974` | ct angiography head | 77/f | 187×187×224 | 219.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0974` |
| `s0984` | ct angiography head | 44/m | 140×140×113 | 159.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0984` |
| `s0993` | ct angiography head | 63/m | 146×146×107 | 157.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0993` |
| `s0997` | ct head | 90/f | 137×137×96 | 139.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0997` |
| `s1003` | ct angiography head | 69/m | 71×119×187 | 100.5 | 0.0 / 1.63 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1003` |
| `s1011` | ct head | 49/m | 181×164×267 | 201.3 | 11.6 / 2.41 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1011` |
| `s1014` | ct angiography head | 70/f | 111×133×116 | 164.3 | 5.18 / 4.55 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1014` |
| `s1015` | ct angiography head | 80/m | 127×127×168 | 105.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1015` |
| `s1032` | ct angiography head | 62/m | 81×155×167 | 115.5 | 0.0 / 3.84 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1032` |
| `s1034` | ct angiography head | 67/f | 184×184×233 | 210.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1034` |
| `s1040` | ct angiography head | 46/m | 135×165×190 | 94.3 | 3.39 / 3.38 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1040` |
| `s1047` | ct angiography head | 52/f | 129×129×62 | 91.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1047` |
| `s1055` | ct orbita | 55/m | 171×171×70 | 103.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1055` |
| `s1056` | ct angiography head | 48/m | 191×191×110 | 156.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1056` |
| `s1072` | ct angiography head | 88/f | 167×167×109 | 151.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1072` |
| `s1075` | ct angiography head | 66/f | 175×175×219 | 225.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1075` |
| `s1107` | ct angiography head | 57/m | 229×229×253 | 235.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1107` |
| `s1113` | ct angiography head | 81/m | 206×206×219 | 219.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1113` |
| `s1115` | ct angiography head | 66/m | 136×136×173 | 99.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1115` |
| `s1129` | ct angiography head | 85/m | 174×174×251 | 195.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1129` |
| `s1138` | ct angiography head | 58/f | 182×182×260 | 234.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1138` |
| `s1147` | ct angiography head | 37/f | 51×101×150 | 24.0 | 2.95 / 5.73 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1147` |
| `s1148` | ct angiography head | 85/f | 105×120×102 | 149.5 | 9.25 / 4.5 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1148` |
| `s1150` | ct head | 45/f | 167×167×101 | 144.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1150` |
| `s1154` | ct angiography head | 69/m | 201×201×239 | 216.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1154` |
| `s1163` | ct angiography head | 79/m | 171×171×228 | 208.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1163` |
| `s1177` | ct orbita | 50/m | 121×137×67 | 94.5 | 0.0 / 0.28 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1177` |
| `s1184` | ct angiography head | 80/m | 204×204×221 | 234.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1184` |
| `s1192` | ct angiography head | 71/m | 228×228×226 | 246.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1192` |
| `s1194` | ct head | 18/m | 162×162×147 | 204.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1194` |
| `s1231` | ct angiography head | 67/f | 177×177×229 | 208.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1231` |
| `s1260` | ct angiography head | 32/m | 135×135×113 | 160.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1260` |
| `s1325` | ct orbita | 83/m | 124×117×77 | 112.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1325` |
| `s1328` | ct angiography head | 68/f | 247×247×47 | 57.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1328` |
| `s1352` | ct angiography head | 56/m | 93×145×201 | 128.6 | 4.24 / 4.24 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1352` |
| `s1406` | ct gesichtshead | 47/m | 117×115×69 | 95.9 | 2.14 / 2.26 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1406` |
| `s1407` | ct headbasis-felsenleg | 30/m | 111×99×111 | 79.5 | 0.0 / 1.26 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1407` |
| `s1408` | ct head | 42/f | 151×151×111 | 156.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1408` |
| `s1409` | ct head | 87/m | 123×130×105 | 155.8 | 2.59 / 3.5 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1409` |
| `s1410` | ct head | 90/f | 116×105×84 | 124.4 | 2.71 / 2.71 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1410` |
| `s1416` | ct head | 87/f | 116×140×102 | 145.5 | 1.21 / 7.13 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1416` |
| `s1417` | ct head | 70/f | 117×134×114 | 151.2 | 3.49 / 4.84 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1417` |
| `s1419` | ct head | 71/f | 92×151×121 | 145.4 | 1.82 / 2.45 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1419` |

## Knee inside the volume — `knee` (32 cases)

*femur present and volume extends > 10 mm below its distal end*

| Case | Study type | Age/Sex | Shape | mm below femur | tilt out / in (deg) | Path |
|---|---|---|---|---|---|---|
| `s0004` | ct thorax-abdomen-pelvis | 71/f | 255×177×440 | 15.0 | 2.68 / 3.32 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0004` |
| `s0074` | ct pelvis | 50/m | 251×192×180 | 19.5 | 0.0 / 0.75 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0074` |
| `s0086` | ct neck-thorax-abdomen-pelvis | 71/m | 273×430×430 | 10.5 | 0.0 / 0.46 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0086` |
| `s0109` | ct abdomen-pelvis | 76/m | 303×252×303 | 10.5 | 0.0 / 1.23 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0109` |
| `s0131` | ct abdomen-pelvis | 38/m | 265×202×245 | 10.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0131` |
| `s0151` | ct abdomen-pelvis | 67/m | 292×184×292 | 39.0 | 0.0 / 0.92 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0151` |
| `s0212` | ct thorax-abdomen | 66/m | 262×184×327 | 12.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0212` |
| `s0287` | ct angiography neck-thx-abd-pelvis-leg | 80/m | 317×317×835 | 292.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0287` |
| `s0298` | ct abdomen-pelvis | 66/m | 348×206×348 | 88.5 | 0.0 / 1.49 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0298` |
| `s0301` | ct pelvis | 76/m | 253×138×253 | 15.0 | 0.0 / 2.6 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0301` |
| `s0361` | ct angiography pelvis-leg | 81/m | 299×190×483 | 88.5 | 0.0 / 2.77 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0361` |
| `s0468` | ct angiography pelvis-leg | 76/m | 233×233×838 | 432.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0468` |
| `s0621` | ct thorax-abdomen-pelvis | 85/m | 270×310×453 | 16.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0621` |
| `s0633` | ct upper leg left | 80/f | 166×166×326 | 42.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0633` |
| `s0774` | ct angiography pelvis-leg | 75/f | 258×206×626 | 397.5 | 0.0 / 2.22 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0774` |
| `s0783` | ct upper leg left | 57/m | 165×159×659 | 486.3 | 8.73 / 4.52 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0783` |
| `s0831` | ct angiography pelvis-leg | 63/m | 253×253×524 | 45.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0831` |
| `s0837` | ct upper leg left | 76/f | 77×101×185 | 47.9 | 4.27 / 4.24 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0837` |
| `s0933` | ct polytrauma | 55/f | 328×268×660 | 21.0 | 0.0 / 0.99 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0933` |
| `s0937` | ct angiography pelvis-leg | 79/m | 253×253×548 | 34.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0937` |
| `s0982` | ct thorax-abdomen-pelvis | 56/m | 347×247×513 | 15.0 | 0.5 / 10.18 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0982` |
| `s1045` | ct polytrauma | 62/m | 299×205×645 | 25.5 | 0.0 / 2.66 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1045` |
| `s1098` | ct pelvis | 56/f | 331×331×422 | 36.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1098` |
| `s1103` | ct angiography pelvis-leg | 71/m | 283×162×851 | 470.2 | 3.41 / 1.98 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1103` |
| `s1104` | ct angiography pelvis-leg | 57/m | 298×138×726 | 562.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1104` |
| `s1121` | ct angiography pelvis-leg | 30/f | 281×281×519 | 85.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1121` |
| `s1170` | ct upper leg left | 90/f | 131×89×311 | 28.4 | 4.58 / 3.45 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1170` |
| `s1174` | ct thorax-abdomen-pelvis | 61/m | 279×178×481 | 12.0 | 0.89 / 5.21 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1174` |
| `s1251` | ct angiography pelvis-leg | 75/m | 280×280×472 | 40.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1251` |
| `s1252` | ct angiography pelvis-leg | 52/f | 276×120×506 | 42.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1252` |
| `s1303` | ct upper leg left | 79/m | 113×113×388 | 61.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1303` |
| `s1310` | ct aortic valve | 86/f | 208×165×461 | 22.5 | 0.48 / 2.07 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1310` |

## Lower leg (below the knee) — `below_knee` (10 cases)

*volume extends > 60 mm below the distal femur*

| Case | Study type | Age/Sex | Shape | mm below femur | tilt out / in (deg) | Path |
|---|---|---|---|---|---|---|
| `s0287` | ct angiography neck-thx-abd-pelvis-leg | 80/m | 317×317×835 | 292.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0287` |
| `s0298` | ct abdomen-pelvis | 66/m | 348×206×348 | 88.5 | 0.0 / 1.49 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0298` |
| `s0361` | ct angiography pelvis-leg | 81/m | 299×190×483 | 88.5 | 0.0 / 2.77 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0361` |
| `s0468` | ct angiography pelvis-leg | 76/m | 233×233×838 | 432.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0468` |
| `s0774` | ct angiography pelvis-leg | 75/f | 258×206×626 | 397.5 | 0.0 / 2.22 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0774` |
| `s0783` | ct upper leg left | 57/m | 165×159×659 | 486.3 | 8.73 / 4.52 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0783` |
| `s1103` | ct angiography pelvis-leg | 71/m | 283×162×851 | 470.2 | 3.41 / 1.98 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1103` |
| `s1104` | ct angiography pelvis-leg | 57/m | 298×138×726 | 562.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1104` |
| `s1121` | ct angiography pelvis-leg | 30/f | 281×281×519 | 85.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1121` |
| `s1303` | ct upper leg left | 79/m | 113×113×388 | 61.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1303` |

## Foot / ankle — `foot_ankle` (5 cases)

*volume extends > 300 mm below the distal femur (all 5 visually confirmed)*

| Case | Study type | Age/Sex | Shape | mm below femur | tilt out / in (deg) | Path |
|---|---|---|---|---|---|---|
| `s0468` | ct angiography pelvis-leg | 76/m | 233×233×838 | 432.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0468` |
| `s0774` | ct angiography pelvis-leg | 75/f | 258×206×626 | 397.5 | 0.0 / 2.22 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0774` |
| `s0783` | ct upper leg left | 57/m | 165×159×659 | 486.3 | 8.73 / 4.52 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0783` |
| `s1103` | ct angiography pelvis-leg | 71/m | 283×162×851 | 470.2 | 3.41 / 1.98 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1103` |
| `s1104` | ct angiography pelvis-leg | 57/m | 298×138×726 | 562.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1104` |

## Thigh (femur present) — `thigh` (559 cases)

*femur mask >= 1000 voxels*

| Case | Study type | Age/Sex | Shape | mm below femur | tilt out / in (deg) | Path |
|---|---|---|---|---|---|---|
| `s0000` | ct pelvis | 29/f | 294×192×179 | 1.5 | 0.0 / 18.36 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0000` |
| `s0001` | ct abdomen-pelvis | 58/f | 249×188×213 | 0.0 | 3.55 / 3.79 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0001` |
| `s0004` | ct thorax-abdomen-pelvis | 71/f | 255×177×440 | 15.0 | 2.68 / 3.32 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0004` |
| `s0006` | ct abdomen-pelvis | 46/m | 216×216×217 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0006` |
| `s0009` | ct spine | 85/f | 119×119×185 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0009` |
| `s0010` | ct abdomen-pelvis | 63/m | 259×259×283 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0010` |
| `s0011` | ct neck-thorax-abdomen-pelvis | 49/m | 311×311×431 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0011` |
| `s0012` | ct abdomen-pelvis | 53/f | 238×145×315 | 1.5 | 1.45 / 1.78 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0012` |
| `s0013` | ct abdomen-pelvis | 63/m | 231×175×301 | 0.0 | 0.0 / 3.83 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0013` |
| `s0014` | ct abdomen-pelvis | 70/f | 286×286×308 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0014` |
| `s0015` | ct abdomen-pelvis | 56/m | 293×293×344 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0015` |
| `s0016` | ct angiography abdomen-pelvis-leg | 83/m | 259×259×294 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0016` |
| `s0019` | ct aortic valve | 78/m | 221×221×407 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0019` |
| `s0022` | ct abdomen-pelvis | 50/m | 295×295×250 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0022` |
| `s0024` | ct thorax-abdomen | 69/m | 287×287×486 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0024` |
| `s0028` | ct neck-thorax-abdomen-pelvis | 59/f | 299×299×394 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0028` |
| `s0029` | ct thorax-abdomen-pelvis | 64/m | 300×300×422 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0029` |
| `s0030` | ct abdomen-pelvis | 76/f | 309×309×317 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0030` |
| `s0031` | ct angiography abdomen-pelvis-leg | 82/m | 288×288×280 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0031` |
| `s0034` | ct angiography pelvis-leg | 60/f | 308×308×283 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0034` |
| `s0038` | ct neck-thorax-abdomen-pelvis | 52/f | 240×240×333 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0038` |
| `s0040` | ct thorax-abdomen-pelvis | 52/m | 291×291×421 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0040` |
| `s0042` | ct abdomen-pelvis | 53/m | 285×285×332 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0042` |
| `s0045` | ct angiography abdomen-pelvis-leg | 70/m | 231×231×306 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0045` |
| `s0049` | ct aortic valve | 68/m | 221×221×455 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0049` |
| `s0050` | ct thorax-abdomen | 31/m | 261×182×399 | 0.0 | 0.0 / 2.84 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0050` |
| `s0052` | ct abdomen-pelvis | 78/m | 249×249×294 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0052` |
| `s0053` | ct abdomen-pelvis | 30/m | 333×333×346 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0053` |
| `s0054` | ct abdomen-pelvis | 74/f | 261×261×273 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0054` |
| `s0058` | ct thorax-abdomen-pelvis | 69/f | 298×298×325 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0058` |
| `s0059` | ct thorax-abdomen-pelvis | 48/f | 233×233×449 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0059` |
| `s0062` | ct pelvis | 63/m | 273×205×273 | 7.4 | 7.47 / 7.53 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0062` |
| `s0065` | ct angiography neck-thx-abd-pelvis-leg | 76/m | 243×243×409 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0065` |
| `s0066` | ct abdomen-pelvis | 53/f | 260×202×217 | 1.5 | 1.51 / 1.54 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0066` |
| `s0068` | ct spine | 73/f | 104×137×189 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0068` |
| `s0072` | ct abdomen-pelvis | 67/m | 286×286×269 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0072` |
| `s0073` | ct abdomen-pelvis | 71/m | 247×245×265 | 0.0 | 0.0 / 1.65 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0073` |
| `s0074` | ct pelvis | 50/m | 251×192×180 | 19.5 | 0.0 / 0.75 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0074` |
| `s0075` | ct abdomen-pelvis | 58/m | 220×363×317 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0075` |
| `s0076` | ct aortic valve | 45/m | 319×319×441 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0076` |
| `s0077` | ct abdomen-pelvis | 77/m | 255×255×272 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0077` |
| `s0078` | ct abdomen-pelvis | 75/m | 271×271×266 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0078` |
| `s0082` | ct abdomen-pelvis | 39/f | 260×205×311 | 0.0 | 0.37 / 2.79 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0082` |
| `s0086` | ct neck-thorax-abdomen-pelvis | 71/m | 273×430×430 | 10.5 | 0.0 / 0.46 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0086` |
| `s0089` | ct neck-thorax-abdomen-pelvis | 75/f | 221×221×263 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0089` |
| `s0090` | ct abdomen-pelvis | 72/f | 258×158×275 | 3.0 | 0.0 / 0.33 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0090` |
| `s0091` | ct angiography neck-thx-abd-pelvis-leg | 83/m | 312×312×437 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0091` |
| `s0092` | ct abdomen-pelvis | 24/m | 243×167×203 | 1.5 | 3.9 / 4.46 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0092` |
| `s0095` | ct aortic valve | 73/m | 221×221×413 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0095` |
| `s0096` | ct angiography abdomen-pelvis-leg | 80/m | 259×259×310 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0096` |
| `s0098` | ct abdomen-pelvis | 50/m | 231×170×231 | 6.0 | 0.99 / 1.29 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0098` |
| `s0104` | ct pelvis | 84/f | 295×295×202 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0104` |
| `s0107` | ct abdomen-pelvis | 87/f | 499×154×333 | 0.0 | 0.0 / 7.23 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0107` |
| `s0108` | ct neck-thorax-abdomen-pelvis | 51/m | 288×288×293 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0108` |
| `s0109` | ct abdomen-pelvis | 76/m | 303×252×303 | 10.5 | 0.0 / 1.23 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0109` |
| `s0117` | ct abdomen-pelvis | 50/m | 259×156×266 | 0.0 | 2.77 / 3.73 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0117` |
| `s0119` | ct abdomen-pelvis | 43/m | 293×293×343 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0119` |
| `s0120` | ct abdomen-pelvis | 60/m | 261×254×355 | 9.0 | 4.89 / 5.06 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0120` |
| `s0122` | ct lower limb left | 54/f | 93×93×83 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0122` |
| `s0124` | ct angiography abdomen-pelvis-leg | 70/m | 431×159×431 | 0.0 | 0.0 / 0.71 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0124` |
| `s0131` | ct abdomen-pelvis | 38/m | 265×202×245 | 10.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0131` |
| `s0133` | ct abdomen-pelvis | 69/m | 307×307×301 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0133` |
| `s0137` | ct abdomen-pelvis | 67/f | 247×247×265 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0137` |
| `s0139` | ct abdomen-pelvis | 30/f | 280×280×285 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0139` |
| `s0141` | ct neck-thorax-abdomen-pelvis | 57/f | 406×168×406 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0141` |
| `s0143` | ct abdomen-pelvis | 22/m | 279×166×267 | 0.0 | 0.0 / 2.56 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0143` |
| `s0145` | ct angiography neck-thx-abd-pelvis-leg | 54/m | 222×219×447 | 0.0 | 0.0 / 2.5 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0145` |
| `s0147` | ct chest | 55/m | 220×310×310 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0147` |
| `s0150` | ct abdomen | 78/m | 247×247×296 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0150` |
| `s0151` | ct abdomen-pelvis | 67/m | 292×184×292 | 39.0 | 0.0 / 0.92 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0151` |
| `s0153` | ct abdomen-pelvis | 38/m | 248×195×299 | 0.0 | 0.0 / 2.16 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0153` |
| `s0157` | ct abdomen-pelvis | 77/f | 314×314×261 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0157` |
| `s0158` | ct abdomen-pelvis | 50/m | 253×253×271 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0158` |
| `s0161` | ct abdomen-pelvis | 53/m | 246×181×300 | 0.0 | 3.17 / 3.48 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0161` |
| `s0163` | ct angiography neck-thx-abd-pelvis-leg | 53/f | 291×291×440 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0163` |
| `s0166` | ct abdomen-pelvis | 58/m | 200×321×321 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0166` |
| `s0168` | ct abdomen | 64/f | 229×229×280 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0168` |
| `s0171` | ct angiography abdomen-pelvis-leg | 78/f | 222×222×409 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0171` |
| `s0174` | ct neck-thorax-abdomen-pelvis | 59/f | 194×133×253 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0174` |
| `s0178` | ct abdomen-pelvis | 61/m | 261×261×310 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0178` |
| `s0179` | ct abdomen | 39/f | 248×248×247 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0179` |
| `s0182` | ct abdomen-pelvis | 47/m | 239×239×303 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0182` |
| `s0183` | ct abdomen | 69/m | 331×201×331 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0183` |
| `s0188` | ct pelvis | 60/f | 311×311×219 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0188` |
| `s0189` | ct aortic valve | 50/m | 254×174×440 | 0.0 | 0.0 / 1.57 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0189` |
| `s0190` | ct thorax-abdomen-pelvis | 59/f | 257×257×237 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0190` |
| `s0192` | ct neck-thorax-abdomen-pelvis | 30/f | 239×136×296 | 0.0 | 0.0 / 2.12 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0192` |
| `s0193` | ct abdomen-pelvis | 63/m | 278×174×278 | 0.0 | 0.0 / 1.6 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0193` |
| `s0194` | ct abdomen | 67/f | 227×227×279 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0194` |
| `s0196` | ct angiography neck-thx-abd-pelvis-leg | 73/f | 295×295×367 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0196` |
| `s0201` | ct abdomen-pelvis | 51/f | 293×293×219 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0201` |
| `s0204` | ct abdomen-pelvis | 36/m | 290×236×321 | 6.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0204` |
| `s0206` | ct abdomen | 81/f | 240×240×288 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0206` |
| `s0210` | ct angiography abdomen-pelvis-leg | 51/m | 333×333×317 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0210` |
| `s0212` | ct thorax-abdomen | 66/m | 262×184×327 | 12.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0212` |
| `s0216` | ct angiography abdomen-pelvis-leg | 38/f | 288×288×179 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0216` |
| `s0218` | ct abdomen-pelvis | 69/f | 265×265×273 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0218` |
| `s0224` | ct polytrauma | 47/m | 333×333×621 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0224` |
| `s0227` | ct thorax-abdomen-pelvis | 65/f | 264×264×247 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0227` |
| `s0228` | ct abdomen-pelvis | 42/m | 254×267×267 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0228` |
| `s0229` | ct  intervention | 52/m | 269×269×108 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0229` |
| `s0232` | ct abdomen | 82/m | 279×148×279 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0232` |
| `s0235` | ct abdomen-pelvis | 44/f | 202×163×245 | 0.0 | 0.0 / 0.48 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0235` |
| `s0236` | ct abdomen-pelvis | 78/m | 252×198×308 | 0.0 | 0.0 / 3.46 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0236` |
| `s0238` | ct abdomen-pelvis | 17/m | 231×134×281 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0238` |
| `s0239` | ct angiography abdomen-pelvis-leg | 48/m | 283×283×412 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0239` |
| `s0242` | ct abdomen-pelvis | 19/f | 248×194×250 | 0.0 | 0.0 / 4.63 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0242` |
| `s0243` | ct abdomen-pelvis | 56/m | 253×253×285 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0243` |
| `s0244` | ct abdomen-pelvis | 85/m | 288×288×287 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0244` |
| `s0248` | ct neck-thorax-abdomen-pelvis | 71/f | 299×299×280 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0248` |
| `s0250` | ct neck-thorax-abdomen-pelvis | 67/m | 307×307×301 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0250` |
| `s0252` | ct abdomen-pelvis | 40/f | 227×227×267 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0252` |
| `s0255` | ct abdomen-pelvis | 57/f | 261×261×306 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0255` |
| `s0257` | ct angiography abdomen-pelvis-leg | 79/f | 233×233×263 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0257` |
| `s0260` | ct neck-thorax-abdomen-pelvis | 48/f | 366×424×283 | 0.0 | 0.0 / 7.49 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0260` |
| `s0261` | ct abdomen | 53/m | 223×223×191 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0261` |
| `s0287` | ct angiography neck-thx-abd-pelvis-leg | 80/m | 317×317×835 | 292.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0287` |
| `s0291` | ct neck-thorax-abdomen-pelvis | 65/f | 251×226×251 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0291` |
| `s0293` | ct abdomen | 77/m | 240×240×329 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0293` |
| `s0298` | ct abdomen-pelvis | 66/m | 348×206×348 | 88.5 | 0.0 / 1.49 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0298` |
| `s0300` | ct thorax-abdomen-pelvis | 76/m | 325×325×278 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0300` |
| `s0301` | ct pelvis | 76/m | 253×138×253 | 15.0 | 0.0 / 2.6 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0301` |
| `s0304` | ct abdomen-pelvis | 72/m | 234×231×231 | 3.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0304` |
| `s0306` | ct abdomen-pelvis | 51/m | 245×202×225 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0306` |
| `s0307` | ct neck-thorax-abdomen-pelvis | 69/f | 240×240×287 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0307` |
| `s0308` | ct angiography abdomen-pelvis-leg | 62/m | 271×166×391 | 1.5 | 0.0 / 2.27 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0308` |
| `s0311` | ct abdomen-pelvis | 55/m | 292×164×292 | 3.0 | 0.0 / 3.11 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0311` |
| `s0314` | ct abdomen-pelvis | 83/m | 299×299×273 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0314` |
| `s0319` | ct abdomen-pelvis | 67/m | 270×213×338 | 1.5 | 4.73 / 6.94 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0319` |
| `s0320` | ct angiography abdomen-pelvis-leg | 80/m | 272×272×282 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0320` |
| `s0321` | ct neck-thorax-abdomen-pelvis | 60/f | 272×272×303 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0321` |
| `s0324` | ct abdomen-pelvis | 54/f | 201×201×261 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0324` |
| `s0325` | ct abdomen-pelvis | 65/f | 315×315×293 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0325` |
| `s0327` | ct angiography neck-thx-abd-pelvis-leg | 72/m | 222×166×429 | 0.0 | 0.0 / 3.55 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0327` |
| `s0328` | ct abdomen-pelvis | 60/m | 265×265×269 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0328` |
| `s0329` | ct abdomen-pelvis | 46/m | 304×329×329 | 0.0 | 3.67 / 3.67 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0329` |
| `s0332` | ct thorax-abdomen-pelvis | 68/m | 269×269×505 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0332` |
| `s0334` | ct neck-thorax-abdomen-pelvis | 48/m | 291×291×469 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0334` |
| `s0339` | ct abdomen | 83/f | 240×240×281 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0339` |
| `s0341` | ct thorax-abdomen | 75/m | 247×247×275 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0341` |
| `s0342` | ct abdomen | 60/f | 233×233×284 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0342` |
| `s0344` | ct thorax-abdomen-pelvis | 57/m | 283×283×384 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0344` |
| `s0345` | ct spine | 53/m | 238×261×476 | 6.0 | 0.0 / 3.59 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0345` |
| `s0350` | ct thorax-abdomen-pelvis | 88/f | 266×266×415 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0350` |
| `s0355` | ct thorax-abdomen | 60/m | 259×259×325 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0355` |
| `s0358` | ct pelvis | 90/f | 255×255×523 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0358` |
| `s0361` | ct angiography pelvis-leg | 81/m | 299×190×483 | 88.5 | 0.0 / 2.77 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0361` |
| `s0362` | ct neck-thorax-abdomen-pelvis | 82/m | 285×285×418 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0362` |
| `s0366` | ct abdomen | 48/f | 325×325×270 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0366` |
| `s0369` | ct neck-thorax-abdomen-pelvis | 70/m | 248×248×439 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0369` |
| `s0370` | ct thorax-abdomen-pelvis | 60/m | 437×202×437 | 9.0 | 0.0 / 0.5 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0370` |
| `s0375` | ct thorax-abdomen-pelvis | 46/m | 259×259×440 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0375` |
| `s0381` | ct pelvis | 78/f | 219×219×192 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0381` |
| `s0383` | ct thorax-abdomen-pelvis | 80/f | 247×145×385 | 6.0 | 0.0 / 1.53 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0383` |
| `s0385` | ct pelvis | 64/f | 243×243×182 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0385` |
| `s0390` | ct abdomen | 73/m | 247×247×299 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0390` |
| `s0399` | ct pelvis | 49/f | 227×227×157 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0399` |
| `s0401` | ct thorax-abdomen | 66/f | 233×233×295 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0401` |
| `s0402` | ct thorax-abdomen-pelvis | 74/f | 264×264×429 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0402` |
| `s0403` | ct heart-thorakale aorta | 84/f | 267×267×391 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0403` |
| `s0406` | ct abdomen | 66/m | 278×278×316 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0406` |
| `s0408` | ct neck-thorax-abdomen-pelvis | 88/m | 279×279×391 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0408` |
| `s0416` | ct abdomen | 30/m | 212×212×328 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0416` |
| `s0418` | ct pelvis | 57/f | 235×235×62 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0418` |
| `s0419` | ct pelvis | 60/m | 288×288×164 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0419` |
| `s0423` | ct neck-thorax-abdomen-pelvis | 65/m | 272×272×441 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0423` |
| `s0426` | ct pelvis | 42/m | 239×239×156 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0426` |
| `s0428` | ct thorax-abdomen | 24/m | 307×307×317 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0428` |
| `s0429` | ct thorax-abdomen-pelvis | 48/f | 254×254×433 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0429` |
| `s0436` | ct thorax-abdomen-pelvis | 57/f | 238×158×421 | 0.0 | 0.26 / 1.81 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0436` |
| `s0440` | ct thorax-abdomen-pelvis | 55/f | 255×255×422 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0440` |
| `s0441` | ct abdomen | 72/m | 242×242×303 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0441` |
| `s0442` | ct pelvis | 74/m | 264×264×178 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0442` |
| `s0443` | ct pelvis | 35/m | 263×263×92 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0443` |
| `s0446` | ct thorax-abdomen-pelvis | 34/m | 239×239×427 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0446` |
| `s0447` | ct thorax-abdomen-pelvis | 76/f | 285×285×427 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0447` |
| `s0450` | ct abdomen | 51/m | 213×213×238 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0450` |
| `s0454` | ct pelvis | 83/f | 311×311×221 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0454` |
| `s0456` | ct neck-thorax-abdomen-pelvis | 69/f | 265×265×390 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0456` |
| `s0458` | ct abdomen | 83/f | 243×243×291 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0458` |
| `s0461` | ct neck-thorax-abdomen-pelvis | 58/m | 269×269×447 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0461` |
| `s0462` | ct spine | 80/f | 194×193×336 | 1.5 | 4.0 / 4.07 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0462` |
| `s0467` | ct angiography neck-thx-abd-pelvis-leg | 50/m | 320×320×495 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0467` |
| `s0468` | ct angiography pelvis-leg | 76/m | 233×233×838 | 432.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0468` |
| `s0472` | ct thorax-abdomen-pelvis | 64/f | 296×296×393 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0472` |
| `s0473` | ct thorax-abdomen | 73/m | 243×243×289 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0473` |
| `s0475` | ct pelvis | 49/f | 277×277×225 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0475` |
| `s0476` | ct neck-thorax-abdomen-pelvis | 61/m | 283×283×463 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0476` |
| `s0477` | ct neck-thorax-abdomen-pelvis | 68/f | 243×243×404 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0477` |
| `s0480` | ct abdomen | 74/m | 233×233×328 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0480` |
| `s0483` | ct thorax-abdomen-pelvis | 51/f | 266×266×444 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0483` |
| `s0484` | ct thorax-abdomen-pelvis | 64/m | 241×241×421 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0484` |
| `s0494` | ct thorax-abdomen | 56/f | 233×233×435 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0494` |
| `s0495` | ct abdomen | 64/f | 247×247×332 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0495` |
| `s0499` | ct thorax-abdomen | 61/f | 233×233×371 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0499` |
| `s0500` | ct neck-thorax-abdomen-pelvis | 49/m | 307×307×354 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0500` |
| `s0502` | ct thorax-abdomen-pelvis | 70/m | 215×215×402 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0502` |
| `s0505` | ct abdomen | 78/m | 366×252×366 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0505` |
| `s0506` | ct pelvis | 58/m | 231×231×177 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0506` |
| `s0507` | ct neck-thorax-abdomen-pelvis | 69/m | 252×252×441 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0507` |
| `s0509` | ct thorax-abdomen | 49/m | 227×227×315 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0509` |
| `s0513` | ct abdomen | 65/f | 307×307×342 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0513` |
| `s0516` | ct thorax-abdomen-pelvis | 88/m | 253×253×413 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0516` |
| `s0519` | ct thorax-abdomen-pelvis | 64/m | 283×283×433 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0519` |
| `s0529` | ct abdomen | 33/f | 247×247×304 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0529` |
| `s0536` | ct neck-thorax-abdomen-pelvis | 65/m | 269×269×426 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0536` |
| `s0541` | ct abdomen | 71/m | 319×319×320 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0541` |
| `s0542` | ct thorax-abdomen | 69/m | 232×311×311 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0542` |
| `s0543` | ct thorax-abdomen-pelvis | 54/f | 301×301×463 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0543` |
| `s0546` | ct thorax-abdomen-pelvis | 64/m | 304×304×458 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0546` |
| `s0548` | ct neck-thorax-abdomen-pelvis | 65/m | 337×171×337 | 1.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0548` |
| `s0549` | ct thorax-abdomen-pelvis | 69/m | 305×305×461 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0549` |
| `s0550` | ct neck-thorax-abdomen-pelvis | 62/m | 294×294×462 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0550` |
| `s0551` | ct neck-thorax-abdomen-pelvis | 82/f | 233×233×352 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0551` |
| `s0553` | ct thorax-abdomen | 45/f | 233×233×414 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0553` |
| `s0561` | ct neck-thorax-abdomen-pelvis | 76/m | 243×155×415 | 9.0 | 0.0 / 4.09 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0561` |
| `s0564` | ct pelvis | 64/f | 215×215×152 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0564` |
| `s0566` | ct thorax-abdomen | 48/m | 319×161×319 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0566` |
| `s0571` | ct thorax-abdomen-pelvis | 55/f | 243×243×409 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0571` |
| `s0573` | ct pelvis | 88/f | 224×224×247 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0573` |
| `s0574` | ct neck-thorax-abdomen-pelvis | 51/m | 251×251×441 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0574` |
| `s0577` | ct thorax-abdomen-pelvis | 42/m | 284×284×319 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0577` |
| `s0578` | ct thorax-abdomen-pelvis | 67/f | 224×224×395 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0578` |
| `s0581` | ct pelvis | 72/f | 233×233×198 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0581` |
| `s0583` | ct thorax-abdomen | 49/m | 232×232×424 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0583` |
| `s0584` | ct abdomen | 74/f | 256×256×228 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0584` |
| `s0585` | ct aortic valve | 79/m | 245×195×456 | 0.0 | 0.99 / 3.51 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0585` |
| `s0586` | ct thorax-abdomen | 58/m | 247×247×257 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0586` |
| `s0587` | ct neck-thorax-abdomen-pelvis | 83/f | 300×300×410 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0587` |
| `s0589` | ct thorax-abdomen-pelvis | 58/f | 265×265×423 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0589` |
| `s0591` | ct polytrauma | 74/m | 275×275×531 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0591` |
| `s0592` | ct polytrauma | 86/f | 319×319×403 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0592` |
| `s0593` | ct neck-thorax-abdomen-pelvis | 55/f | 299×299×426 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0593` |
| `s0600` | ct thorax-abdomen-pelvis | 65/m | 227×227×298 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0600` |
| `s0601` | ct pelvis | 54/m | 218×218×150 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0601` |
| `s0602` | ct thorax-abdomen-pelvis | 44/f | 248×165×419 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0602` |
| `s0603` | ct angiography pelvis-leg | 75/f | 254×254×397 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0603` |
| `s0607` | ct thorax-abdomen | 61/m | 235×338×338 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0607` |
| `s0611` | ct pelvis | 62/m | 207×121×169 | 1.5 | 1.65 / 1.8 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0611` |
| `s0612` | ct neck-thorax-abdomen-pelvis | 73/f | 272×272×411 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0612` |
| `s0613` | ct neck-thorax-abdomen-pelvis | 67/m | 272×272×417 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0613` |
| `s0614` | ct pelvis | 60/m | 205×108×161 | 1.5 | 5.09 / 5.15 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0614` |
| `s0616` | ct polytrauma | 56/m | 337×261×644 | 1.5 | 2.96 / 3.53 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0616` |
| `s0617` | ct thorax-abdomen-pelvis | 57/m | 251×251×431 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0617` |
| `s0619` | ct thorax-abdomen-pelvis | 79/f | 225×155×395 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0619` |
| `s0620` | ct abdomen | 67/m | 245×245×300 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0620` |
| `s0621` | ct thorax-abdomen-pelvis | 85/m | 270×310×453 | 16.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0621` |
| `s0623` | ct neck-thorax-abdomen-pelvis | 89/m | 243×243×442 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0623` |
| `s0624` | ct aortic valve | 81/m | 230×186×517 | 1.5 | 0.85 / 5.28 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0624` |
| `s0625` | ct neck-thorax-abdomen-pelvis | 54/m | 263×198×423 | 1.5 | 5.21 / 5.2 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0625` |
| `s0626` | ct thorax-abdomen | 60/f | 227×227×396 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0626` |
| `s0628` | ct thorax-abdomen-pelvis | 61/f | 271×271×418 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0628` |
| `s0629` | ct thorax-abdomen-pelvis | 66/m | 244×244×440 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0629` |
| `s0633` | ct upper leg left | 80/f | 166×166×326 | 42.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0633` |
| `s0635` | ct thorax-abdomen-pelvis | 76/f | 291×291×381 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0635` |
| `s0636` | ct thorax-abdomen | 59/f | 277×277×477 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0636` |
| `s0637` | ct thorax-abdomen-pelvis | 72/m | 245×245×437 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0637` |
| `s0639` | ct neck-thorax-abdomen-pelvis | 58/f | 313×208×425 | 0.0 | 0.0 / 2.36 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0639` |
| `s0644` | ct neck-thorax-abdomen-pelvis | 66/f | 272×272×389 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0644` |
| `s0646` | ct pelvis | 81/m | 275×275×215 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0646` |
| `s0647` | ct abdomen | 15/f | 111×106×257 | 0.0 | 1.88 / 1.89 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0647` |
| `s0648` | ct neck-thorax-abdomen-pelvis | 72/f | 272×198×401 | 3.0 | 0.0 / 0.9 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0648` |
| `s0649` | ct thorax-abdomen-pelvis | 63/f | 265×265×388 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0649` |
| `s0650` | ct thorax-abdomen-pelvis | 65/m | 333×333×462 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0650` |
| `s0656` | ct thorax-abdomen-pelvis | 71/f | 242×221×416 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0656` |
| `s0657` | ct neck-thorax-abdomen-pelvis | 53/m | 297×297×488 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0657` |
| `s0661` | ct neck-thorax-abdomen-pelvis | 90/f | 252×252×390 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0661` |
| `s0662` | ct neck-thorax-abdomen-pelvis | 51/m | 267×267×419 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0662` |
| `s0663` | ct neck-thorax-abdomen-pelvis | 61/f | 255×255×418 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0663` |
| `s0664` | ct thorax-abdomen | 74/f | 332×332×405 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0664` |
| `s0666` | ct  intervention | 53/m | 259×259×56 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0666` |
| `s0667` | ct neck-thorax-abdomen-pelvis | 65/m | 295×295×454 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0667` |
| `s0668` | ct abdomen | 51/f | 183×183×246 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0668` |
| `s0669` | ct angiography neck-thx-abd-pelvis-leg | 82/m | 299×299×443 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0669` |
| `s0670` | ct neck-thorax-abdomen-pelvis | 84/f | 228×228×396 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0670` |
| `s0673` | ct pelvis | 54/m | 207×207×205 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0673` |
| `s0680` | ct thorax-abdomen-pelvis | 73/m | 289×289×449 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0680` |
| `s0682` | ct thorax-abdomen | 44/m | 206×317×317 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0682` |
| `s0683` | ct thorax-abdomen | 75/m | 309×309×288 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0683` |
| `s0685` | ct angiography pelvis-leg | 47/m | 263×263×520 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0685` |
| `s0686` | ct neck-thorax-abdomen-pelvis | 74/m | 265×265×407 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0686` |
| `s0687` | ct thorax-abdomen-pelvis | 60/f | 212×212×399 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0687` |
| `s0690` | ct polytrauma | 94/f | 281×162×538 | 0.0 | 0.0 / 0.71 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0690` |
| `s0692` | ct angiography neck-thx-abd-pelvis-leg | 66/m | 333×333×449 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0692` |
| `s0694` | ct thorax-abdomen | 32/m | 233×233×286 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0694` |
| `s0699` | ct neck-thorax-abdomen-pelvis | 80/f | 248×248×258 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0699` |
| `s0702` | ct heart | 81/m | 221×221×379 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0702` |
| `s0703` | ct thorax-abdomen-pelvis | 79/m | 265×265×434 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0703` |
| `s0705` | ct thorax-abdomen | 67/m | 267×267×301 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0705` |
| `s0707` | ct angiography neck-thx-abd-pelvis-leg | 74/m | 267×267×298 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0707` |
| `s0708` | ct neck-thorax-abdomen-pelvis | 36/f | 235×235×404 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0708` |
| `s0711` | ct thorax-abdomen | 64/m | 213×213×433 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0711` |
| `s0712` | ct pelvis | 81/f | 207×123×167 | 1.5 | 1.28 / 6.1 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0712` |
| `s0720` | ct neck-thorax-abdomen-pelvis | 30/m | 307×307×445 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0720` |
| `s0721` | ct abdomen | 33/f | 227×227×265 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0721` |
| `s0723` | ct neck-thorax-abdomen-pelvis | 71/f | 222×187×415 | 7.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0723` |
| `s0724` | ct angiography pelvis-leg | 74/m | 259×259×299 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0724` |
| `s0726` | ct thorax-abdomen-pelvis | 69/f | 273×273×401 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0726` |
| `s0727` | ct polytrauma | 88/f | 247×247×525 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0727` |
| `s0728` | ct thorax-abdomen | 51/f | 195×195×270 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0728` |
| `s0730` | ct pelvis | 80/f | 214×214×74 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0730` |
| `s0731` | ct thorax-abdomen-pelvis | 57/f | 299×299×427 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0731` |
| `s0733` | ct neck-thorax-abdomen-pelvis | 66/m | 283×283×434 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0733` |
| `s0737` | ct thorax-abdomen | 71/f | 230×155×413 | 0.0 | 0.0 / 3.33 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0737` |
| `s0739` | ct thorax-abdomen-pelvis | 37/m | 263×263×440 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0739` |
| `s0746` | ct abdomen | 62/m | 233×233×294 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0746` |
| `s0749` | ct abdomen | 75/m | 260×260×313 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0749` |
| `s0751` | ct angiography neck-thx-abd-pelvis-leg | 66/m | 292×192×527 | 0.0 | 0.53 / 0.88 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0751` |
| `s0754` | ct polytrauma | 60/m | 376×202×649 | 0.0 | 0.0 / 1.62 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0754` |
| `s0760` | ct thorax-abdomen-pelvis | 42/f | 247×247×288 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0760` |
| `s0762` | ct thorax-abdomen-pelvis | 65/m | 312×312×323 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0762` |
| `s0763` | ct polytrauma | 58/m | 292×292×514 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0763` |
| `s0764` | ct neck-thorax-abdomen-pelvis | 75/f | 291×291×405 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0764` |
| `s0765` | ct thorax-abdomen-pelvis | 54/f | 228×228×396 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0765` |
| `s0768` | ct abdomen | 75/f | 219×174×258 | 1.5 | 0.18 / 0.72 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0768` |
| `s0772` | ct pelvis | 66/m | 249×123×201 | 0.0 | 1.01 / 2.59 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0772` |
| `s0774` | ct angiography pelvis-leg | 75/f | 258×206×626 | 397.5 | 0.0 / 2.22 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0774` |
| `s0776` | ct pelvis | 85/f | 227×133×173 | 0.0 | 0.41 / 5.66 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0776` |
| `s0777` | ct polytrauma | 74/m | 305×305×536 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0777` |
| `s0778` | ct thorax-abdomen-pelvis | 57/m | 262×262×431 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0778` |
| `s0783` | ct upper leg left | 57/m | 165×159×659 | 486.3 | 8.73 / 4.52 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0783` |
| `s0790` | ct polytrauma | 40/f | 291×291×538 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0790` |
| `s0794` | ct thorax-abdomen-pelvis | 60/f | 283×283×447 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0794` |
| `s0796` | ct neck-thorax-abdomen-pelvis | 82/m | 281×281×410 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0796` |
| `s0797` | ct neck-thorax-abdomen-pelvis | 63/f | 264×264×433 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0797` |
| `s0801` | ct thorax-abdomen-pelvis | 57/m | 256×171×445 | 0.0 | 0.0 / 1.27 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0801` |
| `s0804` | ct angiography neck-thx-abd-pelvis-leg | 44/f | 236×236×467 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0804` |
| `s0806` | ct neck-thorax-abdomen-pelvis | 72/m | 306×255×387 | 0.0 | 1.89 / 6.91 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0806` |
| `s0807` | ct thorax-abdomen-pelvis | 65/m | 250×250×454 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0807` |
| `s0815` | ct pelvis | 88/m | 253×253×267 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0815` |
| `s0818` | ct pelvis | 34/m | 216×216×165 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0818` |
| `s0819` | ct pelvis | 81/f | 212×212×157 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0819` |
| `s0830` | ct neck-thorax-abdomen-pelvis | 84/m | 248×248×440 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0830` |
| `s0831` | ct angiography pelvis-leg | 63/m | 253×253×524 | 45.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0831` |
| `s0835` | ct neck-thorax-abdomen-pelvis | 53/f | 264×264×419 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0835` |
| `s0836` | ct thorax-abdomen-pelvis | 62/f | 231×231×439 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0836` |
| `s0837` | ct upper leg left | 76/f | 77×101×185 | 47.9 | 4.27 / 4.24 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0837` |
| `s0842` | ct thorax-abdomen-pelvis | 64/f | 245×245×450 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0842` |
| `s0850` | ct upper leg left | 63/f | 133×133×275 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0850` |
| `s0859` | ct thorax-abdomen-pelvis | 70/f | 241×241×450 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0859` |
| `s0860` | ct polytrauma | 56/m | 332×207×449 | 0.0 | 0.0 / 0.34 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0860` |
| `s0861` | ct  operation | 66/f | 332×332×140 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0861` |
| `s0862` | ct pelvis | 73/m | 272×272×183 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0862` |
| `s0863` | ct neck-thorax-abdomen-pelvis | 61/f | 263×263×435 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0863` |
| `s0864` | ct thorax-abdomen-pelvis | 57/f | 286×286×391 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0864` |
| `s0869` | ct neck-thorax-abdomen-pelvis | 75/m | 261×261×459 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0869` |
| `s0873` | ct pelvis | 63/f | 207×207×179 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0873` |
| `s0878` | ct neck-thorax-abdomen-pelvis | 66/m | 274×274×427 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0878` |
| `s0880` | ct thorax-abdomen-pelvis | 72/m | 291×291×463 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0880` |
| `s0884` | ct thorax-abdomen-pelvis | 56/m | 272×272×455 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0884` |
| `s0885` | ct thorax-abdomen-pelvis | 63/m | 259×259×448 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0885` |
| `s0894` | ct polytrauma | 35/m | 269×269×445 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0894` |
| `s0895` | ct angiography neck-thx-abd-pelvis-leg | 56/m | 231×231×218 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0895` |
| `s0896` | ct neck-thorax-abdomen-pelvis | 21/f | 261×261×431 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0896` |
| `s0899` | ct thorax-abdomen-pelvis | 74/m | 286×286×456 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0899` |
| `s0903` | ct polytrauma | 87/m | 333×333×427 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0903` |
| `s0904` | ct thorax-abdomen-pelvis | 82/m | 275×215×433 | 0.0 | 0.0 / 0.34 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0904` |
| `s0907` | ct pelvis | 73/m | 267×267×262 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0907` |
| `s0912` | ct neck-thorax-abdomen-pelvis | 68/m | 272×272×451 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0912` |
| `s0913` | ct neck-thorax-abdomen-pelvis | 29/m | 289×289×465 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0913` |
| `s0915` | ct thorax-abdomen-pelvis | 71/m | 227×227×423 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0915` |
| `s0916` | ct thorax-abdomen-pelvis | 65/m | 234×234×442 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0916` |
| `s0918` | ct neck-thorax-abdomen-pelvis | 67/m | 252×252×459 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0918` |
| `s0919` | ct aortic valve | 65/f | 221×168×415 | 1.5 | 0.07 / 0.33 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0919` |
| `s0921` | ct pelvis | 51/m | 259×259×94 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0921` |
| `s0923` | ct neck-thorax-abdomen-pelvis | 43/f | 281×281×500 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0923` |
| `s0924` | ct neck-thorax-abdomen-pelvis | 63/m | 287×287×434 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0924` |
| `s0927` | ct polytrauma | 93/f | 333×333×519 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0927` |
| `s0928` | ct thorax-abdomen-pelvis | 77/f | 282×282×417 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0928` |
| `s0931` | ct angiography neck-thx-abd-pelvis-leg | 86/m | 245×187×479 | 1.5 | 1.15 / 2.96 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0931` |
| `s0933` | ct polytrauma | 55/f | 328×268×660 | 21.0 | 0.0 / 0.99 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0933` |
| `s0937` | ct angiography pelvis-leg | 79/m | 253×253×548 | 34.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0937` |
| `s0939` | ct thorax-abdomen-pelvis | 73/m | 233×233×440 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0939` |
| `s0940` | ct neck-thorax-abdomen-pelvis | 77/m | 231×231×439 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0940` |
| `s0943` | ct upper leg left | 69/m | 289×289×252 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0943` |
| `s0944` | ct pelvis | 61/f | 303×127×303 | 0.0 | 1.61 / 4.83 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0944` |
| `s0945` | ct thorax-abdomen-pelvis | 68/f | 285×285×399 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0945` |
| `s0947` | ct pelvis | 28/m | 207×207×167 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0947` |
| `s0950` | ct thorax-abdomen-pelvis | 79/f | 232×232×399 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0950` |
| `s0951` | ct pelvis | 72/f | 243×243×227 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0951` |
| `s0957` | ct neck-thorax-abdomen-pelvis | 50/f | 248×248×437 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0957` |
| `s0959` | ct thorax-abdomen-pelvis | 47/f | 233×233×413 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0959` |
| `s0961` | ct thorax-abdomen-pelvis | 56/m | 253×253×413 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0961` |
| `s0962` | ct  operation | 78/f | 308×308×98 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0962` |
| `s0963` | ct thorax-abdomen-pelvis | 59/f | 208×208×402 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0963` |
| `s0965` | ct thorax-abdomen-pelvis | 64/f | 215×215×309 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0965` |
| `s0971` | ct pelvis | 67/m | 285×285×183 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0971` |
| `s0979` | ct abdomen | 74/m | 432×193×349 | 1.5 | 0.0 / 4.43 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0979` |
| `s0981` | ct pelvis | 94/m | 221×221×174 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0981` |
| `s0982` | ct thorax-abdomen-pelvis | 56/m | 347×247×513 | 15.0 | 0.5 / 10.18 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0982` |
| `s0983` | ct thorax-abdomen-pelvis | 67/m | 241×241×433 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0983` |
| `s0985` | ct neck-thorax-abdomen-pelvis | 73/f | 248×248×389 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0985` |
| `s0986` | ct polytrauma | 88/f | 331×145×571 | 1.5 | 0.0 / 0.88 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0986` |
| `s0988` | ct polytrauma | 79/m | 333×333×273 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0988` |
| `s0991` | ct thorax-abdomen-pelvis | 71/f | 208×208×411 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0991` |
| `s0992` | ct neck-thorax-abdomen-pelvis | 54/f | 250×250×413 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0992` |
| `s0994` | ct neck-thorax-abdomen-pelvis | 79/m | 245×245×427 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s0994` |
| `s1000` | ct upper leg left | 47/m | 229×135×161 | 0.0 | 0.65 / 1.83 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1000` |
| `s1005` | ct pelvis | 57/m | 217×121×197 | 0.0 | 0.13 / 0.63 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1005` |
| `s1006` | ct thorax-abdomen-pelvis | 67/f | 264×264×415 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1006` |
| `s1008` | ct thorax-abdomen-pelvis | 76/f | 249×249×382 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1008` |
| `s1009` | ct pelvis | 77/m | 253×131×177 | 0.0 | 0.0 / 0.39 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1009` |
| `s1012` | ct thorax-abdomen-pelvis | 46/m | 285×285×453 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1012` |
| `s1016` | ct polytrauma | 79/m | 333×333×471 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1016` |
| `s1020` | ct  operation | 60/m | 209×209×90 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1020` |
| `s1022` | ct aortic valve | 87/m | 236×236×440 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1022` |
| `s1024` | ct thorax-abdomen-pelvis | 31/m | 333×333×415 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1024` |
| `s1029` | ct polytrauma | 71/m | 321×321×545 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1029` |
| `s1031` | ct neck-thorax-abdomen-pelvis | 69/m | 287×287×419 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1031` |
| `s1037` | ct abdomen | 71/f | 250×216×285 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1037` |
| `s1038` | ct neck-thorax-abdomen-pelvis | 58/m | 244×157×440 | 0.0 | 0.6 / 2.22 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1038` |
| `s1041` | ct pelvis | 92/f | 237×237×232 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1041` |
| `s1044` | ct thorax-abdomen-pelvis | 71/m | 257×257×459 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1044` |
| `s1045` | ct polytrauma | 62/m | 299×205×645 | 25.5 | 0.0 / 2.66 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1045` |
| `s1046` | ct thorax-abdomen-pelvis | 72/m | 268×175×437 | 7.5 | 0.61 / 0.62 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1046` |
| `s1050` | ct pelvis | 83/f | 255×122×218 | 1.5 | 3.53 / 4.39 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1050` |
| `s1053` | ct upper leg left | 59/f | 163×163×278 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1053` |
| `s1057` | ct pelvis | 71/f | 263×263×172 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1057` |
| `s1061` | ct thorax-abdomen-pelvis | 44/f | 285×285×424 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1061` |
| `s1062` | ct angiography neck-thx-abd-pelvis-leg | 73/m | 236×236×477 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1062` |
| `s1068` | ct pelvis | 22/f | 223×223×80 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1068` |
| `s1069` | ct thorax-abdomen-pelvis | 71/m | 231×231×444 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1069` |
| `s1070` | ct thorax-abdomen-pelvis | 37/f | 247×247×428 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1070` |
| `s1085` | ct neck-thorax-abdomen-pelvis | 76/m | 236×236×443 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1085` |
| `s1086` | ct neck-thorax-abdomen-pelvis | 45/m | 271×271×443 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1086` |
| `s1088` | ct polytrauma | 90/m | 333×333×544 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1088` |
| `s1089` | ct polytrauma | 65/m | 265×265×550 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1089` |
| `s1090` | ct thorax-abdomen-pelvis | 80/m | 257×257×439 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1090` |
| `s1098` | ct pelvis | 56/f | 331×331×422 | 36.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1098` |
| `s1099` | ct neck-thorax-abdomen-pelvis | 52/f | 285×285×427 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1099` |
| `s1103` | ct angiography pelvis-leg | 71/m | 283×162×851 | 470.2 | 3.41 / 1.98 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1103` |
| `s1104` | ct angiography pelvis-leg | 57/m | 298×138×726 | 562.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1104` |
| `s1105` | ct aortic valve | 78/f | 221×221×407 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1105` |
| `s1111` | ct thorax-abdomen-pelvis | 70/m | 231×231×419 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1111` |
| `s1120` | ct thorax-abdomen-pelvis | 55/f | 247×247×407 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1120` |
| `s1121` | ct angiography pelvis-leg | 30/f | 281×281×519 | 85.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1121` |
| `s1124` | ct polytrauma | 72/m | 267×267×536 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1124` |
| `s1125` | ct angiography pelvis-leg | 91/m | 247×247×245 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1125` |
| `s1127` | ct neck-thorax-abdomen-pelvis | 69/f | 275×275×409 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1127` |
| `s1128` | ct pelvis | 82/f | 225×225×159 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1128` |
| `s1130` | ct abdomen | 82/m | 280×280×283 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1130` |
| `s1131` | ct abdomen | 42/f | 365×180×299 | 0.0 | 0.0 / 3.03 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1131` |
| `s1135` | ct thorax-abdomen-pelvis | 52/m | 299×299×443 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1135` |
| `s1136` | ct pelvis | 73/f | 216×216×164 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1136` |
| `s1137` | ct pelvis | 52/m | 245×145×278 | 0.0 | 0.69 / 1.81 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1137` |
| `s1141` | ct pelvis | 87/f | 253×143×170 | 1.5 | 2.82 / 4.97 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1141` |
| `s1143` | ct neck-thorax-abdomen-pelvis | 70/m | 266×266×479 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1143` |
| `s1145` | ct neck-thorax-abdomen-pelvis | 57/f | 220×220×415 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1145` |
| `s1149` | ct abdomen | 80/m | 215×183×329 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1149` |
| `s1151` | ct angiography pelvis-leg | 49/m | 241×241×379 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1151` |
| `s1152` | ct polytrauma | 76/f | 333×333×548 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1152` |
| `s1156` | ct upper leg left | 54/m | 163×163×235 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1156` |
| `s1159` | ct polytrauma | 47/f | 329×329×537 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1159` |
| `s1161` | ct polytrauma | 52/m | 313×313×544 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1161` |
| `s1170` | ct upper leg left | 90/f | 131×89×311 | 28.4 | 4.58 / 3.45 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1170` |
| `s1174` | ct thorax-abdomen-pelvis | 61/m | 279×178×481 | 12.0 | 0.89 / 5.21 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1174` |
| `s1178` | ct neck-thorax-abdomen-pelvis | 57/f | 333×333×433 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1178` |
| `s1183` | ct abdomen | 62/m | 254×216×335 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1183` |
| `s1187` | ct abdomen | 62/m | 261×261×340 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1187` |
| `s1206` | ct polytrauma | 62/m | 279×279×538 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1206` |
| `s1207` | ct thorax-abdomen-pelvis | 83/f | 301×301×405 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1207` |
| `s1208` | ct angiography neck-thx-abd-pelvis-leg | 67/m | 221×221×448 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1208` |
| `s1209` | ct thorax-abdomen-pelvis | 74/m | 303×303×446 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1209` |
| `s1210` | ct thorax-abdomen-pelvis | 80/m | 246×246×422 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1210` |
| `s1212` | ct abdomen | 70/f | 249×249×260 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1212` |
| `s1216` | ct thorax-chest | 55/f | 315×315×317 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1216` |
| `s1223` | ct thorax-abdomen-pelvis | 54/f | 261×150×427 | 1.5 | 0.12 / 0.85 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1223` |
| `s1224` | ct thorax-abdomen-pelvis | 72/m | 272×272×452 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1224` |
| `s1228` | ct thorax-abdomen-pelvis | 62/f | 321×321×402 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1228` |
| `s1230` | ct aortic valve | 82/m | 236×236×486 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1230` |
| `s1233` | ct thorax-abdomen-pelvis | 88/f | 265×265×401 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1233` |
| `s1234` | ct angiography pelvis-leg | 57/f | 218×122×257 | 6.0 | 0.0 / 7.45 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1234` |
| `s1238` | ct thorax-abdomen-pelvis | 64/m | 295×295×457 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1238` |
| `s1244` | ct abdomen | 68/m | 245×245×319 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1244` |
| `s1247` | ct neck-thorax-abdomen-pelvis | 27/f | 259×259×431 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1247` |
| `s1248` | ct aortic valve | 83/f | 221×221×402 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1248` |
| `s1249` | ct polytrauma | 94/f | 333×333×603 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1249` |
| `s1251` | ct angiography pelvis-leg | 75/m | 280×280×472 | 40.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1251` |
| `s1252` | ct angiography pelvis-leg | 52/f | 276×120×506 | 42.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1252` |
| `s1256` | ct pelvis | 59/m | 292×292×228 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1256` |
| `s1257` | ct thorax-chest | 65/f | 220×220×155 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1257` |
| `s1267` | ct thorax-abdomen-pelvis | 59/m | 245×245×457 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1267` |
| `s1273` | ct thorax-abdomen-pelvis | 56/m | 263×263×465 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1273` |
| `s1276` | ct neck-thorax-abdomen-pelvis | 50/f | 240×221×411 | 0.0 | 0.33 / 2.47 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1276` |
| `s1279` | ct upper leg left | 64/f | 172×172×57 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1279` |
| `s1283` | ct neck-thorax-abdomen-pelvis | 75/f | 283×283×397 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1283` |
| `s1287` | ct neck-thorax-abdomen-pelvis | 77/m | 239×239×440 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1287` |
| `s1291` | ct abdomen | 71/f | 245×245×314 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1291` |
| `s1293` | ct thorax-abdomen | 78/f | 273×273×288 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1293` |
| `s1294` | ct thorax-abdomen | 21/f | 238×238×278 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1294` |
| `s1297` | ct neck-thorax-abdomen-pelvis | 84/m | 247×247×439 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1297` |
| `s1301` | ct pelvis | 77/m | 275×275×263 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1301` |
| `s1303` | ct upper leg left | 79/m | 113×113×388 | 61.5 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1303` |
| `s1307` | ct thorax-abdomen-pelvis | 52/m | 276×276×363 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1307` |
| `s1309` | ct abdomen | 65/m | 275×275×311 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1309` |
| `s1310` | ct aortic valve | 86/f | 208×165×461 | 22.5 | 0.48 / 2.07 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1310` |
| `s1314` | ct polytrauma | 62/f | 333×333×546 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1314` |
| `s1319` | ct spine | 81/f | 247×247×427 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1319` |
| `s1321` | ct polytrauma | 50/m | 275×275×540 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1321` |
| `s1322` | ct abdomen | 57/m | 240×240×297 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1322` |
| `s1326` | ct heart-thorakale aorta | 78/m | 221×221×440 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1326` |
| `s1331` | ct abdomen | 74/m | 240×240×238 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1331` |
| `s1334` | ct angiography neck-thx-abd-pelvis-leg | 79/m | 221×221×510 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1334` |
| `s1336` | ct neck-thorax-abdomen-pelvis | 79/m | 285×285×416 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1336` |
| `s1339` | ct abdomen | 55/f | 193×351×287 | 0.0 | 0.66 / 6.45 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1339` |
| `s1340` | ct thorax-abdomen-pelvis | 82/m | 275×275×431 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1340` |
| `s1344` | ct angiography neck-thx-abd-pelvis-leg | 67/f | 212×183×438 | 0.0 | 0.91 / 1.15 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1344` |
| `s1347` | ct abdomen | 78/m | 263×264×286 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1347` |
| `s1348` | ct thorax-abdomen-pelvis | 69/m | 239×239×437 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1348` |
| `s1349` | ct angiography neck-thx-abd-pelvis-leg | 66/m | 255×255×517 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1349` |
| `s1350` | ct polytrauma | 66/f | 333×333×539 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1350` |
| `s1355` | ct angiography abdomen-pelvis | 62/f | 299×299×201 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1355` |
| `s1357` | ct hip right | 82/f | 285×285×250 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1357` |
| `s1358` | ct angiography abdomen-pelvis | 69/f | 217×217×202 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1358` |
| `s1359` | ct hip right | 48/m | 140×140×196 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1359` |
| `s1361` | ct angiography thorax-abdomen-pelvis | 81/m | 283×283×475 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1361` |
| `s1362` | ct angiography thorax-abdomen-pelvis | 88/m | 275×275×429 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1362` |
| `s1363` | ct angiography thorax-abdomen-pelvis | 63/m | 296×296×389 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1363` |
| `s1364` | ct angiography thorax-abdomen-pelvis | 74/m | 247×247×413 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1364` |
| `s1365` | ct angiography thorax-abdomen-pelvis | 71/f | 277×277×438 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1365` |
| `s1366` | ct polytrauma | 50/? | 333×333×336 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1366` |
| `s1368` | ct angiography abdomen-pelvis | 41/m | 247×247×280 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1368` |
| `s1369` | ct whole body | 73/f | 290×290×549 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1369` |
| `s1371` | ct whole body | 70/m | 333×333×525 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1371` |
| `s1372` | ct angiography thorax-abdomen-pelvis | 82/f | 236×236×428 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1372` |
| `s1373` | ct angiography abdomen-pelvis | 80/m | 237×237×300 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1373` |
| `s1374` | ct angiography abdomen-pelvis | 75/m | 256×256×267 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1374` |
| `s1377` | ct angiography thorax-abdomen-pelvis | 71/m | 287×287×397 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1377` |
| `s1378` | ct hip right | 59/f | 267×267×115 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1378` |
| `s1379` | ct angiography thorax-abdomen-pelvis | 65/m | 279×279×490 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1379` |
| `s1380` | ct whole body | 78/f | 320×320×543 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1380` |
| `s1382` | ct angiography thorax-abdomen-pelvis | 77/m | 279×279×465 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1382` |
| `s1383` | ct angiography abdomen-pelvis | 75/m | 274×274×320 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1383` |
| `s1385` | ct angiography abdomen-pelvis | 69/f | 250×250×169 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1385` |
| `s1386` | ct angiography thorax-abdomen-pelvis | 87/m | 247×247×331 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1386` |
| `s1387` | ct angiography thorax-abdomen-pelvis | 70/m | 281×281×453 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1387` |
| `s1388` | ct whole body | 68/f | 316×316×525 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1388` |
| `s1390` | ct angiography abdomen-pelvis | 63/m | 285×285×335 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1390` |
| `s1391` | ct angiography abdomen-pelvis | 65/m | 270×270×335 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1391` |
| `s1394` | ct angiography abdomen-pelvis | 57/m | 261×261×306 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1394` |
| `s1395` | ct angiography thorax-abdomen-pelvis | 70/m | 281×281×409 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1395` |
| `s1397` | ct whole body | 74/m | 320×320×547 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1397` |
| `s1399` | ct hip right | 56/f | 245×245×239 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1399` |
| `s1400` | ct angiography thorax-abdomen-pelvis | 66/m | 271×271×459 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1400` |
| `s1401` | ct angiography abdomen-pelvis | 46/f | 244×244×179 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1401` |
| `s1403` | ct angiography abdomen-pelvis | 60/m | 258×258×317 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1403` |
| `s1404` | ct angiography thorax-abdomen-pelvis | 63/f | 236×236×421 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1404` |
| `s1405` | ct angiography abdomen-pelvis | 76/m | 255×255×287 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1405` |
| `s1411` | ct thorax-abdomen-pelvis | 63/m | 260×260×295 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1411` |
| `s1412` | ct polytrauma | 43/m | 255×255×311 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1412` |
| `s1413` | ct neck-thorax-abdomen-pelvis | 32/m | 333×333×347 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1413` |
| `s1414` | ct abdomen-pelvis | 43/f | 235×235×313 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1414` |
| `s1415` | ct abdomen-pelvis | 68/f | 291×291×337 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1415` |
| `s1418` | ct abdomen-pelvis | 47/f | 234×234×307 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1418` |
| `s1421` | ct abdomen-pelvis | 69/m | 303×303×370 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1421` |
| `s1422` | ct thorax-abdomen-pelvis | 64/f | 285×285×345 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1422` |
| `s1423` | ct abdomen-pelvis | 27/f | 277×277×299 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1423` |
| `s1424` | ct thorax-abdomen-pelvis | 98/f | 316×316×281 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1424` |
| `s1429` | — | 24/f | 296×296×168 | 0.0 | 0.0 / 0.0 | `H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201\s1429` |

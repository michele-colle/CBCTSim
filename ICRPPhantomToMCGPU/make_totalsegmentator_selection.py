#!/usr/bin/env python3
"""Select TotalSegmentator cases by imaged body region.

Writes `totalsegmentator_selection.json` (machine-readable, for job
generation) and `TOTALSEGMENTATOR_SELECTION.md` (the same content as tables).

Why the groups are defined the way they are: TotalSegmentator's 117 classes
contain no hand, forearm, foot, ankle, knee, tibia or patella label, so only
head / upper arm / thigh can be read off a mask directly.  Coverage further
down a limb has to be inferred from how far the volume extends past the
distal femur -- the last bone that IS segmented.

The per-case scan decompresses a few masks per case and takes ~10 min over a
Windows mount, so it is cached; pass --rescan to force it.

Usage
-----
  python3 make_totalsegmentator_selection.py [--root DIR] [--rescan]
"""

import argparse
import csv
import json
import os
from pathlib import Path

import nibabel as nib
import numpy as np

DEFAULT_ROOT = '/mnt/h/MICHELE_MCGPU/Totalsegmentator_dataset_v201'
ROOT_WIN = r'H:\MICHELE_MCGPU\Totalsegmentator_dataset_v201'
REPO = Path(__file__).resolve().parent

# Masks worth decompressing: the only ones that bound a body region.
PROBE = ['skull', 'brain', 'humerus_left', 'humerus_right',
         'femur_left', 'femur_right']
# A mask smaller than this is segmentation noise, not anatomy.  Two
# `ct angiography head` cases carry a 2- and an 8-voxel "femur"; without this
# floor they would count as leg scans.
MIN_VOX = 1000
FULL_CRANIUM_MM = 100          # skull z-extent that means more than a clipped base
KNEE_MM = 10                   # volume continues past the distal femur -> knee seen
BELOW_KNEE_MM = 60
FOOT_MM = 300                  # distal femur to ankle is ~350-400 mm in an adult


def scan_dataset(root: Path) -> list:
    """Per-case z-extent of each probe mask, in patient world coordinates."""
    out = []
    cases = sorted(p for p in root.glob('s[0-9]*') if p.is_dir())
    for n, d in enumerate(cases, 1):
        if n % 100 == 0:
            print(f'    {n}/{len(cases)}...', flush=True)
        rec = {'case': d.name}
        try:
            ct = nib.load(str(d / 'ct.nii.gz'))
        except Exception as exc:
            rec['error'] = str(exc)
            out.append(rec)
            continue
        aff, shape = ct.affine, ct.shape
        ci, cj = shape[0] / 2, shape[1] / 2
        zc = lambda k: float(aff[2] @ [ci, cj, k, 1])
        z0, z1 = zc(0), zc(shape[2] - 1)
        rec['z_inf'], rec['z_sup'] = min(z0, z1), max(z0, z1)

        sp = np.linalg.norm(aff[:3, :3], axis=0)
        D = aff[:3, :3] / sp
        ang = lambda v: round(float(np.degrees(
            np.arccos(np.clip(np.abs(v).max(), -1, 1)))), 2)
        rec['shape'] = list(shape)
        rec['spacing_mm'] = [round(float(s), 4) for s in sp]
        # Out-of-plane tilt shears the phantom under the round-7 bug; in-plane
        # rotation only spins the patient inside the voxel box.
        rec['tilt_out_of_plane_deg'] = ang(D[:, 2])
        rec['tilt_in_plane_deg'] = ang(D[:, 0])

        ext = {}
        for name in PROBE:
            p = d / 'segmentations' / f'{name}.nii.gz'
            if not p.exists():
                continue
            a = np.asanyarray(nib.load(str(p)).dataobj)
            if not a.any():
                continue
            ks = np.nonzero(a.any(axis=(0, 1)))[0]
            lo, hi = zc(int(ks.min())), zc(int(ks.max()))
            ext[name] = {'n': int(a.sum()), 'z_lo': min(lo, hi), 'z_hi': max(lo, hi)}
        rec['ext'] = ext
        out.append(rec)
    return out


def real(rec, *names):
    es = [rec['ext'][n] for n in names
          if rec['ext'].get(n) and rec['ext'][n]['n'] >= MIN_VOX]
    return es or None


def build(scan, meta, root):
    groups = {
        'full_head': ('Full / near-full cranium',
                      f'skull mask spans >= {FULL_CRANIUM_MM} mm in z'),
        'dedicated_head': ('Dedicated head studies',
                           "meta.csv study_type contains 'head' or 'orbita'"),
        'knee': ('Knee inside the volume',
                 f'femur present and volume extends > {KNEE_MM} mm below its distal end'),
        'below_knee': ('Lower leg (below the knee)',
                       f'volume extends > {BELOW_KNEE_MM} mm below the distal femur'),
        'foot_ankle': ('Foot / ankle',
                       f'volume extends > {FOOT_MM} mm below the distal femur '
                       '(all 5 visually confirmed)'),
        'thigh': ('Thigh (femur present)', f'femur mask >= {MIN_VOX} voxels'),
    }
    groups = {k: {'title': t, 'criterion': c, 'cases': []}
              for k, (t, c) in groups.items()}

    for rec in scan:
        if 'error' in rec:
            continue
        case = rec['case']
        m = meta.get(case, {})
        st = m.get('study_type', '').strip()
        sk = real(rec, 'skull')
        fem = real(rec, 'femur_left', 'femur_right')
        below = (min(e['z_lo'] for e in fem) - rec['z_inf']) if fem else None
        skull_mm = (max(e['z_hi'] for e in sk) - min(e['z_lo'] for e in sk)) if sk else 0.0

        hits = []
        if sk and skull_mm >= FULL_CRANIUM_MM:
            hits.append('full_head')
        if 'head' in st.lower() or 'orbita' in st.lower():
            hits.append('dedicated_head')
        if fem:
            hits.append('thigh')
            if below > KNEE_MM:
                hits.append('knee')
            if below > BELOW_KNEE_MM:
                hits.append('below_knee')
            if below > FOOT_MM:
                hits.append('foot_ankle')
        if not hits:
            continue

        entry = {'case': case,
                 'dir_linux': f'{root}/{case}',
                 'dir_windows': f'{ROOT_WIN}\\{case}',
                 'ct': f'{root}/{case}/ct.nii.gz',
                 'segmentations': f'{root}/{case}/segmentations',
                 'study_type': st, 'age': m.get('age', ''), 'sex': m.get('gender', ''),
                 'manufacturer': m.get('manufacturer', ''), 'kvp': m.get('kvp', ''),
                 'shape': rec['shape'], 'spacing_mm': rec['spacing_mm'],
                 'tilt_out_of_plane_deg': rec['tilt_out_of_plane_deg'],
                 'tilt_in_plane_deg': rec['tilt_in_plane_deg']}
        if skull_mm:
            entry['skull_z_extent_mm'] = round(float(skull_mm), 1)
        if below is not None:
            entry['mm_below_distal_femur'] = round(float(below), 1)
        for h in hits:
            groups[h]['cases'].append(entry)

    for g in groups.values():
        g['count'] = len(g['cases'])
    return groups


def write_markdown(path, groups, root, n_total):
    L = []
    A = L.append
    A('# TotalSegmentator v2.0.1 — case selection by body region\n')
    A(f'Source: `{ROOT_WIN}` (`{root}` from WSL) — {n_total} cases, '
      '**all 1.5 mm isotropic**.')
    A('Machine-readable companion: `totalsegmentator_selection.json`. '
      'Regenerate both with `make_totalsegmentator_selection.py`.\n')
    A('Each case folder holds `ct.nii.gz` plus a `segmentations/` folder of 117 '
      'masks. Convert a case to a DICOM series `dicom_to_mcgpu` can read with:\n')
    A('```bash')
    A('python3 nrrd_to_dicom_series.py <case>/ct.nii.gz <out_dir>/<case> --name <case>')
    A('```\n')
    A('## How the groups were derived\n')
    A('TotalSegmentator has no hand, forearm, foot, ankle, knee, tibia or patella '
      'class, so only head, upper arm and thigh are directly readable from a mask. '
      'Coverage further down a limb is inferred from how far the volume extends '
      f'past the distal femur. Masks under {MIN_VOX} voxels are ignored as '
      'segmentation noise (two `ct angiography head` cases carry a 2- and an '
      '8-voxel "femur" that would otherwise count as leg scans).\n')
    A('**Not present anywhere in the dataset:** hands and forearms. The single '
      '`ct upper limb both` case (s0035) is a shoulder/upper-thorax scan cut off '
      'at mid-humerus.\n')
    A('| Group | Criterion | Cases | Usable as-is | Sheared by the round-7 bug |')
    A('|---|---|---|---|---|')
    for k, g in groups.items():
        ok = sum(1 for c in g['cases'] if c['tilt_out_of_plane_deg'] < 0.01)
        A(f'| `{k}` | {g["criterion"]} | {g["count"]} | {ok} | {g["count"] - ok} |')
    A('')
    A('Groups **overlap**: `knee` ⊃ `below_knee` ⊃ `foot_ankle` are nested subsets '
      'of `thigh`, and most `dedicated_head` cases are also `full_head`.\n')
    A('## ⚠ Obliquity — read before building phantoms\n')
    A('`dicom_to_mcgpu` places voxels at `(index+0.5)*spacing` and never reads '
      '`ImageOrientationPatient` (PROGRESS round 7, still open):\n')
    A('- **`tilt_out_of_plane_deg` > 0** — the slice normal is off the patient '
      'axis, so the phantom comes out **SHEARED**. Fix the direction-cosine bug '
      'before building these.')
    A('- **`tilt_in_plane_deg` only** — the patient is rigidly rotated about z '
      'inside the voxel box. Geometrically valid, just an unusual pose.\n')
    for k, g in groups.items():
        cs = g['cases']
        A(f'## {g["title"]} — `{k}` ({len(cs)} cases)\n')
        A(f'*{g["criterion"]}*\n')
        if not cs:
            A('_none_\n')
            continue
        head_grp = k in ('full_head', 'dedicated_head')
        extra = 'skull mm' if head_grp else 'mm below femur'
        A(f'| Case | Study type | Age/Sex | Shape | {extra} | tilt out / in (deg) | Path |')
        A('|---|---|---|---|---|---|---|')
        for c in cs:
            val = c.get('skull_z_extent_mm', '') if head_grp else \
                  c.get('mm_below_distal_femur', '')
            age = c['age'].split('.')[0] if c['age'] else '?'
            sh = '×'.join(str(x) for x in c['shape'])
            A(f'| `{c["case"]}` | {c["study_type"] or "—"} | {age}/{c["sex"] or "?"} '
              f'| {sh} | {val} | {c["tilt_out_of_plane_deg"]} / '
              f'{c["tilt_in_plane_deg"]} | `{c["dir_windows"]}` |')
        A('')
    Path(path).write_text('\n'.join(L))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--root', default=DEFAULT_ROOT, help='dataset root')
    ap.add_argument('--rescan', action='store_true', help='ignore the cached scan')
    args = ap.parse_args()

    root = Path(args.root)
    cache = REPO / '.totalsegmentator_scan_cache.json'
    if cache.exists() and not args.rescan:
        scan = json.loads(cache.read_text())
        print(f'using cached scan ({len(scan)} cases) -- --rescan to redo')
    else:
        print(f'scanning {root} (a few minutes)...')
        scan = scan_dataset(root)
        cache.write_text(json.dumps(scan))
        print(f'  cached -> {cache}')

    meta = {r['image_id']: r for r in csv.DictReader(
        open(root / 'meta.csv', encoding='utf-8-sig'), delimiter=';')}
    groups = build(scan, meta, str(root))

    jpath = REPO / 'totalsegmentator_selection.json'
    jpath.write_text(json.dumps(
        {'dataset': 'TotalSegmentator v2.0.1',
         'root_linux': str(root), 'root_windows': ROOT_WIN,
         'total_cases_in_dataset': len(scan),
         'note': ('Groups OVERLAP: knee/below_knee/foot_ankle are nested subsets '
                  'of thigh, and most dedicated_head cases are also full_head. '
                  'All cases are 1.5 mm isotropic. No hands or forearms exist '
                  'anywhere in the dataset.'),
         'groups': groups}, indent=2))
    write_markdown(REPO / 'TOTALSEGMENTATOR_SELECTION.md', groups, str(root), len(scan))

    print(f'wrote {jpath}')
    print(f'wrote {REPO / "TOTALSEGMENTATOR_SELECTION.md"}')
    for k, g in groups.items():
        ok = sum(1 for c in g['cases'] if c['tilt_out_of_plane_deg'] < 0.01)
        print(f'  {k:16} {g["count"]:4d}  ({ok} tilt-free)')


if __name__ == '__main__':
    main()

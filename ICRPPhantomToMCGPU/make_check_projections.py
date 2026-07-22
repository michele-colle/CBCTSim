#!/usr/bin/env python3
"""
make_check_projections.py -- build single-projection .in files for a quick
visual check of the simulation geometry, WITHOUT touching the originals.

For each patient it picks one 120 kV head_bar .in from mcgpu_in_batch (stretcher
A by default), copies it into mcgpu_in_check/, and makes two edits:

  * NUMBER OF PROJECTIONS  -> 0   (single projection)
  * VOXEL GEOMETRY FILE    -> absolute on-disk path of the .raw
                              (spectrum/material/results stay relative, since
                               BeerLambert.x runs from example_simulations/)

It also writes run_check_projections.sh, which runs each one through
BeerLambert.x from the example_simulations folder (like run_beerLambert.sh).

Usage:  python3 make_check_projections.py
"""

import re
from pathlib import Path

REPO = Path(__file__).resolve().parent
IN_BATCH = REPO / "mcgpu_in_batch"
OUT_DIR = REPO / "mcgpu_in_check"
EXPORT = Path("/home/colle/GradientHealthExport/export")
SIM_DIR = Path("/home/colle/MCGPU_sim/example_simulations")
BEERLAMBERT = "./BeerLambert.x"

KIND = "head_bar"          # most informative for a positioning check
KV = "120kV"               # positioning is identical across kV
PREFERRED_LABEL = "A"      # which stretcher position to check per patient
N_PROJECTIONS = "0"        # single projection (.in comment: "1 disables tomographic mode")

GEOM_RE = re.compile(r"(\s*)phantom/(\S+\.raw)(.*)")


def patient_of(p: Path) -> str:
    # <N><letter>_<patient>_<label>_<kind>_<kv>.in
    return p.name.split("_")[1]


def label_of(p: Path) -> str:
    return p.name.split("_")[2]


def main():
    candidates = sorted(IN_BATCH.glob(f"*_{KIND}_{KV}.in"))
    if not candidates:
        raise SystemExit(f"No *_{KIND}_{KV}.in in {IN_BATCH}")

    # one per patient: prefer PREFERRED_LABEL, else first sorted
    by_patient = {}
    for c in candidates:
        by_patient.setdefault(patient_of(c), []).append(c)
    chosen = []
    for patient, files in sorted(by_patient.items()):
        pick = next((f for f in files if label_of(f) == PREFERRED_LABEL), files[0])
        chosen.append(pick)

    # index of .raw basenames on disk (one rglob, not one per file)
    raw_index = {p.name: p for p in EXPORT.rglob("*.raw")}

    OUT_DIR.mkdir(exist_ok=True)
    run_lines = ["#!/bin/bash",
                 "# Auto-generated: single-projection geometry checks via BeerLambert.x",
                 "set -uo pipefail",
                 f'cd "{SIM_DIR}"', ""]

    written, missing_raw = [], []
    for src in chosen:
        lines = src.read_text().splitlines(keepends=True)
        out, fixed_geom, fixed_proj = [], False, False
        for ln in lines:
            if not fixed_proj and "NUMBER OF PROJECTIONS" in ln:
                m = re.match(r"(\s*)(\S+)(.*)", ln)
                out.append(f"{m.group(1)}{N_PROJECTIONS}{m.group(3)}\n")
                fixed_proj = True
                continue
            mg = GEOM_RE.match(ln)
            if not fixed_geom and mg and "VOXEL GEOMETRY FILE" in ln:
                basename = mg.group(2)
                disk = raw_index.get(basename)
                if disk is None:
                    missing_raw.append((src.name, basename))
                    out.append(ln)
                else:
                    out.append(f"{mg.group(1)}{disk}{mg.group(3)}\n")
                fixed_geom = True
                continue
            out.append(ln)

        dst = OUT_DIR / src.name
        dst.write_text("".join(out))
        written.append(dst)
        run_lines += [f'echo "=== {dst.name} ==="',
                      f'{BEERLAMBERT} "{dst}" 2>&1 | tee "{dst.stem}.out"', ""]

    run_sh = REPO / "run_check_projections.sh"
    run_sh.write_text("\n".join(run_lines) + "\n")
    run_sh.chmod(0o755)

    print(f"Wrote {len(written)} check .in files -> {OUT_DIR}")
    for d in written:
        print(f"  {d.name}")
    if missing_raw:
        print("\n!! .raw not found on disk for:")
        for n, b in missing_raw:
            print(f"   {n}: {b}")
    have = {patient_of(p) for p in chosen}
    print(f"\nPatients covered: {len(have)}.  Runner -> {run_sh}")
    print(f"Run all checks:  bash {run_sh}")


if __name__ == "__main__":
    main()

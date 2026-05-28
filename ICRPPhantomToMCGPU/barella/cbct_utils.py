"""
cbct_utils.py — shared I/O helpers for CBCT analysis scripts.
"""

import os
import re
import json

import numpy as np


# ── I0-from-DAP lookup table (mirrors Cefla_de_data.cs I0CalculatorDictionary)
# key: (fov_tag, kv_int) → (A, B, default_I0)  where  I0 = A * dap_derivative + B
_FOV_I0_PARAMS = {
    ("fov17x17_308um", 120): (3131.556,  -374.043, 42000),
    ("fov17x17_308um",  80): (1395.841,  -315.374, 40000),
    ("fov15x06_154um", 120): (8020.34,   -222.125, 31000),
    ("fov15x06_154um",  80): (3768.585,  -576.258, 29000),
    ("fov10x10_154um", 120): (8020.34,   -222.125, 31000),
    ("fov10x10_154um",  80): (3768.585,  -576.258, 29000),
}


def _fov_tag(fov_str, pitch):
    """Map the JSON 'fov' string + det_column_pitch to the FOV tag used in _FOV_I0_PARAMS."""
    s = fov_str.replace("[", "").replace("]", "").replace(" ", "")
    parts = re.split(r"[xX]", s)
    if len(parts) != 2:
        return None
    try:
        c1, c2 = int(parts[0]), int(parts[1])
    except ValueError:
        return None
    if c1 == 17 and c2 == 17 and abs(pitch - 0.308) < 0.002:
        return "fov17x17_308um"
    if c1 == 15 and c2 == 6  and abs(pitch - 0.154) < 0.002:
        return "fov15x06_154um"
    if c1 == 10 and c2 == 10 and abs(pitch - 0.154) < 0.002:
        return "fov10x10_154um"
    return None



def calc_i0_mean_from_dap(scan_params, fov_str):
    """
    Replicate Cefla_de_data I0_calculator.Evaluate (DAP branch only).

    Procedure (mirrors the C# code):
      1. Bin dap_value by 50 → (pos, binned).
      2. Fit Akima spline to (pos, binned).
      3. Differentiate at every integer projection index → per-image DAP increment.
      4. Apply the FOV+kV linear equation: I0_i = A * dap_i + B.
      5. Return the mean over all projections.

    Returns None if dap_value is absent or the FOV/kV combo is unknown.
    """
    from scipy.interpolate import Akima1DInterpolator

    dap_value = scan_params.get("dap_value")
    if dap_value is None:
        return None

    n_proj = len(scan_params["angle_eff"])
    kv     = int(round(float(scan_params["kv"])))
    pitch  = scan_params["det_column_pitch"]

    tag    = _fov_tag(fov_str, pitch)
    entry  = _FOV_I0_PARAMS.get((tag, kv))
    if entry is None:
        return None
    A, B, _ = entry

    # Linear fit to the cumulative DAP: slope = mean per-image increment.
    # More robust than dap[-1]/n because it uses all points and ignores end noise.
    dap_arr = np.asarray(dap_value, dtype=np.float64)
    slope, _ = np.polyfit(np.arange(len(dap_arr)), dap_arr, 1)
    return A * slope + B


def load_study(params_path):
    """Return the full JSON dict from a *_Params.json file."""
    with open(params_path) as f:
        return json.load(f)


def load_params(params_path, scan_index=0):
    return load_study(params_path)["scans"][scan_index]


def find_params_json(folder):
    """Return the path of the single *_Params.json file inside folder."""
    for fname in os.listdir(folder):
        if fname.endswith("_Params.json"):
            return os.path.join(folder, fname)
    raise FileNotFoundError(f"No *_Params.json found in {folder}")


def load_acq_time(params_path):
    """
    Return a human-readable acquisition timestamp from a *_Params.json file.
    Fields acquisition_date (YYYYMMDD) and acquisition_time (HHMMSS) are read
    from the top-level dict.  Returns a string like '2026-05-25  09:17:12'.
    """
    with open(params_path) as f:
        top = json.load(f)
    date_s = str(top.get("acquisition_date", ""))
    time_s = str(top.get("acquisition_time", "")).zfill(6)
    date_fmt = f"{date_s[:4]}-{date_s[4:6]}-{date_s[6:]}" if len(date_s) == 8 else date_s
    time_fmt = f"{time_s[:2]}:{time_s[2:4]}:{time_s[4:]}"
    return f"{date_fmt}  {time_fmt}"


def list_images(imgdir):
    """Sorted list of (1-based index, full path) excluding BlankImg files."""
    pat = re.compile(r"_Img(\d+)\.raw$", re.IGNORECASE)
    entries = []
    for fname in os.listdir(imgdir):
        m = pat.search(fname)
        if m:
            entries.append((int(m.group(1)), os.path.join(imgdir, fname)))
    entries.sort(key=lambda x: x[0])
    return entries

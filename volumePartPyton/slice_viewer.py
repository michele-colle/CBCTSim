import os
import re
import numpy as np
import matplotlib.pyplot as plt
from matplotlib import colors
from matplotlib.widgets import Slider

# --- CONFIG ---
input_folder = "/home/colle/Desktop/CBCTSim/CBCTSim/volumePartPyton/ICRPdata/ICRP110_g4dat/AF/"
colormap = "nipy_spectral"
brush_radius = 5  # <-- Set your desired brush radius here (in pixels)

def natural_key(s):
    return [int(text) if text.isdigit() else text.lower() for text in re.split(r'(\d+)', s)]

def load_g4dat_slice(filepath):
    with open(filepath, 'r') as f:
        lines = f.readlines()
    nx = int(lines[0].strip())
    ny = int(lines[1].strip())
    try:
        _ = int(lines[2].strip())
        data_lines = lines[3:]
    except ValueError:
        data_lines = lines[2:]
    values = []
    for line in data_lines:
        values.extend([int(v) for v in line.strip().split()])
    if len(values) != nx * ny:
        raise ValueError(f"Expected {nx*ny} values, got {len(values)} in {filepath}")
    array2d = np.array(values, dtype=np.int16).reshape((ny, nx))
    return array2d, nx, ny

slice_files = sorted(
    [f for f in os.listdir(input_folder) if f.endswith(".g4dat")],
    key=natural_key
)
if not slice_files:
    raise FileNotFoundError("No .g4dat files found.")

slices = []
all_ids = set()
for f in slice_files:
    arr, _, _ = load_g4dat_slice(os.path.join(input_folder, f))
    slices.append(arr)
    all_ids.update(np.unique(arr))

all_ids = sorted(all_ids)
id_to_index = {val: idx for idx, val in enumerate(all_ids)}
index_to_id = {idx: val for val, idx in id_to_index.items()}
num_classes = len(all_ids)

mapped_slices = [np.vectorize(id_to_index.get)(arr) for arr in slices]

cmap = plt.get_cmap(colormap, num_classes)
norm = colors.BoundaryNorm(boundaries=np.arange(-0.5, num_classes + 0.5), ncolors=num_classes)

fig, ax = plt.subplots()
plt.subplots_adjust(bottom=0.15)
img = ax.imshow(mapped_slices[0], cmap=cmap, norm=norm)
title = ax.set_title(f"Slice 0: {slice_files[0]}")
cbar = plt.colorbar(img, ax=ax, ticks=np.arange(num_classes))
cbar.ax.set_yticklabels([index_to_id[i] for i in range(num_classes)])
cbar.set_label("Material ID")
ax.axis("off")

ax_slider = plt.axes([0.2, 0.05, 0.6, 0.03])
slider = Slider(ax_slider, 'Slice', 0, len(slices)-1, valinit=0, valstep=1)

current_slice_idx = 0
painting = False

def update(val):
    global current_slice_idx
    current_slice_idx = int(slider.val)
    img.set_data(mapped_slices[current_slice_idx])
    title.set_text(f"Slice {current_slice_idx}: {slice_files[current_slice_idx]}")
    fig.canvas.draw_idle()

slider.on_changed(update)

def get_pixels_in_brush(center_y, center_x, radius, max_y, max_x):
    """Return list of pixel coords (y,x) inside a round brush centered at (center_y, center_x)."""
    y_min = max(center_y - radius, 0)
    y_max = min(center_y + radius, max_y - 1)
    x_min = max(center_x - radius, 0)
    x_max = min(center_x + radius, max_x - 1)

    pixels = []
    for y in range(y_min, y_max+1):
        for x in range(x_min, x_max+1):
            if (y - center_y)**2 + (x - center_x)**2 <= radius**2:
                pixels.append((y, x))
    return pixels

def get_pixel(event):
    if event.inaxes != ax:
        return None
    x = int(event.xdata + 0.5)
    y = int(event.ydata + 0.5)
    max_y, max_x = slices[0].shape
    if 0 <= x < max_x and 0 <= y < max_y:
        return y, x
    return None

def paint_at(event):
    pixel = get_pixel(event)
    if pixel is None:
        return
    y, x = pixel
    max_y, max_x = slices[0].shape
    brush_pixels = get_pixels_in_brush(y, x, brush_radius, max_y, max_x)

    for py, px in brush_pixels:
        slices[current_slice_idx][py, px] = 0
        if 0 in id_to_index:
            mapped_slices[current_slice_idx][py, px] = id_to_index[0]

    img.set_data(mapped_slices[current_slice_idx])
    fig.canvas.draw_idle()

def on_press(event):
    global painting
    if event.button == 1 and event.inaxes == ax:
        painting = True
        paint_at(event)

def on_move(event):
    global painting
    if painting:
        paint_at(event)

def on_release(event):
    global painting
    painting = False

fig.canvas.mpl_connect('button_press_event', on_press)
fig.canvas.mpl_connect('motion_notify_event', on_move)
fig.canvas.mpl_connect('button_release_event', on_release)

plt.show()

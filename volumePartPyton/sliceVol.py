import os
import numpy as np
import matplotlib.pyplot as plt
from matplotlib import colors

# --- CONFIG ---
input_folder = "slices_input"
output_folder = "slices_output"
nx, ny = 200, 400  # Slice dimensions (X and Y)
colormap = "nipy_spectral"  # Color map for visualization

# Create output folder if needed
os.makedirs(output_folder, exist_ok=True)

# --- Utility: Load one 2D slice from file ---
def load_slice(filepath):
    with open(filepath, 'r') as f:
        flat_data = [int(line.strip()) for line in f]
    return np.array(flat_data, dtype=np.int16).reshape((ny, nx))

# --- Utility: Save one 2D slice to file ---
def save_slice(array2d, filepath):
    flat = array2d.flatten()
    with open(filepath, 'w') as f:
        for val in flat:
            f.write(f"{val}\n")

# --- List and sort slice files ---
slice_files = sorted([f for f in os.listdir(input_folder) if f.endswith(".dat")])

# --- Collect unique IDs for color normalization ---
all_ids = set()
for filename in slice_files:
    arr = load_slice(os.path.join(input_folder, filename))
    all_ids.update(np.unique(arr))
all_ids = sorted(all_ids)
id_to_index = {val: idx for idx, val in enumerate(all_ids)}
num_classes = len(all_ids)

# --- Define color normalization for colormap ---
cmap = plt.get_cmap(colormap, num_classes)
norm = colors.BoundaryNorm(boundaries=np.arange(-0.5, num_classes + 0.5), ncolors=num_classes)

# --- Process and visualize each slice ---
for i, filename in enumerate(slice_files):
    full_path = os.path.join(input_folder, filename)
    slice2d = load_slice(full_path)

    # Remap to index space (0...N)
    mapped = np.vectorize(id_to_index.get)(slice2d)

    # Display
    plt.figure(figsize=(8, 6))
    plt.imshow(mapped, cmap=cmap, norm=norm)
    plt.title(f"Slice {i} - {filename}")
    plt.colorbar(label="Material ID")
    plt.tight_layout()
    plt.show()

    # --- Optional: Save processed slice to output folder ---
    output_path = os.path.join(output_folder, filename)
    save_slice(slice2d, output_path)  # You could also save `mapped` if desired

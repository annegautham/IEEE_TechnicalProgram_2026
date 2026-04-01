import os
from PIL import Image
import tkinter as tk
from tkinter import filedialog
import re

def stitch_images():
    # 1. Open Folder Picker
    root = tk.Tk()
    root.withdraw()  # Hide the main tkinter window
    folder_selected = filedialog.askdirectory(title="Select Folder with KLayout Tiles")
    
    if not folder_selected:
        print("No folder selected. Exiting.")
        return

    # 2. Get list of files and find the grid dimensions
    files = [f for f in os.listdir(folder_selected) if f.endswith(".png") and "tile_y" in f]
    
    if not files:
        print("No tile images found in this folder.")
        return

    # Parse filenames to find max x and max y
    # Expects format: tile_y02_x03.png
    coords = []
    for f in files:
        match = re.search(r'tile_y(\d+)_x(\d+)', f)
        if match:
            coords.append((int(match.group(2)), int(match.group(1)))) # (x, y)

    nx = max(c[0] for c in coords) + 1
    ny = max(c[1] for c in coords) + 1

    # 3. Get dimensions of a single tile
    sample_img = Image.open(os.path.join(folder_selected, files[0]))
    w, h = sample_img.size

    # 4. Create the canvas
    full_image = Image.new('RGB', (nx * w, ny * h))

    print(f"Stitching a {nx}x{ny} grid into a {nx*w}x{ny*h} image...")

    # 5. Paste tiles
    for ix, iy in coords:
        img_path = os.path.join(folder_selected, f"tile_y{iy:02d}_x{ix:02d}.png")
        if os.path.exists(img_path):
            img = Image.open(img_path)
            # In PIL (0,0) is TOP-left. KLayout exports (0,0) as BOTTOM-left.
            # We flip the Y coordinate for the canvas:
            canvas_y = (ny - 1 - iy) * h
            full_image.paste(img, (ix * w, canvas_y))

    # 6. Save result
    save_path = os.path.join(folder_selected, "stitched_layout_final.png")
    full_image.save(save_path)
    print(f"Success! Saved to: {save_path}")

if __name__ == "__main__":
    stitch_images()
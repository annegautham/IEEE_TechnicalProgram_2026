import pya
import math
import os

# =========================
# CONFIG
# =========================
OUTPUT_DIR = os.path.join(os.path.expanduser("~"), "Desktop", "klayout_tiles")
IMG_WIDTH = 1280
IMG_HEIGHT = 720
MARGIN = 0  # Margin in micrometers (um)

if not os.path.exists(OUTPUT_DIR):
    os.makedirs(OUTPUT_DIR)

# =========================
# GET VIEW / LAYOUT
# =========================
view = pya.LayoutView.current()
if view is None:
    raise Exception("No active layout view. Open a file first!")

cv = view.active_cellview()
top_cell = cv.cell
dbu = cv.layout().dbu

# =========================
# CALCULATION LOGIC
# =========================
# Get the full extent of the design in Microns
bbox = top_cell.dbbox() 

# Apply the margin to the bounding box boundaries
xmin = bbox.left - MARGIN
ymin = bbox.bottom - MARGIN
xmax = bbox.right + MARGIN
ymax = bbox.top + MARGIN

layout_w = xmax - xmin
layout_h = ymax - ymin

# Define zoom level
tiles_across = 9
tile_w_um = layout_w / tiles_across
# Calculate height based on 1280:720 aspect ratio
tile_h_um = tile_w_um * (IMG_HEIGHT / IMG_WIDTH) 

nx = tiles_across
ny = math.ceil(layout_h / tile_h_um)

print(f"Starting export: {nx}x{ny} grid with {MARGIN}um margin...")

# =========================
# EXPORT LOOP
# =========================
old_grid = view.get_config("grid-visible")
old_axes = view.get_config("axes-visible")
old_scale = view.get_config("scale-bar-visible")

view.set_config("grid-visible", "false")
view.set_config("axes-visible", "false")
view.set_config("scale-bar-visible", "false")

view.set_config("inst-points-visible", "false")
view.set_config("selection-visible", "false")

# Force the renderer to show all details regardless of zoom
view.set_config("min-feature-size", "0")
# Disable the "Stippling" (stripes) globally
view.set_config("fill-mode", "0")

# 2. Force the layer to be solid instead of striped
# This iterates through the layers and sets their fill to 'solid'
lp = view.begin_layers()
while not lp.at_end():
    layer_properties = lp.current()
    layer_properties.dither_pattern = 0 # 0 is solid fill
    lp.next()

try:
    for iy in range(ny):
        for ix in range(nx):
            # Calculate coordinates based on the padded boundaries
            x0 = xmin + ix * tile_w_um
            y0 = ymin + iy * tile_h_um
            x1 = x0 + tile_w_um
            y1 = y0 + tile_h_um

            # zoom_box sets the viewport to these specific micron coordinates
            view.zoom_box(pya.DBox(x0, y0, x1, y1))
            pya.Application.instance().process_events()
            
            clip_box = pya.DBox(x0, y0, x1, y1)
            filename = os.path.join(OUTPUT_DIR, f"tile_y{iy:02d}_x{ix:02d}.png")
            view.save_image_with_options(filename, IMG_WIDTH, IMG_HEIGHT, 0, 0, 0, clip_box, False)
            print(f"Saved: {filename}")

finally:
    view.set_config("grid-visible", old_grid)
    view.set_config("axes-visible", old_axes)
    view.set_config("scale-bar-visible", old_scale)
    view.zoom_fit()

print("Done! Tiles saved with requested padding.")
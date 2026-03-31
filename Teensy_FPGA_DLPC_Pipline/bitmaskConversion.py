# This script uses KLayout's Python API to load a GDS layout and save it as an image.

import pya

# Load layout
app = pya.Application.instance()
mw = app.main_window()
cv = mw.load_layout("layout.gds", 1)
lv = mw.current_view()

# Zoom to fit
lv.zoom_fit()

# Save image with options (file name, width, height)
# Supported formats: png, bmp, tiff
lv.save_image_with_options("output.png", 4000, 4000)

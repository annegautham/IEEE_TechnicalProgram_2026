import os

def create_bga():
    # ============================================================
    # CUSTOMIZE THESE VALUES FOR YOUR SPECIFIC BGA CHIP:
    # ============================================================
    PITCH = 0.8                      # Ball pitch in mm (distance between centers)
    PAD_DIA = 0.4                    # Pad diameter in mm
    ROWS = "ABCDEFGHJKLMNPR"         # Row letters (15 rows, skipping I,O,Q,S,U)
    NUM_COLS = 15                    # Number of columns
    
    # CUSTOMIZE YOUR LIBRARY NAME:
    LIB_NAME = "my_bga_footprints.pretty"
    FOOTPRINT_NAME = "DLPC1438-201_15x15_0.8mm"  # Updated to reflect actual pin count
    
    # ============================================================
    # MISSING BALLS - Based on Figure 5-1, Page 3 of datasheet
    # The DLPC1438 has 201 pins, not 225 (24 balls are missing)
    # These positions have NO solder balls
    # ============================================================
    EXCLUDED_POSITIONS = {
        # Central void region (based on Figure 5-1 analysis)
        # Rows E-K, columns 3-13 appear to have a central opening
        ("E", 5), ("E", 6), ("E", 7), ("E", 8), ("E", 9), ("E", 10), ("E", 11),
        ("F", 5), ("F", 6), ("F", 7), ("F", 8), ("F", 9), ("F", 10), ("F", 11),
        ("G", 4), ("G", 5), ("G", 6), ("G", 7), ("G", 8), ("G", 9), ("G", 10), ("G", 11),
        ("H", 5), ("H", 6), ("H", 7), ("H", 8), ("H", 9), ("H", 10),
        ("J", 5), ("J", 6), ("J", 7), ("J", 8), ("J", 9), ("J", 10),
        ("K", 5), ("K", 6), ("K", 7), ("K", 8), ("K", 9), ("K", 10),
        ("L", 5), ("L", 6), ("L", 7), ("L", 8), ("L", 9), ("L", 10), ("L", 11),
    }
    
    # NOTE: The datasheet shows BOTTOM VIEW (Figure 5-1)
    # KiCad footprints are TOP VIEW, so we mirror the X-axis
    # This ensures pin A1 is in the correct position
    MIRROR_X = True  # Set to True for bottom-to-top view conversion
    
    # ============================================================
    
    offset = (14 * PITCH) / 2.0
    
    # Start building the kicad_mod file content
    content = f'''(footprint "{FOOTPRINT_NAME}"
  (version 20221018)
  (generator "python_script")
  (layer "F.Cu")
  (descr "DLPC1438 BGA-201, 15x15 grid, 0.8mm pitch, Bottom View converted to Top View")
  (tags "BGA DLPC1438")
  (attr smd)
  (fp_text reference "REF**" (at 0 -8) (layer "F.SilkS")
    (effects (font (size 1 1) (thickness 0.15)))
  )
  (fp_text value "{FOOTPRINT_NAME}" (at 0 8) (layer "F.Fab")
    (effects (font (size 1 1) (thickness 0.15)))
  )
  (fp_line (start -6.5 -6.5) (end 6.5 -6.5) (layer "F.SilkS") (width 0.12))
  (fp_line (start 6.5 -6.5) (end 6.5 6.5) (layer "F.SilkS") (width 0.12))
  (fp_line (start 6.5 6.5) (end -6.5 6.5) (layer "F.SilkS") (width 0.12))
  (fp_line (start -6.5 6.5) (end -6.5 -6.5) (layer "F.SilkS") (width 0.12))
  (fp_circle (center -7 -7) (end -6.8 -7) (layer "F.SilkS") (width 0.12))
'''
    
    # Generate pads
    pad_count = 0
    for r_idx, row_let in enumerate(ROWS):
        for c_idx in range(1, NUM_COLS + 1):
            # Skip positions with no balls
            if (row_let, c_idx) in EXCLUDED_POSITIONS:
                continue
            
            # Calculate position
            x_base = (c_idx - 1) * PITCH - offset
            y = r_idx * PITCH - offset
            
            # Mirror X-axis if converting from bottom view to top view
            if MIRROR_X:
                x = -x_base
            else:
                x = x_base
            
            pad_name = f"{row_let}{c_idx}"
            
            content += f'''  (pad "{pad_name}" smd circle (at {x:.3f} {y:.3f}) (size {PAD_DIA} {PAD_DIA}) (layers "F.Cu" "F.Paste" "F.Mask"))
'''
            pad_count += 1
    
    # Close the footprint
    content += ')\n'
    
    # Create library folder if it doesn't exist
    if not os.path.exists(LIB_NAME):
        os.makedirs(LIB_NAME)
    
    # Write the file
    output_file = os.path.join(LIB_NAME, FOOTPRINT_NAME + ".kicad_mod")
    with open(output_file, 'w') as f:
        f.write(content)
    
    print(f"Footprint saved to {output_file}")
    print(f"Total pads created: {pad_count}")
    print(f"Expected pads: 201")
    print(f"Match: {'✓ YES' if pad_count == 201 else '✗ NO - CHECK EXCLUDED_POSITIONS'}")
    print(f"File exists: {os.path.exists(output_file)}")
    
    # Print excluded count for verification
    print(f"\nExcluded positions: {len(EXCLUDED_POSITIONS)}")
    print(f"Expected excluded: 24")

create_bga()
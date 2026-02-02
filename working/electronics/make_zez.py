import pcbnew
import os

def create_zez_file():
    # 1. Define the Footprint
    fp = pcbnew.Footprint()
    fp.SetName("DLPC1438_ZEZ")
    
    # 2. Minimalist Constants
    PITCH = 0.8
    PAD_DIA = 0.4
    ROWS = "ABCDEFGHJKLMNPR" # Correct ZEZ skip logic
    offset = (14 * PITCH) / 2.0

    # 3. Geometric Loop
    for r_idx, row_let in enumerate(ROWS):
        for c_idx in range(1, 16):
            # Top-view adjustment (Col 1 on Left)
            x = (c_idx - 1) * PITCH - offset
            y = r_idx * PITCH - offset
            
            pad = pcbnew.PAD(fp)
            pad.SetNumber(f"{row_let}{c_idx}")
            pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
            pad.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
            pad.SetSize(pcbnew.VECTOR2I_MM(PAD_DIA, PAD_DIA))
            pad.SetPosition(pcbnew.VECTOR2I_MM(x, y))
            
            # Layer assignments
            ls = pcbnew.LSET(pcbnew.F_Cu)
            ls.AddLayer(pcbnew.F_Mask)
            ls.AddLayer(pcbnew.F_Paste)
            pad.SetLayerSet(ls)
            fp.Add(pad)

    # 4. Save directly to your library
    # Replace with your actual path
    lib_path = "DLPC1438.pretty" 
    if not os.path.exists(lib_path): os.mkdir(lib_path)
    
    fp_bridge = pcbnew.FootprintLoad(lib_path, "DLPC1438_ZEZ") # Check if exists
    pcbnew.FootprintSave(lib_path, fp)
    print(f"File created: {lib_path}/DLPC1438_ZEZ.kicad_mod")

if __name__ == "__main__":
    create_zez_file()
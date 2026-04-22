from PIL import Image
import numpy as np

def remove_black_markers(input_path, output_path, threshold=20):
    """
    Converts black/near-black pixels to white.
    threshold: 0 is strict black, 255 would catch everything. 
               20-40 is usually good for UI markers.
    """
    # Open image and ensure it's in RGB mode
    img = Image.open(input_path).convert("RGB")
    data = np.array(img)

    # Create a mask of pixels where R, G, and B are all below the threshold
    # (This identifies the black squares and the "E" text)
    black_mask = (data[:, :, 0] <= threshold) & \
                 (data[:, :, 1] <= threshold) & \
                 (data[:, :, 2] <= threshold)

    # Change those pixels to white [255, 255, 255]
    data[black_mask] = [255, 255, 255]

    # Save the cleaned image
    clean_img = Image.fromarray(data)
    clean_img.save(output_path)
    # print(f"Cleaned image saved to: {output_path}")

# Example Usage:
# remove_black_markers("tile_y00_x00.png", "tile_cleaned.png")
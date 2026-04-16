import serial.tools.list_ports
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

def get_com_ports():
    """Opens dialog to select COM port."""
    root = tk.Tk()
    root.title("Select Port")
    root.attributes("-topmost", True)
    
    # Detect Ports
    ports = [p.device for p in serial.tools.list_ports.comports()]
    if not ports:
        print("No ports found.")
        root.destroy()
        return None

    # Variable to hold choice
    selected_port = tk.StringVar(value=ports[0])

    # UI Elements
    tk.Label(root, text="COM Port:").pack(padx=20, pady=5)
    
    dropdown = ttk.Combobox(root, textvariable=selected_port, values=ports, state="readonly")
    dropdown.pack(padx=20, pady=5)

    def on_confirm():
        root.quit()     # Stop the mainloop
        root.destroy()  # Close the window hardware
        root.update()

    tk.Button(root, text="Confirm", command=on_confirm).pack(pady=10)

    root.mainloop()
    return selected_port.get()

def select_file_ui():
    """Opens a dialog to select a single image."""
    root = tk.Tk()
    root.withdraw() 
    file_path = filedialog.askopenfilename(
        title="Select Image to Send",
        filetypes=[("Image Files", "*.png *.jpg *.jpeg *.bmp *.tiff"), ("All Files", "*.*")]
    )
    root.destroy()
    return file_path

def select_folder_ui():
    """Opens a dialog to select a folder."""
    root = tk.Tk()
    root.withdraw()
    folder_path = filedialog.askdirectory(title="Select Folder to Send")
    root.destroy()
    return folder_path
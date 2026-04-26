# --------------------------------------------------------------------------------------
# Project: OpenMicroManipulator
# License: MIT (see LICENSE file for full description)
#          All text in here must be included in any redistribution.
# Author:  M. S. (diffraction limited)
# --------------------------------------------------------------------------------------

import os
import sys

# Disable scaling
os.environ['QT_SCALE_FACTOR'] = '1'
os.environ['QT_AUTO_SCREEN_SCALE_FACTOR'] = '0'
os.environ['GDK_SCALE'] = '1'
os.environ['GDK_DPI_SCALE'] = '1'

from PySide6.QtWidgets import QApplication
from serial.tools import list_ports

from hardware.open_micro_stage_api import OpenMicroStageInterface
from mainwindow import DeviceControlMainWindow


# Raspberry Pi Foundation USB VID (covers Pico / Pico 2 CDC serial)
PICO_USB_VID = 0x2E8A


def auto_detect_port():
    """Return the first serial port that looks like a Pico, or None."""
    ports = list(list_ports.comports())

    # Prefer an exact VID match — most reliable across OSes.
    for p in ports:
        if p.vid == PICO_USB_VID:
            return p.device

    # Fallback to name-based matching for generic CDC enumerations.
    def looks_like_pico(name: str) -> bool:
        return (
            'usbmodem' in name      # macOS
            or 'ACM' in name        # Linux
            or name.upper().startswith('COM')  # Windows
        )

    for p in ports:
        if looks_like_pico(p.device):
            return p.device

    return None


def main():
    # Allow override via CLI arg, otherwise auto-detect.
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        port = auto_detect_port()
        if port is None:
            print("No Pico serial port found. Pass the port as an argument, e.g.")
            print(f"  {sys.argv[0]} /dev/tty.usbmodem1101")
            sys.exit(1)
        print(f"Auto-detected port: {port}")

    # create interface and connect
    oms = OpenMicroStageInterface(show_communication=True, show_log_messages=True)
    oms.connect(port)

    # Setup camera — set to None to disable (no camera connected)
    camera = None
    # camera = OpenCVCamera(camera_index=0)   # USB webcam
    # camera = BaslerCamera()                 # Basler industrial camera

    # ------------------------------------------------------------------------

    # create the Qt app and GUI
    app = QApplication()
    gui = DeviceControlMainWindow(oms, camera)
    gui.show()

    # Start the Qt event loop (camera loop not used when camera=None)
    app.exec()

if __name__ == "__main__":
    main()
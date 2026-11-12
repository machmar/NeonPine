#!/usr/bin/env python3
"""Flash the NeonPine firmware over USB (Linux + Windows, no extra packages).

  npflash.py [image.bin] [--wait S]

Uses the companion app's flasher (a Python port of minichlink's b003boot programmer): reboots a running tree
into the USB bootloader (1209:B003), erases, writes, verifies and starts the image. The settings / custom
animation store above 15 360 bytes is left alone.

If no tree is running (broken firmware, or it can't reboot itself into the bootloader) it waits --wait
seconds for the bootloader: unplug the tree and plug it back in, the bootloader listens for ~5 s after power-up.
"""
import argparse
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "companion"))
from neonpine_companion import device, flasher  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("image", nargs="?", default=os.path.join(HERE, "..", "neonpine", "neonpine.bin"))
    ap.add_argument("--wait", type=float, default=8.0, help="seconds to wait for the bootloader (default 8)")
    a = ap.parse_args()

    image = open(a.image, "rb").read()
    t0 = time.monotonic()
    log = lambda s: print("%5.1f s  %s" % (time.monotonic() - t0, s), flush=True)
    log("%s: %d bytes" % (os.path.relpath(a.image), len(image)))
    if not device.find_trees() and not device.find_bootloaders():
        log("No tree on USB - unplug it and plug it back in now (waiting %.0f s)" % a.wait)
    try:
        ok = flasher.flash_firmware(image, log=log, wait_s=a.wait)
    except (flasher.FlashError, device.DeviceError, OSError) as e:
        log("FAILED: %s" % e)
        if "didn't show up" in str(e):
            print("\nThe tree didn't come up as the bootloader. Run this again with --wait 60 and replug the tree\n"
                  "while it waits (the bootloader listens for ~5 s after power-up).")
        return 1
    return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(main())

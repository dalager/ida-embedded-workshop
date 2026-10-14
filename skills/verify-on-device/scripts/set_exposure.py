#!/usr/bin/env python3
"""Put a UVC webcam in manual exposure and set the exposure time (raw V4L2 ioctl, no v4l2-ctl).
usage: set_exposure.py /dev/videoN <exposure_absolute>
Auto-exposure turns the bright round panel into a white disc, so manual is mandatory here."""
import fcntl, struct, sys

V4L2_CID_EXPOSURE_AUTO = 0x009A0901      # 1 = manual, 3 = aperture priority (auto)
V4L2_CID_EXPOSURE_ABSOLUTE = 0x009A0902
VIDIOC_S_CTRL = 0xC008561C
VIDIOC_G_CTRL = 0xC008561B

def set_ctrl(fd, cid, val):
    fcntl.ioctl(fd, VIDIOC_S_CTRL, struct.pack("Ii", cid, val))

def get_ctrl(fd, cid):
    buf = bytearray(struct.pack("Ii", cid, 0))
    fcntl.ioctl(fd, VIDIOC_G_CTRL, buf)
    return struct.unpack("Ii", buf)[1]

def main():
    dev, exposure = sys.argv[1], int(sys.argv[2])
    with open(dev, "rb", buffering=0) as fd:
        set_ctrl(fd, V4L2_CID_EXPOSURE_AUTO, 1)
        set_ctrl(fd, V4L2_CID_EXPOSURE_ABSOLUTE, exposure)
        print("exposure_auto=%d exposure_absolute=%d" % (
            get_ctrl(fd, V4L2_CID_EXPOSURE_AUTO), get_ctrl(fd, V4L2_CID_EXPOSURE_ABSOLUTE)))

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Mean channel value of the librashader chain-image dumps, for photometric comparisons.

Measures the PNG pairs that `LibrashaderDumpChainImages` writes under <User>/Dump/Textures/, and the
OS-level window captures taken beside them, so that "the shader renders too dark" becomes a number.
The result of using it is recorded in
docs/superpowers/specs/2026-09-16-librashader-desktop-runtimes-design.md section 7.

Why ffmpeg rather than PIL: no third-party Python dependency, ffmpeg decodes PNG exactly, and
averaging raw bytes is verifiable against a synthetic flat field. Generate such a field as rawvideo,
not with ffmpeg's lavfi `color=` source -- that one goes through YUV and reports 126 for 128.

Usage:
    ./chain-dump-mean.py [--crop X,Y,W,H] [--rgb10a2] <image.png> [<image.png> ...]
    ./chain-dump-mean.py [--crop X,Y,W,H] [--rgb10a2-output] --ratio <input.png> <output.png>

--ratio prints the output/input mean ratio as a percentage, which is the number that section's
tables are expressed in. It does NOT require the two images to be the same size: the chain routinely
scales, and a mean is resolution-independent.

--crop restricts every image to a rectangle before averaging, and exists for the OS-level window
captures: those include the title bar and the letterbox borders, which are near-black and would pull
both means toward zero, compressing any ratio toward 100% and hiding exactly the effect being
measured. The chain dumps need no crop -- they are the chain's own images.

--rgb10a2 says the PNG's bytes are not really RGBA8 but packed A2B10G10R10 words, and unpacks them
before averaging. This is needed for the chain OUTPUT dump, and is the difference between a real
measurement and a fabricated one. The chain output target inherits the backbuffer's format, and
AbstractTexture::Save copies the source texture's native bytes into an RGBA8 staging texture and
hands them to the PNG encoder unconverted -- so on a 10-bit swapchain (this UAT host's Vulkan and
D3D12 both pick one) the PNG is a valid image file full of misread 10-bit words. It looks plausible:
measured 2026-09-18, a pure passthrough preset's output dump read as RGBA8 came out 160% of its own
input, and unpacking it reproduced the input to within 0.05/255. The giveaway is the alpha channel --
an opaque 10-bit image read as RGBA8 has alpha in exactly 192..255, because a 2-bit alpha of 3 sits
in the top two bits of the fourth byte. Values are rescaled to 0-255 so they compare directly with
the 8-bit path. Never pass this for a screen capture or a chain INPUT dump; those really are RGBA8,
and unpacking one gives nonsense (that is a useful control: the input dump unpacks to B 251.8).
--rgb10a2-output is the --ratio form: it unpacks only <output.png>, which is the pairing that
actually occurs, since the input dump is the RGBA8 EFB texture and only the output target follows the
backbuffer.

Reports per-channel means as well as the overall mean, because a chain that darkens uniformly and a
chain that has lost a channel are different bugs and the overall mean alone cannot tell them apart.

Alpha is reported too -- mean, min and max -- and it is not decoration. It is the only evidence in
the file itself of which of the two formats above it is in, so a tool that teaches the 192..255 rule
and then declines to read alpha leaves its user to check by hand in some other program, which is how
the fabricated 160% number survived as long as it did. When a file's alpha lies entirely within
192..255 and at least one pixel falls short of 255, while --rgb10a2 was not passed, that is the
packed-word signature and the tool says so instead of printing a plausible-looking mean. (The
"falls short of 255" half matters: an ordinary opaque RGBA8 image has alpha exactly 255 in every
pixel, which is inside 192..255 as well, so the window alone would fire on every screen capture. In
a misread 10-bit image the low six bits of that byte are the top six bits of blue, so alpha varies
with the picture and reaches 255 only where blue is near full.)
"""

import struct
import subprocess
import sys


def decode(path, crop, pixel_format):
    """Return `path` decoded to raw `pixel_format` bytes, cropped, via ffmpeg."""
    command = ["ffmpeg", "-v", "error", "-i", path]
    if crop is not None:
        x, y, w, h = crop
        command += ["-vf", f"crop={w}:{h}:{x}:{y}"]
    command += ["-f", "rawvideo", "-pix_fmt", pixel_format, "-"]
    proc = subprocess.run(command, capture_output=True)
    if proc.returncode != 0:
        raise SystemExit(f"ffmpeg failed on {path}:\n{proc.stderr.decode(errors='replace')}")
    if not proc.stdout:
        raise SystemExit(f"ffmpeg produced no pixels for {path}")
    return proc.stdout


def mean_channels(path, crop=None, rgb10a2=False):
    """Return (means_rgb, mean_overall, pixel_count, alpha) for a PNG, via ffmpeg.

    `alpha` is (mean, min, max, scale_max): the 2-bit alpha on its own 0..3 scale when unpacking,
    otherwise the raw fourth byte on 0..255. Both are decoded as rgba -- ffmpeg fills alpha with
    255 for a PNG that has none, which is exactly the "genuinely opaque RGBA8" case the check below
    wants to be able to tell apart from a misread 10-bit word.
    """
    raw = decode(path, crop, "rgba")
    if len(raw) % 4 != 0:
        raise SystemExit(f"{path}: {len(raw)} bytes is not a whole number of 32-bit words")
    pixels = len(raw) // 4
    if rgb10a2:
        sums = [0, 0, 0]
        alphas = []
        for word in struct.unpack(f"<{pixels}I", raw):
            sums[0] += word & 0x3FF
            sums[1] += (word >> 10) & 0x3FF
            sums[2] += (word >> 20) & 0x3FF
            alphas.append(word >> 30)
        # Rescale 10-bit to 0-255 rather than reporting a second, incomparable scale. Alpha keeps
        # its own 0..3 scale: that is the number the format stores, and 3 means opaque.
        means = [s / pixels / (1023.0 / 255.0) for s in sums]
        alpha = (sum(alphas) / pixels, min(alphas), max(alphas), 3)
    else:
        means = [sum(raw[channel::4]) / pixels for channel in range(3)]
        alpha_bytes = raw[3::4]
        alpha = (sum(alpha_bytes) / pixels, min(alpha_bytes), max(alpha_bytes), 255)
    return means, sum(means) / 3.0, pixels, alpha


def looks_like_packed_10_bit(alpha):
    """True when this file's alpha carries the signature of A2B10G10R10 misread as RGBA8.

    Opaque 10-bit words have A == 3, which occupies the top two bits of the fourth byte, so that
    byte is 192 + (top six bits of blue): inside 192..255 always, and 255 only in pixels where blue
    is nearly full. A real RGBA8 image is either opaque -- alpha exactly 255 in every pixel -- or it
    has transparency, which takes it below 192. So the test is the window with the all-255 case carved
    out, i.e. a minimum of 192..254: the window alone would flag every opaque screen capture, while
    demanding that the maximum stay under 255 would miss any frame containing one near-white pixel,
    which is most of them.
    """
    mean, low, high, scale_max = alpha
    return scale_max == 255 and 192 <= low <= 254


def report(path, crop=None, rgb10a2=False):
    means, overall, pixels, alpha = mean_channels(path, crop, rgb10a2)
    r, g, b = means
    alpha_mean, alpha_min, alpha_max, alpha_scale = alpha
    notes = "" if crop is None else "  crop=%d,%d,%d,%d" % crop
    notes += "  as A2B10G10R10" if rgb10a2 else ""
    print(f"{path}{notes}")
    print(f"  pixels {pixels}")
    print(f"  mean R {r:8.3f}  G {g:8.3f}  B {b:8.3f}")
    print(f"  mean overall {overall:8.3f}")
    print(f"  alpha mean {alpha_mean:8.3f}  min {alpha_min}  max {alpha_max}  of {alpha_scale}")
    if rgb10a2 and alpha_min != 3:
        print("  NOTE: unpacked alpha is not 3 everywhere, so this is not a plainly opaque 10-bit")
        print("        image; check that --rgb10a2 belongs on this file at all.")
    elif looks_like_packed_10_bit(alpha):
        print("  WARNING: alpha stays inside 192..255 yet is not a constant 255, which is what")
        print("           packed A2B10G10R10 words look like read as RGBA8. The means above are")
        print("           then fabricated -- re-run with --rgb10a2 (--rgb10a2-output for --ratio).")
    return overall


def parse_crop(spec):
    try:
        x, y, w, h = (int(part) for part in spec.split(","))
    except ValueError:
        raise SystemExit(f"--crop wants four integers X,Y,W,H, got {spec!r}") from None
    if w <= 0 or h <= 0:
        raise SystemExit(f"--crop needs a positive width and height, got {w}x{h}")
    return x, y, w, h


def main(argv):
    args = argv[1:]
    crop = None
    # Two flags rather than one, because the pairing is asymmetric: a chain input dump really is
    # RGBA8 while the chain output dump beside it is packed, so a single flag spanning both images
    # of a --ratio could only ever be wrong for one of them.
    packed_all = False
    packed_output = False
    while args and args[0].startswith("--") and args[0] != "--ratio":
        flag = args.pop(0)
        if flag == "--crop":
            if not args:
                raise SystemExit("--crop needs an X,Y,W,H argument")
            crop = parse_crop(args.pop(0))
        elif flag == "--rgb10a2":
            packed_all = True
        elif flag == "--rgb10a2-output":
            packed_output = True
        else:
            raise SystemExit(f"unknown option {flag!r}\n{__doc__}")

    if args and args[0] == "--ratio":
        if len(args) != 3:
            raise SystemExit("--ratio takes exactly two images: <input.png> <output.png>")
        src = report(args[1], crop, packed_all)
        dst = report(args[2], crop, packed_all or packed_output)
        if src == 0:
            raise SystemExit("input mean is 0; a ratio against it is meaningless")
        print(f"\noutput/input = {100.0 * dst / src:.1f}%")
        return 0

    if packed_output:
        raise SystemExit("--rgb10a2-output only means something with --ratio; use --rgb10a2")
    if not args:
        raise SystemExit(__doc__)
    for path in args:
        report(path, crop, packed_all)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

import argparse

parser = argparse.ArgumentParser()
parser.add_argument("firmware", help="Path to firmware .bin file")
args = parser.parse_args()

with open(args.firmware, "rb") as f:
    firmware = f.read()

print(f"\nLoaded {len(firmware)} bytes from {args.firmware}\n")

# Pretty hex dump (16 bytes per line)
BYTES_PER_LINE = 16

for offset in range(0, len(firmware), BYTES_PER_LINE):
    chunk = firmware[offset:offset + BYTES_PER_LINE]

    # Hex part
    hex_bytes = " ".join(f"{b:02X}" for b in chunk)

    # Optional ASCII (nice for spotting patterns)
    ascii_repr = "".join(chr(b) if 32 <= b <= 126 else "." for b in chunk)

    print(f"{offset:08X}  {hex_bytes:<48}  {ascii_repr}")
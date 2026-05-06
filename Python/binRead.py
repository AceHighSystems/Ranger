import argparse

parser = argparse.ArgumentParser()
parser.add_argument("firmware", help="Path to firmware .bin file")
args = parser.parse_args()

with open(args.firmware, "rb") as f:
    firmware = f.read()

print(f"Loaded {len(firmware)} bytes from {args.firmware}")
#!/usr/bin/env python3
"""Report occupancy-grid quality from a map_saver .pgm/.yaml pair."""
import sys, re

stem = sys.argv[1]                       # path without extension
with open(stem + ".yaml") as f:
    y = f.read()
res = float(re.search(r"resolution:\s*([\d.]+)", y).group(1))

# Parse binary PGM (P5).
with open(stem + ".pgm", "rb") as f:
    assert f.readline().strip() == b"P5"
    line = f.readline()
    while line.startswith(b"#"):
        line = f.readline()
    w, h = map(int, line.split())
    maxv = int(f.readline())
    px = f.read(w * h)

# map_saver convention: 254=free, 0=occupied, 205=unknown.
free = sum(1 for v in px if v >= 250)
occ  = sum(1 for v in px if v <= 50)
unk  = w * h - free - occ
tot  = w * h
known = free + occ

print(f"grid: {w} x {h} px  @ {res:.3f} m/px  =>  {w*res:.1f} x {h*res:.1f} m")
print(f"free    : {free:7d}  ({100*free/tot:5.1f}%)")
print(f"occupied: {occ:7d}  ({100*occ/tot:5.1f}%)")
print(f"unknown : {unk:7d}  ({100*unk/tot:5.1f}%)")
print(f"known    coverage (free+occ): {100*known/tot:5.1f}%")
if known:
    print(f"occupied / known (wall density): {100*occ/known:5.1f}%")
mapped_area = known * res * res
print(f"mapped (known) area: {mapped_area:.1f} m^2")

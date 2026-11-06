"""Compare integer tuning with the previous floating-point formula."""
import math
from pathlib import Path

source = (Path(__file__).resolve().parent.parent / "pitch_table.hpp").read_text()
table = [int(v) for v in source.split("{", 1)[1].split("}", 1)[0].split(",") if v.strip()]
assert len(table) == 3072
maximum = 0
for rate in (11999, 12000, 22050, 32000):
    for pitch in range(-12288, 12289):
        octave, fraction = divmod(pitch, 3072)
        ratio = table[fraction]
        ratio = ratio << octave if octave >= 0 else ratio >> -octave
        actual = (ratio * 12000 // rate) >> 8
        reference = int(65536 * 12000 / rate * math.pow(2, pitch / 3072))
        maximum = max(maximum, abs(actual - reference))
assert maximum <= 1, maximum
print(f"PITCH PASS 98308 cases; maximum step difference={maximum}/65536 sample")

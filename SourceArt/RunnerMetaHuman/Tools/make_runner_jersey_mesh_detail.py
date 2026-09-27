"""Make a small tileable polyester-knit normal map for Runner's jersey."""
from math import cos, sin, pi, sqrt
from pathlib import Path

import bpy

ROOT = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/RunnerMetaHuman')
SIZE = 512
REPEATS = 28
image = bpy.data.images.new('Runner_JerseyMeshNormal', width=SIZE, height=SIZE,
                            alpha=True, float_buffer=False)
pixels = []
for y in range(SIZE):
    phase_y = 2.0 * pi * REPEATS * y / SIZE
    for x in range(SIZE):
        phase_x = 2.0 * pi * REPEATS * x / SIZE
        # Two crossing thread directions leave shallow oval pores. Keep the
        # relief deliberately low so it reads as fabric, not metal or scales.
        dx = .065 * sin(phase_x) * cos(phase_y)
        dy = .055 * cos(phase_x) * sin(phase_y)
        length = sqrt(dx * dx + dy * dy + 1.0)
        pixels.extend((.5 + dx / (2 * length),
                       .5 + dy / (2 * length),
                       .5 + 1.0 / (2 * length), 1.0))
image.pixels.foreach_set(pixels)
image.filepath_raw = str(ROOT / 'T_Runner_JerseyMeshNormal.png')
image.file_format = 'PNG'
image.save()
print('RUNNER_JERSEY_MESH_NORMAL_SAVED', image.filepath_raw, SIZE)

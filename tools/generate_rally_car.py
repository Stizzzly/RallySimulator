"""Generate a low-poly Group-A hatchback with Delta HF Integrale cues.

It deliberately uses original, texture-free geometry. Coordinates match the
demo: X = width, Y = height, Z = length; front points toward -Z.
"""

from pathlib import Path

verts: list[tuple[float, float, float]] = []
faces: list[tuple[int, int, int]] = []


def p(x: float, y: float, z: float) -> int:
    verts.append((x, y, z))
    return len(verts)


def q(a: int, b: int, c: int, d: int) -> None:
    faces.extend(((a, b, c), (a, c, d)))


def box(x0: float, x1: float, y0: float, y1: float, z0: float, z1: float) -> None:
    ids = [p(x0,y0,z0), p(x1,y0,z0), p(x1,y1,z0), p(x0,y1,z0),
           p(x0,y0,z1), p(x1,y0,z1), p(x1,y1,z1), p(x0,y1,z1)]
    q(ids[0],ids[3],ids[2],ids[1]); q(ids[4],ids[5],ids[6],ids[7])
    q(ids[0],ids[1],ids[5],ids[4]); q(ids[3],ids[7],ids[6],ids[2])
    q(ids[0],ids[4],ids[7],ids[3]); q(ids[1],ids[2],ids[6],ids[5])


def wedge(rings: list[list[tuple[float, float, float]]]) -> None:
    ids = [[p(*v) for v in ring] for ring in rings]
    for a, b in zip(ids, ids[1:]):
        for i in range(4): q(a[i], a[(i + 1) % 4], b[(i + 1) % 4], b[i])
    q(ids[0][0],ids[0][1],ids[0][2],ids[0][3])
    q(ids[-1][3],ids[-1][2],ids[-1][1],ids[-1][0])


# Wide, slab-sided Group-A lower shell with very short overhangs.
wedge([
    [(-.96,.31,-2.10),(.96,.31,-2.10),(.96,.96,-2.10),(-.96,.96,-2.10)],
    [(-.94,.27,-1.56),(.94,.27,-1.56),(.94,.94,-1.56),(-.94,.94,-1.56)],
    [(-.91,.26,1.63),(.91,.26,1.63),(.91,.96,1.63),(-.91,.96,1.63)],
    [(-.94,.32,2.04),(.94,.32,2.04),(.94,1.03,2.04),(-.94,1.03,2.04)],
])

# Long flat bonnet, square rear deck, shallow screen and nearly upright hatch.
wedge([
    [(-.80,.92,-2.03),(.80,.92,-2.03),(.80,.92,-.52),(-.80,.92,-.52)],
    [(-.76,1.08,-1.85),(.76,1.08,-1.85),(.76,1.08,-.48),(-.76,1.08,-.48)],
])
box(-.83,.83,.94,1.10,1.25,1.92)
wedge([
    [(-.78,.95,-.58),(.78,.95,-.58),(.78,.95,1.31),(-.78,.95,1.31)],
    [(-.67,1.55,-.22),(.67,1.55,-.22),(.67,1.55,.92),(-.67,1.55,.92)],
    [(-.70,1.49,1.35),(.70,1.49,1.35),(.70,1.12,1.62),(-.70,1.12,1.62)],
])

# Broad, square flares. The game renders the wheels separately.
for z in (-1.35, 1.32):
    box(-1.17,-.80,.30,.90,z-.46,z+.46)
    box(.80,1.17,.30,.90,z-.46,z+.46)
box(-1.08,1.08,.25,.40,-.94,1.00)

# Delta-style blunt nose: divided grille, four lamps and a ribbed deep bumper.
box(-1.00,1.00,.36,.70,-2.23,-2.06)
box(-.66,-.16,.68,.94,-2.27,-2.10); box(.16,.66,.68,.94,-2.27,-2.10)
box(-.91,-.69,.72,.96,-2.28,-2.11); box(.69,.91,.72,.96,-2.28,-2.11)
box(-.10,.10,.65,.99,-2.29,-2.11)
for x in (-.72,-.35,.35,.72): box(x-.12,x+.12,.75,.94,-2.31,-2.26)
for x in (-.66,-.22,.22,.66): box(x-.08,x+.08,.45,.63,-2.30,-2.22)

# Rear lamps, blunt bumper and tall detached roof wing.
box(-.98,.98,.35,.66,1.99,2.17)
box(-.89,-.61,.69,.96,2.03,2.18); box(.61,.89,.69,.96,2.03,2.18)
box(-.83,-.65,1.19,1.54,1.48,1.63); box(.65,.83,1.19,1.54,1.48,1.63)
box(-1.05,1.05,1.52,1.65,1.42,1.82)

# Scoop, door creases and handles preserve readable detail with no textures.
box(-.34,.34,1.08,1.17,-1.30,-.68)
box(-1.00,-.94,.79,.88,-.34,1.15); box(.94,1.00,.79,.88,-.34,1.15)
box(-.79,-.58,1.04,1.10,.47,.57); box(.58,.79,1.04,1.10,.47,.57)

out = Path(__file__).resolve().parents[1] / "car.obj"
with out.open("w", encoding="ascii", newline="\n") as obj:
    obj.write("# Low-poly Group-A hatchback for RallySimulator\n")
    obj.write("# Original Delta HF Integrale-inspired geometry; front faces -Z\n")
    for x, y, z in verts: obj.write(f"v {x:.4f} {y:.4f} {z:.4f}\n")
    for a, b, c in faces: obj.write(f"f {a} {b} {c}\n")

print(f"Wrote {out}: {len(verts)} vertices, {len(faces)} triangles")

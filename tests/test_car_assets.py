"""Regression tests for the OBJ/UV/BMP assets consumed by RallyGame."""

from pathlib import Path
import re
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
OBJ = ASSETS / "lancia_delta_volterra.obj"
MTL = ASSETS / "lancia_delta_volterra.mtl"
BMP = ASSETS / "lancia_delta_volterra.bmp"
SOURCE = Path(r"C:\Users\ADMIN\AppData\Local\Temp\lancia_delta_HF_alta.png")


def bmp_size(path: Path):
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise AssertionError(f"{path} is not a BMP")
    return struct.unpack_from("<ii", data, 18)


def png_size(path: Path):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise AssertionError(f"{path} is not a PNG")
    return struct.unpack_from(">II", data, 16)


class CarAssetTests(unittest.TestCase):
    def test_texture_is_a_uv_safe_bmp(self):
        self.assertTrue(BMP.is_file())
        self.assertEqual(bmp_size(BMP), png_size(SOURCE))

    def test_mtl_references_the_shipped_texture(self):
        self.assertIn("map_Kd lancia_delta_volterra.bmp", MTL.read_text(encoding="utf-8"))

    def test_obj_has_valid_triangle_uv_indices(self):
        vertices, uvs, faces = [], [], []
        for line in OBJ.read_text(encoding="utf-8").splitlines():
            if line.startswith("v "):
                vertices.append(line)
            elif line.startswith("vt "):
                u, v = map(float, line.split()[1:3])
                self.assertGreaterEqual(u, 0.0); self.assertLessEqual(u, 1.0)
                self.assertGreaterEqual(v, 0.0); self.assertLessEqual(v, 1.0)
                uvs.append(line)
            elif line.startswith("f "):
                faces.append(line.split()[1:])

        self.assertGreater(len(vertices), 3000)
        self.assertGreater(len(uvs), 2000)
        self.assertGreater(len(faces), 3000)
        for face in faces:
            self.assertEqual(len(face), 3)
            for token in face:
                match = re.fullmatch(r"(\d+)/(\d+)/(\d+)", token)
                self.assertIsNotNone(match, token)
                vertex, uv, _normal = map(int, match.groups())
                self.assertTrue(1 <= vertex <= len(vertices))
                self.assertTrue(1 <= uv <= len(uvs))

    def test_obj_is_blender_z_up(self):
        coords = [tuple(map(float, line.split()[1:4])) for line in OBJ.read_text(encoding="utf-8").splitlines() if line.startswith("v ")]
        ys = [v[1] for v in coords]
        zs = [v[2] for v in coords]
        self.assertGreater(max(ys) - min(ys), 3.5)  # vehicle length
        self.assertLess(min(zs), 0.02)
        self.assertGreater(max(zs), 1.3)            # vehicle height


if __name__ == "__main__":
    unittest.main()

"""
NAIJA HUSTLE colour grade lookup table.

Makes T_NHGrade_LUT, the 256x16 colour lookup table the lighting rig mixes in last (NHLightingRig > Grade LUT, and
"Lut Intensity" on each preset). Run it from the editor: Tools > Execute Python Script.

To change the look, edit GRADE below and run the script again. Or paint your own: open the PNG this script writes
(Saved/NaijaHustle/T_NHGrade_LUT.png) in an image editor, grade it together with a screenshot, and import the result
over the asset. With every value at its neutral setting the table changes nothing.
"""
import os
import struct
import sys
import zlib

SIZE = 16  # 16 slices of 16x16: red runs across a slice, green down it, blue from slice to slice
LUT_DIR = "/Game/NaijaHustle/Lighting/Presets"
LUT_NAME = "T_NHGrade_LUT"

# The look: a worn film print. Values act on display colour (0 to 1).
GRADE = {
    "contrast": 0.18,                  # S-curve strength (0 = none)
    "shadow_tint": (0.0, 0.035, 0.05),   # added to the darkest tones: teal
    "highlight_tint": (0.03, 0.01, -0.04), # added to the brightest tones: warm
    "saturation": 0.92,                # 1 = unchanged
    "highlight_desaturation": 0.15,    # how much the brightest tones lose colour
    "black_point": 0.012,              # lifted blacks: 0 = true black
}
NEUTRAL = {"contrast": 0.0, "shadow_tint": (0, 0, 0), "highlight_tint": (0, 0, 0), "saturation": 1.0,
           "highlight_desaturation": 0.0, "black_point": 0.0}


def grade(rgb, g=GRADE):
    """One colour through the grade; components in and out are 0 to 1."""
    luma = 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2]
    out = []
    for i, c in enumerate(rgb):
        s = c * c * (3.0 - 2.0 * c)                       # smoothstep: an S-curve through the mid point
        c = c + (s - c) * g["contrast"]
        c += g["shadow_tint"][i] * (1.0 - luma) ** 2 + g["highlight_tint"][i] * luma ** 2
        out.append(c)
    luma2 = 0.2126 * out[0] + 0.7152 * out[1] + 0.0722 * out[2]
    sat = g["saturation"] * (1.0 - g["highlight_desaturation"] * luma * luma)
    out = [luma2 + (c - luma2) * sat for c in out]
    return tuple(min(1.0, max(0.0, g["black_point"] + c * (1.0 - g["black_point"]))) for c in out)


def lut_rows(g=GRADE):
    """The table as rows of (r, g, b) bytes, top row first"""
    rows = []
    for gi in range(SIZE):
        row = []
        for bi in range(SIZE):
            for ri in range(SIZE):
                c = grade((ri / (SIZE - 1), gi / (SIZE - 1), bi / (SIZE - 1)), g)
                row.append(tuple(int(round(v * 255)) for v in c))
        rows.append(row)
    return rows


def write_png(path, rows):
    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + bytes(v for px in row for v in px) for row in rows)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", len(rows[0]), len(rows), 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def main():
    import unreal
    png = os.path.join(unreal.Paths.project_saved_dir(), "NaijaHustle", LUT_NAME + ".png")
    write_png(png, lut_rows())
    task = unreal.AssetImportTask()
    for k, v in {"filename": png, "destination_path": LUT_DIR, "destination_name": LUT_NAME, "automated": True, "replace_existing": True, "save": False}.items():
        task.set_editor_property(k, v)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    path = f"{LUT_DIR}/{LUT_NAME}"
    tex = unreal.EditorAssetLibrary.load_asset(path)
    if not tex:
        unreal.log_error(f"NAIJA HUSTLE: could not import {png}")
        return
    # a lookup table must not be block-compressed, mip-mapped or streamed
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_COLOR_LOOKUP_TABLE)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log(f"NAIJA HUSTLE: {path} ready ({SIZE * SIZE}x{SIZE}, from {png})")


if __name__ == "__main__" or "unreal" in sys.modules:
    if "NH_NO_MAIN" not in os.environ:
        main()

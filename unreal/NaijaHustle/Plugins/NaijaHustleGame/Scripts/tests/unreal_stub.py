"""A stand-in for Unreal's `unreal` Python module, so the editor scripts can be run and checked without
the engine. It records what the scripts spawn and set, and checks property names against the C++
UPROPERTYs (as the Python API names them: snake_case, 'b' dropped from bools)."""
import math
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE = os.path.join(ROOT, "Source", "NaijaHustleGame")  # ROOT is the plugin folder


def _snake(name, is_bool):
    if is_bool and re.match(r"b[A-Z]", name):
        name = name[1:]
    s = re.sub(r"(?<=[a-z0-9])([A-Z])", r"_\1", name)
    s = re.sub(r"(?<=[A-Z])([A-Z][a-z])", r"_\1", s)
    return s.lower()


def _uprops(header, cls):
    """snake_case property names of a USTRUCT/UCLASS in one of our headers (plus its UE base, roughly)"""
    text = open(os.path.join(SOURCE, "Public", header), encoding="utf-8").read()
    m = re.search(r"(struct|class) (?:NAIJAHUSTLE(?:GAME)?_API )?" + cls + r"\b[^{]*\{(.*?)\n\};", text, re.S)
    assert m, f"{cls} not found in {header}"
    names = set()
    for t, n in re.findall(r"UPROPERTY\([^)]*(?:\([^)]*\)[^)]*)*\)\s*([\w<>:, ]+?)\s+(\w+)\s*(?:=|;)", m.group(2)):
        names.add(_snake(n, t.strip() == "bool"))
    return names


ACCESS_LOG = []


class _Obj:
    _props = None  # set of allowed names, or None for "anything"

    def __init__(self, *a, **k):
        self.__dict__["_values"] = {}

    def set_editor_property(self, name, value, notify=None):
        if self._props is not None and name not in self._props:
            raise AttributeError(f"{type(self).__name__} has no property '{name}' (known: {sorted(self._props)})")
        self._values[name] = value
        ACCESS_LOG.append((type(self).__name__, name))

    def get_editor_property(self, name):
        return self._values.get(name)


class Vector:
    def __init__(self, x=0.0, y=0.0, z=0.0):
        self.x, self.y, self.z = x, y, z


class Vector2D:
    def __init__(self, x=0.0, y=0.0):
        self.x, self.y = x, y


class LinearColor:
    def __init__(self, r=0.0, g=0.0, b=0.0, a=1.0):
        self.r, self.g, self.b, self.a = r, g, b, a


class Rotator:
    def __init__(self, roll=0.0, pitch=0.0, yaw=0.0):
        self.roll, self.pitch, self.yaw = roll, pitch, yaw


class Quat:
    def __init__(self, x=0.0, y=0.0, z=0.0, w=1.0):
        self.x, self.y, self.z, self.w = x, y, z, w

    def rotator(self):
        x, y, z, w = self.x, self.y, self.z, self.w
        yaw = math.degrees(math.atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)))
        return Rotator(yaw=yaw)


class Transform:
    def __init__(self, location=None, rotation=None, scale=None):
        self.translation, self.rotation, self.scale3d = location, rotation, scale


class Name(str):
    pass


class _Enum:
    def __init__(self, *names):
        for n in names:
            setattr(self, n, f"{type(self).__name__}.{n}")


class PropertyAccessChangeNotifyMode:
    NEVER = "never"


class Paths:
    @staticmethod
    def project_dir():
        return ROOT + os.sep

    @staticmethod
    def project_plugins_dir():
        return os.path.dirname(ROOT) + os.sep


def _struct(header, cls):
    allowed = _uprops(header, "F" + cls)
    return type(cls, (_Obj,), {"_props": allowed})


NHSurface = _struct("World/NHBlockoutActor.h", "NHSurface")
NHMarking = _struct("World/NHCityTile.h", "NHMarking")
NHPropInstance = _struct("World/NHCityTile.h", "NHPropInstance")
NHSign = _struct("World/NHCityTile.h", "NHSign")
NHShopfront = _struct("World/NHCityTile.h", "NHShopfront")
NHShape = type("NHShape", (_Enum,), {})("BOX", "CYLINDER", "SPHERE", "CONE")
NHShopfrontState = type("NHShopfrontState", (_Enum,), {})("SHUTTER", "HALF", "OPEN", "PAINTED")
NHBuildingKind = type("NHBuildingKind", (_Enum,), {})("HOUSE", "ESTATE", "TOWER", "STALL", "STILT", "FUEL_STATION", "BUS_SHELTER", "FOOTBRIDGE")
NHRoofStyle = type("NHRoofStyle", (_Enum,), {})("FLAT", "ZINC")
NHLightingPreset = type("NHLightingPreset", (_Enum,), {})("DAY", "DUSTY_NOON", "SUNSET", "NIGHT_RAIN")

ACTOR_COMMON = {"tags"}


class _Actor(_Obj):
    def __init__(self, loc=None, rot=None):
        super().__init__()
        self.loc, self.rot, self.label, self.folder, self.rebuilt = loc, rot, None, None, 0

    def set_actor_label(self, l):
        self.label = l

    def set_folder_path(self, f):
        self.folder = f

    def actor_has_tag(self, t):
        return t in [str(x) for x in (self._values.get("tags") or [])]

    def rebuild(self):
        self.rebuilt += 1


def _actor(header, cls, extra=()):
    allowed = _uprops(header, "A" + cls) | ACTOR_COMMON | set(extra)
    return type(cls, (_Actor,), {"_props": allowed})


_BLOCKOUT = _uprops("World/NHBlockoutActor.h", "ANHBlockoutActor")
NHCityTile = _actor("World/NHCityTile.h", "NHCityTile", _BLOCKOUT)
NHBlockoutBuilding = _actor("World/NHBlockoutBuilding.h", "NHBlockoutBuilding", _BLOCKOUT)
NHLightingRig = _actor("Lighting/NHLightingRig.h", "NHLightingRig")
NHLightingRig.apply_preset = lambda self, p: self._values.__setitem__("_applied", p)
PlayerStart = type("PlayerStart", (_Actor,), {"_props": ACTOR_COMMON})


class _EAS:
    def __init__(self):
        self.actors = []

    def get_all_level_actors(self):
        return list(self.actors)

    def destroy_actor(self, a):
        self.actors.remove(a)

    def spawn_actor_from_class(self, cls, loc, rot):
        a = cls(loc, rot)
        self.actors.append(a)
        return a


class _LES:
    def __init__(self):
        self.calls = []

    def load_level(self, p):
        self.calls.append(("load", p))

    def new_level(self, p, is_partitioned_world=False):
        self.calls.append(("new", p, is_partitioned_world))

    def save_current_level(self):
        self.calls.append(("save",))


class _WorldSettings(_Obj):
    _props = {"default_game_mode"}


class _World:
    def __init__(self):
        self.settings = _WorldSettings()

    def get_world_settings(self):
        return self.settings


class _UES:
    def __init__(self):
        self.world = _World()

    def get_editor_world(self):
        return self.world


class EditorActorSubsystem: pass
class LevelEditorSubsystem: pass
class UnrealEditorSubsystem: pass


class NHGameMode:
    @staticmethod
    def static_class():
        return "Class'/Script/NaijaHustleGame.NHGameMode'"


_SUBS = {EditorActorSubsystem: _EAS(), LevelEditorSubsystem: _LES(), UnrealEditorSubsystem: _UES()}


def get_editor_subsystem(cls):
    return _SUBS[cls]


class EditorAssetLibrary:
    existing = set()

    @staticmethod
    def does_asset_exist(p):
        return p in EditorAssetLibrary.existing


class ScopedSlowTask:
    def __init__(self, *a):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def make_dialog(self, *a):
        pass

    def enter_progress_frame(self, *a):
        pass


LOG = []


def log(msg):
    LOG.append(msg)
    print("[unreal.log]", msg)


sys.modules["unreal"] = sys.modules[__name__]


# ---- material editing (nh_blockout_materials.py). Expression classes accept only the properties and pins
# their real counterparts have (the subset the script uses), so a typo fails here rather than in the editor.
EXPR_PROPS = {
    "MaterialExpressionPerInstanceCustomData": ({"data_index", "default_value"}, set()),
    "MaterialExpressionScalarParameter": ({"parameter_name", "default_value", "use_custom_primitive_data", "primitive_data_index"}, set()),
    "MaterialExpressionCollectionParameter": ({"collection", "parameter_name"}, set()),
    "MaterialExpressionMultiply": ({"const_a", "const_b"}, {"A", "B"}),
    "MaterialExpressionLinearInterpolate": ({"const_a", "const_b", "const_alpha"}, {"A", "B", "Alpha"}),
    "MaterialExpressionSaturate": (set(), {""}),
    "MaterialExpressionAppendVector": (set(), {"A", "B"}),
    "MaterialExpressionVertexNormalWS": (set(), set()),
    "MaterialExpressionComponentMask": ({"r", "g", "b", "a"}, {""}),
    "MaterialExpressionSubtract": ({"const_a", "const_b"}, {"A", "B"}),
    "MaterialExpressionNoise": ({"scale", "levels", "output_min", "output_max", "quality", "noise_function", "turbulence"}, {"Position", "FilterWidth"}),
    "MaterialExpressionConstant3Vector": ({"constant"}, set()),
}
for _n, (_p, _pins) in EXPR_PROPS.items():
    globals()[_n] = type(_n, (_Obj,), {"_props": _p, "_pins": _pins})


class Material(_Obj):
    _props = {"used_with_instanced_static_meshes"}


class MaterialParameterCollection(_Obj):
    _props = {"scalar_parameters", "vector_parameters"}

    def __init__(self):
        super().__init__()
        self._values["scalar_parameters"] = []


class CollectionScalarParameter(_Obj):
    _props = {"parameter_name", "default_value"}


class MaterialFactoryNew: pass
class MaterialParameterCollectionFactoryNew: pass


class MaterialProperty:
    MP_BASE_COLOR, MP_ROUGHNESS, MP_METALLIC, MP_EMISSIVE_COLOR = "base", "rough", "metal", "emissive"


class _AssetTools:
    def create_asset(self, name, folder, cls, factory):
        return cls()


class AssetToolsHelpers:
    @staticmethod
    def get_asset_tools():
        return _AssetTools()


EditorAssetLibrary.saved = []
EditorAssetLibrary.does_directory_exist = staticmethod(lambda p: True)
EditorAssetLibrary.make_directory = staticmethod(lambda p: True)
EditorAssetLibrary.save_asset = staticmethod(lambda p: EditorAssetLibrary.saved.append(p) or True)


class MaterialEditingLibrary:
    nodes, links, outputs = [], [], {}
    graphs = []  # (nodes, links, outputs) per material built

    @staticmethod
    def delete_all_material_expressions(m):
        MaterialEditingLibrary.nodes, MaterialEditingLibrary.links, MaterialEditingLibrary.outputs = [], [], {}
        MaterialEditingLibrary.graphs.append((m, MaterialEditingLibrary))

    @staticmethod
    def create_material_expression(m, cls, x, y):
        e = cls()
        MaterialEditingLibrary.nodes.append(e)
        return e

    @staticmethod
    def connect_material_expressions(src, out, dst, pin):
        if pin not in dst._pins:
            raise ValueError(f"{type(dst).__name__} has no input '{pin}' (has {sorted(dst._pins)})")
        MaterialEditingLibrary.links.append((src, dst, pin))
        return True

    @staticmethod
    def connect_material_property(src, out, prop):
        MaterialEditingLibrary.outputs[prop] = src
        return True

    @staticmethod
    def recompile_material(m):
        m.graph = (list(MaterialEditingLibrary.nodes), list(MaterialEditingLibrary.links), dict(MaterialEditingLibrary.outputs))

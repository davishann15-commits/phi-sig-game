"""Reference-informed head modeling targets, applied before rig/accessory fitting.

These are editable artistic fits, not measurements or photo-exact reconstructions.
The photographs have different focal lengths, head pitches and facial expressions;
only consistent frontal proportions and side silhouettes are modeled here.
"""

from pathlib import Path


WORK = Path(__file__).resolve().parent
TARGET_ROOT = WORK / "cobble-tools/mpfb2/src/mpfb/data/targets"
FACE_PREFIXES = (
    "head-", "chin-", "nose-", "mouth-", "forehead-", "eyebrows-",
    "l-eye-", "r-eye-", "l-cheek-", "r-cheek-", "l-ear-", "r-ear-",
)
PROPORTION_KEY = "CobbleFaceProportions"
PHOTO_DIMENSIONS = ((4284, 5712),) * 4 + ((1170, 1560),) * 4


def _paired(category, stem, targets):
    return {
        f"{category}/{side}-{stem}-{suffix}": value
        for side in ("l", "r") for suffix, value in targets.items()
    }


# Values are deliberately submaximal: simultaneous face targets otherwise quickly
# exaggerate a likeness. Symmetric modeling avoids baking camera perspective or an
# incidental expression into the neutral game mesh. C02 eyes are obscured by glasses.
FACE_PROFILES = {
    1: {
        "head/head-rectangular": .32,
        "head/head-square": .10,
        "head/head-oval": .10,
        "head/head-scale-depth-incr": .05,
        "chin/chin-width-incr": .20,
        "chin/chin-height-incr": .09,
        "chin/chin-prominent-incr": .20,
        "chin/chin-bones-incr": .12,
        "forehead/forehead-trans-backward": .05,
        "nose/nose-scale-depth-incr": .22,
        "nose/nose-scale-vert-incr": .08,
        "nose/nose-point-width-incr": .12,
        "nose/nose-point-down": .08,
        "nose/nose-curve-convex": .05,
        "mouth/mouth-scale-horiz-decr": .05,
        "mouth/mouth-scale-vert-decr": .08,
        "mouth/mouth-upperlip-height-decr": .10,
        "mouth/mouth-lowerlip-height-decr": .03,
        "mouth/mouth-cupidsbow-incr": .06,
        "eyebrows/eyebrows-trans-down": .08,
        **_paired("eyes", "eye", {"height1-decr": .14, "height2-decr": .15,
                                   "height3-decr": .12, "eyefold-down": .10}),
        **_paired("cheek", "cheek", {"bones-incr": .08, "volume-decr": .06}),
        **_paired("ears", "ear", {"scale-decr": .08, "wing-decr": .06}),
    },
    2: {
        "head/head-oval": .30,
        "head/head-rectangular": .16,
        "head/head-scale-horiz-decr": .08,
        "chin/chin-prominent-decr": .14,
        "chin/chin-height-decr": .08,
        "chin/chin-width-decr": .05,
        "nose/nose-scale-horiz-decr": .18,
        "nose/nose-scale-depth-incr": .21,
        "nose/nose-scale-vert-incr": .12,
        "nose/nose-point-width-incr": .10,
        "nose/nose-point-down": .07,
        "mouth/mouth-scale-horiz-decr": .18,
        "mouth/mouth-upperlip-height-decr": .12,
        "mouth/mouth-lowerlip-volume-incr": .04,
        **_paired("eyes", "eye", {"height2-decr": .10}),
        **_paired("cheek", "cheek", {"volume-decr": .04}),
        **_paired("ears", "ear", {"wing-incr": .05}),
    },
    3: {
        "head/head-oval": .30,
        "head/head-scale-horiz-decr": .06,
        "head/head-fat-incr": .04,
        "chin/chin-width-incr": .02,
        "chin/chin-prominent-incr": .12,
        "chin/chin-height-decr": .04,
        "nose/nose-point-up": .26,
        "nose/nose-curve-concave": .18,
        "nose/nose-scale-depth-incr": .10,
        "nose/nose-scale-horiz-decr": .10,
        "nose/nose-scale-vert-decr": .08,
        "nose/nose-point-width-incr": .08,
        "mouth/mouth-scale-horiz-decr": .12,
        "mouth/mouth-upperlip-height-decr": .04,
        "mouth/mouth-lowerlip-volume-incr": .06,
        "eyebrows/eyebrows-angle-up": .08,
        **_paired("eyes", "eye", {"height2-decr": .15, "eyefold-up": .10}),
        **_paired("cheek", "cheek", {"volume-incr": .14, "bones-incr": .08}),
        **_paired("ears", "ear", {"scale-decr": .03, "wing-decr": .04}),
    },
    4: {
        "head/head-rectangular": .30,
        "head/head-square": .30,
        "head/head-scale-vert-incr": .06,
        "chin/chin-width-incr": .22,
        "chin/chin-bones-incr": .22,
        "chin/chin-prominent-incr": .24,
        "chin/chin-jaw-drop-incr": .06,
        "forehead/forehead-trans-forward": .05,
        "nose/nose-hump-incr": .12,
        "nose/nose-scale-depth-incr": .30,
        "nose/nose-scale-vert-incr": .15,
        "nose/nose-point-down": .12,
        "nose/nose-scale-horiz-decr": .06,
        "mouth/mouth-scale-horiz-decr": .06,
        "mouth/mouth-upperlip-height-decr": .08,
        "mouth/mouth-lowerlip-volume-incr": .08,
        "eyebrows/eyebrows-trans-forward": .16,
        "eyebrows/eyebrows-trans-down": .10,
        **_paired("eyes", "eye", {"height1-decr": .08, "height2-decr": .28,
                                   "height3-decr": .10, "eyefold-down": .08}),
        **_paired("cheek", "cheek", {"bones-incr": .20, "volume-decr": .10}),
        **_paired("ears", "ear", {"wing-incr": .15, "scale-incr": .04}),
    },
    5: {
        "head/head-round": .28,
        "head/head-oval": .12,
        "head/head-fat-incr": .10,
        "head/head-scale-horiz-incr": .14,
        "chin/chin-width-incr": .20,
        "chin/chin-prominent-decr": .10,
        "chin/chin-height-decr": .10,
        "chin/chin-bones-decr": .14,
        "nose/nose-curve-convex": .08,
        "nose/nose-scale-depth-incr": .15,
        "nose/nose-point-width-incr": .16,
        "nose/nose-nostrils-width-incr": .10,
        "nose/nose-scale-vert-decr": .08,
        "mouth/mouth-scale-horiz-decr": .18,
        "mouth/mouth-scale-vert-decr": .12,
        "mouth/mouth-upperlip-height-decr": .14,
        **_paired("eyes", "eye", {"height1-decr": .10, "height2-decr": .30,
                                   "height3-decr": .12, "bag-incr": .06}),
        **_paired("cheek", "cheek", {"volume-incr": .20, "inner-incr": .10}),
        **_paired("ears", "ear", {"wing-decr": .10, "scale-decr": .04}),
    },
    6: {
        "head/head-rectangular": .35,
        "head/head-oval": .18,
        "head/head-scale-vert-incr": .10,
        "head/head-scale-horiz-decr": .05,
        "chin/chin-prominent-decr": .12,
        "chin/chin-height-incr": .16,
        "chin/chin-width-incr": .06,
        "nose/nose-scale-depth-incr": .24,
        "nose/nose-curve-convex": .15,
        "nose/nose-point-down": .08,
        "nose/nose-width1-incr": .08,
        "nose/nose-scale-vert-incr": .10,
        "mouth/mouth-scale-horiz-decr": .10,
        "mouth/mouth-upperlip-volume-incr": .10,
        "mouth/mouth-lowerlip-volume-incr": .10,
        "mouth/mouth-philtrum-volume-incr": .03,
        "eyebrows/eyebrows-trans-forward": .08,
        "eyebrows/eyebrows-angle-up": .06,
        **_paired("eyes", "eye", {"height2-decr": .08, "eyefold-up": .12}),
        **_paired("cheek", "cheek", {"bones-incr": .08, "volume-decr": .06}),
        **_paired("ears", "ear", {"scale-incr": .10, "wing-incr": .06}),
    },
    7: {
        "head/head-round": .25,
        "head/head-square": .22,
        "head/head-fat-incr": .10,
        "head/head-scale-horiz-incr": .14,
        "chin/chin-width-incr": .28,
        "chin/chin-height-incr": .02,
        "chin/chin-prominent-incr": .05,
        "chin/chin-bones-decr": .05,
        "nose/nose-width1-incr": .16,
        "nose/nose-nostrils-width-incr": .12,
        "nose/nose-point-width-incr": .14,
        "nose/nose-scale-depth-incr": .14,
        "nose/nose-point-up": .04,
        "mouth/mouth-scale-horiz-decr": .22,
        "mouth/mouth-scale-vert-decr": .20,
        "mouth/mouth-upperlip-height-decr": .12,
        "mouth/mouth-lowerlip-height-decr": .06,
        "eyebrows/eyebrows-angle-up": .04,
        **_paired("eyes", "eye", {"height2-decr": .20, "eyefold-down": .06}),
        **_paired("cheek", "cheek", {"volume-incr": .18, "inner-incr": .12}),
        **_paired("ears", "ear", {"wing-incr": .13, "scale-vert-decr": .04}),
    },
    8: {
        "head/head-oval": .22,
        "head/head-invertedtriangular": .13,
        "head/head-scale-vert-decr": .04,
        "chin/chin-width-decr": .16,
        "chin/chin-prominent-decr": .08,
        "chin/chin-height-decr": .10,
        "nose/nose-curve-convex": .10,
        "nose/nose-scale-depth-incr": .16,
        "nose/nose-point-width-incr": .05,
        "nose/nose-point-down": .04,
        "mouth/mouth-scale-horiz-incr": .04,
        "mouth/mouth-scale-vert-decr": .06,
        "mouth/mouth-lowerlip-height-incr": .08,
        "mouth/mouth-lowerlip-volume-incr": .04,
        "eyebrows/eyebrows-trans-down": .10,
        **_paired("eyes", "eye", {"height2-decr": .06, "eyefold-up": .04}),
        **_paired("cheek", "cheek", {"bones-incr": .06}),
        **_paired("ears", "ear", {"scale-decr": .04}),
    },
}


def validate_profiles():
    """Fail before altering any mesh if a fitted target is unavailable."""
    for index, targets in FACE_PROFILES.items():
        for target, weight in targets.items():
            if not 0.0 <= weight <= 1.0:
                raise ValueError(f"C{index:02d}: invalid weight for {target}: {weight}")
            if not (TARGET_ROOT / (target + ".target.gz")).is_file():
                raise FileNotFoundError(TARGET_ROOT / (target + ".target.gz"))
    return {index: len(targets) for index, targets in FACE_PROFILES.items()}


def _fit_reference_proportions(index, human):
    """Conservatively fit front-face landmark spacing, retaining true 3D depth.

    Targets only translate small horizontal bands vertically. They do not flatten
    the face or map photographs onto planes. The same smooth deformation reaches
    the original MPFB joint/helper vertices, so subsequent rig and asset fitting
    see the changed mouth/chin positions. The eye plane and upper cranium stay put.
    """
    import numpy as np
    from cobble_face_texture import LANDMARKS

    keys = human.data.shape_keys.key_blocks
    if PROPORTION_KEY in keys:
        keys[PROPORTION_KEY].value = 0.0
    basis = np.empty(len(human.data.vertices) * 3, dtype=np.float32)
    keys[0].data.foreach_get("co", basis)
    basis = basis.reshape(-1, 3)
    mixed = basis.copy()
    buffer = np.empty(basis.size, dtype=np.float32)
    for key in keys:
        if key == keys[0] or key.mute or abs(key.value) < 1e-8:
            continue
        key.data.foreach_get("co", buffer)
        mixed += (buffer.reshape(-1, 3) - basis) * key.value

    def group_center(name):
        group = human.vertex_groups.get(name)
        indices = [v.index for v in human.data.vertices
                   if group and any(g.group == group.index and g.weight > .5 for g in v.groups)]
        if not indices:
            raise RuntimeError(f"Proportion fitting requires the MPFB {name} helper group")
        return mixed[indices].mean(axis=0)

    eye_l = group_center("joint-l-eye")
    eye_r = group_center("joint-r-eye")
    eye_z = float((eye_l[2] + eye_r[2]) * .5)
    eye_width = float(abs(eye_l[0] - eye_r[0]))
    mouth = group_center("lips")
    x, y, z = mixed.T
    nose_points = mixed[(abs(x) < .013) & (z > mouth[2] + .014) & (z < eye_z + .004)]
    if not len(nose_points):
        raise RuntimeError("No nose surface found for proportion fitting")
    nose_z = float(nose_points[np.argmin(nose_points[:, 1]), 2])
    chin_points = mixed[(abs(x) < .021) & (z < mouth[2] - .020) &
                        (z > mouth[2] - .09) & (y < mouth[1] + .022)]
    if not len(chin_points):
        raise RuntimeError("No chin surface found for proportion fitting")
    chin_z = float(chin_points[:, 2].min())

    photo = LANDMARKS[index - 1]
    width, height = PHOTO_DIMENSIONS[index - 1]
    pixel_scale = eye_width / (2.0 * photo["eyehalf"] * width)
    original = {"chin": chin_z, "mouth": float(mouth[2]), "nose": nose_z}
    # Substantial head pitch and/or sunglasses make C02/C03/C07 less certain.
    confidence = (.85, .55, .65, .85, .85, .85, .65, .80)[index - 1]
    limits = {"chin": .008, "mouth": .012, "nose": .007}
    delta = {}
    for landmark in original:
        target = eye_z - (photo[landmark][1] - photo["eye"][1]) * height * pixel_scale
        delta[landmark] = float(np.clip(target - original[landmark],
                                        -limits[landmark], limits[landmark]) * confidence)

    knots = np.asarray([chin_z - .055, chin_z, mouth[2], nose_z, eye_z, eye_z + .025])
    offsets = np.asarray([0., delta["chin"], delta["mouth"], delta["nose"], 0., 0.])
    if not np.all(np.diff(knots) > .001):
        raise RuntimeError("Non-monotonic facial anchors; refusing unsafe mesh deformation")
    # A smoothstep segment has a maximum slope of 1.5; bound its compression so
    # neither the lips nor nostrils can invert even for an imprecise photo anchor.
    negative_slope = float(np.min(np.diff(offsets) / np.diff(knots)))
    if negative_slope < -.35:
        offsets *= .35 / -negative_slope
    segment = np.clip(np.searchsorted(knots, z, side="right") - 1, 0, len(knots) - 2)
    fraction = np.clip((z - knots[segment]) / (knots[segment + 1] - knots[segment]), 0., 1.)
    blend = fraction * fraction * (3. - 2. * fraction)
    displacement = offsets[segment] * (1. - blend) + offsets[segment + 1] * blend

    def smooth(low, high, value):
        t = np.clip((value - low) / (high - low), 0., 1.)
        return t * t * (3. - 2. * t)

    # Feather into cheeks/jaw/neck, leaving rear cranium, ears and shoulders intact.
    mask = 1. - smooth(eye_width * 1.05, eye_width * 1.60, abs(x))
    mask *= 1. - smooth(float(mouth[1]) + .025, float(mouth[1]) + .120, y)
    result = basis.copy()
    result[:, 2] += displacement * mask
    key = keys.get(PROPORTION_KEY)
    if key is None:
        key = human.shape_key_add(name=PROPORTION_KEY, from_mix=False)
    key.relative_key = keys[0]
    key.data.foreach_set("co", result.astype(np.float32).ravel())
    key.value = 1.0
    key.slider_min = 0.
    key.slider_max = 1.
    report = {name: round(float(offsets[position]) * 1000, 2)
              for name, position in (("chin", 1), ("mouth", 2), ("nose", 3))}
    print(f"COBBLE_FACE_PROPORTIONS C{index:02d}: vertical mm {report}")
    return report


def prepare_face_targets(index, human):
    """Fit one 1-based character on an unbaked MPFB human before adding its rig.

    Call after the builder's existing target loop and BEFORE add_builtin_rig and
    add_mhclo_asset. Previous head/face target weights are reset, preventing the
    initial approximate head presets from stacking with this refinement. Body,
    neck, demographic macros, materials, UVs and topology remain untouched.
    Repeated calls reuse target keys rather than accumulating duplicate geometry.
    """
    import bpy
    from mpfb.services.targetservice import TargetService

    if isinstance(index, bool) or not isinstance(index, int) or index not in FACE_PROFILES:
        raise ValueError("Character index must be an integer from 1 through 8")
    if human is None or human.type != "MESH" or human.mode != "OBJECT":
        raise ValueError("Face fitting requires an unbaked MPFB human in Object mode")
    if any(mod.type == "ARMATURE" for mod in human.modifiers):
        raise ValueError("Apply face targets before rig and accessory fitting")
    if human.data.shape_keys is None:
        raise ValueError("Face fitting cannot operate on a baked production mesh")
    validate_profiles()

    keys = human.data.shape_keys.key_blocks
    for key in keys:
        name = TargetService.decode_shapekey_name(key.name).removeprefix("$md-")
        if name.startswith(FACE_PREFIXES):
            key.value = 0.0

    for target, weight in FACE_PROFILES[index].items():
        path = str(TARGET_ROOT / (target + ".target.gz"))
        name = TargetService.filename_to_shapekey_name(path)
        if name in keys:
            keys[name].value = weight
        else:
            TargetService.load_target(human, path, weight=weight)
    _fit_reference_proportions(index, human)
    human.active_shape_key_index = 0
    human.data.update()
    bpy.context.view_layer.update()
    print(f"COBBLE_FACE_SHAPE C{index:02d}: {len(FACE_PROFILES[index])} fitted targets")
    return dict(FACE_PROFILES[index])


def _smoke_test():
    """In-memory validation only; never saves or exports production assets."""
    import bpy
    import hashlib
    import json
    import sys
    import numpy as np

    tools = WORK / "cobble-tools"
    sys.path.insert(0, str(WORK))
    sys.path.insert(0, str(tools / "mpfb2/src"))
    original_path = bpy.utils.extension_path_user
    bpy.utils.extension_path_user = lambda package, **kwargs: (
        str(tools / "mpfb-user") if package == "mpfb" else original_path(package, **kwargs)
    )
    import addon_utils
    addon_utils.enable("mpfb", default_set=True, persistent=False)
    from mpfb.services.humanservice import HumanService
    from mpfb.services.targetservice import TargetService

    validate_profiles()
    results = []
    for index in FACE_PROFILES:
        bpy.ops.object.select_all(action="SELECT")
        bpy.ops.object.delete(use_global=False)
        macro = TargetService.get_default_macro_info_dict()
        macro.update(gender=1., age=.5, muscle=.5, weight=.5, height=.5)
        # Keep the existing builder baseline; no demographic fitting from photos.
        macro["race"] = {"caucasian": 1., "asian": 0., "african": 0.}
        human = HumanService.create_human(macro_detail_dict=macro)
        neck = TargetService.load_target(
            human, str(TARGET_ROOT / "neck/neck-scale-vert-incr.target.gz"), weight=.16)
        TargetService.load_target(
            human, str(TARGET_ROOT / "head/head-oval.target.gz"), weight=.99)
        prepare_face_targets(index, human)
        count = len(human.data.shape_keys.key_blocks)

        def snapshot():
            evaluated = human.evaluated_get(bpy.context.evaluated_depsgraph_get())
            mesh = evaluated.to_mesh()
            coordinates = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
            mesh.vertices.foreach_get("co", coordinates)
            evaluated.to_mesh_clear()
            return coordinates

        first = snapshot()
        prepare_face_targets(index, human)
        second = snapshot()
        assert len(human.data.shape_keys.key_blocks) == count, "Repeated fit duplicated shape keys"
        assert np.array_equal(first, second), "Repeated fit changed geometry"
        assert abs(neck.value - .16) < 1e-6, "Body/neck target was changed"
        assert np.isfinite(first).all(), "Nonfinite mesh coordinates"
        assert abs(TargetService.get_target_value(human, "head-oval") -
                   FACE_PROFILES[index].get("head/head-oval", 0.)) < 1e-6
        rig = HumanService.add_builtin_rig(human, "game_engine")
        assert rig.data.bones.get("head") is not None, "Refined mesh failed rig fitting"
        results.append({"character": index, "targets": len(FACE_PROFILES[index]),
                        "geometry": hashlib.sha256(first.tobytes()).hexdigest()[:16]})
    assert len({item["geometry"] for item in results}) == 8, "Characters have identical geometry"
    print("COBBLE_FACE_SHAPE_PASS " + json.dumps(results))


if __name__ == "__main__":
    _smoke_test()

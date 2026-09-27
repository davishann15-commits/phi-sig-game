"""Idempotently restore the campaign's Mouse XY look mapping.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>. The asset is
saved through Unreal, rather than editing binary .uasset data directly.
"""

import unreal


context_path = "/Game/Input/IMC_Default"
action_path = "/Game/Input/Actions/IA_MouseLook"
context = unreal.EditorAssetLibrary.load_asset(context_path)
action = unreal.EditorAssetLibrary.load_asset(action_path)
if context is None or action is None:
    raise RuntimeError(f"Missing mouse input assets: {context_path}, {action_path}")

key = unreal.Key()
key.set_editor_property("key_name", "Mouse2D")
def matches():
    return [mapping for mapping in context.get_editor_property("default_key_mappings").get_editor_property("mappings")
            if mapping.get_editor_property("action") == action
            and str(mapping.get_editor_property("key").get_editor_property("key_name")) == "Mouse2D"]

count = len(matches())
if count != 1:
    context.modify()
    if count == 0:
        context.map_key(action, key)
    else:
        for _ in range(count - 1):
            context.unmap_key(action, key)
    if not unreal.EditorAssetLibrary.save_loaded_asset(context, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {context_path}")
if len(matches()) != 1:
    raise RuntimeError("Mouse mapping must occur exactly once")
unreal.log("SSO_MOUSE_MAPPING_PASS IA_MouseLook -> Mouse2D (exactly one mapping)")

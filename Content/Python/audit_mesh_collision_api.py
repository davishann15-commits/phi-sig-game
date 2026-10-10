"""Inspect collision editing APIs available in the Python commandlet."""

import unreal


mesh = unreal.load_asset("/Game/SeniorSendOff/House/Interactivity/HI_SM_GR3_NearSeating_004")
editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
unreal.log("PILLOW_API subsystem=" + repr(editor))
for label, thing in (("EditorStaticMeshLibrary", getattr(unreal, "EditorStaticMeshLibrary", None)),
                     ("StaticMesh", mesh),
                     ("BodySetup", mesh.get_editor_property("body_setup"))):
    unreal.log("PILLOW_API " + label + " collision=" + repr([
        name for name in dir(thing)
        if any(word in name.lower() for word in ("collision", "agg_geom", "convex"))]))
setup = mesh.get_editor_property("body_setup")
aggregate = setup.get_editor_property("agg_geom")
unreal.log("PILLOW_API Aggregate=" + repr([
    name for name in dir(aggregate) if any(word in name.lower() for word in ("convex", "box", "sphere"))]))
unreal.log("PILLOW_API Hulls=" + str(len(aggregate.get_editor_property("convex_elems"))))

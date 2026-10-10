import pathlib,unreal
B=pathlib.Path(unreal.Paths.project_dir())/"SourceArt/NoodleWing"
exec((B/"install.py").read_text().split("def run():")[0],globals())
unreal.SystemLibrary.execute_console_command(None,"Interchange.FeatureFlags.Import.FBX 0")
art=json.loads((B/"art/manifest.json").read_text());tex={}
for key in ["paint","concrete","wood","brick","metal","fabric","leather"]:
 tex[key]={}
 for suffix in ["Detail","Normal","Roughness"]:
  t=imp(B/"art/textures"/("T_NW_"+key+"_"+suffix+".png"),"T_NW_"+key+"_"+suffix);t.set_editor_property("srgb",False);t.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_NORMALMAP if suffix=="Normal" else unreal.TextureCompressionSettings.TC_MASKS);A.save_loaded_asset(t,False);tex[key][suffix]=t
mats={n:material(n,r,tex)for n,r in art["materials"].items()}
for r in art["assets"]:
 me=imp(r["fbx"],r["name"]);slots=list(me.static_materials)
 for sl in slots:
  key=re.sub(r"[._]\d+$","",str(sl.get_editor_property("imported_material_slot_name")));sl.material_interface=A.load_asset(r['material_overrides'][key]) if key in r.get('material_overrides',{}) else mats[key]
 me.static_materials=slots;setupcol(me,r["kind"]);A.save_loaded_asset(me,False)
unreal.log("NOODLE_IMPORT_ART_PASS")

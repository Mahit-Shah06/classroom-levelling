# Run inside Unreal (-ExecutePythonScript): exports the mannequin meshes to Assets/Characters/rig/ for tools/rig_char.py.
import unreal
for n in ('Manny','Quinn'):
    t=unreal.AssetExportTask()
    t.object=unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_%s_Simple'%n)
    t.filename=__import__('os').path.join(__import__('os').path.dirname(__file__),'..','Assets','Characters','rig',n+'.fbx')
    t.automated=True; t.replace_identical=True; t.prompt=False
    o=unreal.FbxExportOption(); o.ascii=False; o.level_of_detail=False; o.collision=False
    t.options=o; t.exporter=unreal.SkeletalMeshExporterFBX()
    unreal.log('[export] %s %s'%(n, unreal.Exporter.run_asset_export_task(t)))
unreal.SystemLibrary.quit_editor()

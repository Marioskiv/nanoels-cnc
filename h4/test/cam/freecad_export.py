# freecad_export.py - real CAM output from FreeCAD for the H4 tests (test/cam/README.md).
# https://github.com/Marioskiv/nanoels-cnc - Copyright (c) 2026 Marioskiv - SPDX-License-Identifier: GPL-3.0-or-later
# Real CAM output from FreeCAD: one plate with the usual operations, written by FreeCAD's own
# posts (grbl, grbl_legacy, linuxcnc, linuxcnc_legacy) with their default settings and a few
# common options. Run: freecadcmd export.py  (OUT env = output folder)
import os, sys, traceback
import FreeCAD as App
OUT = os.environ["OUT"]
os.makedirs(OUT, exist_ok=True)
log = open(os.path.join(OUT, "log.txt"), "w")
try:
    import Part
    from Path.Main import Job as PathJob
    from Path.Post.Processor import PostProcessorFactory
    V = App.Vector
    doc = App.newDocument("CamTest")
    # plate 100 x 80 x 12 at the origin's corner, with a pocket, two holes and a slot
    shape = Part.makeBox(100, 80, 12)
    shape = shape.cut(Part.makeBox(30, 20, 5, V(15, 15, 7)))
    shape = shape.cut(Part.makeCylinder(4, 12, V(70, 25, 0)))
    shape = shape.cut(Part.makeCylinder(3, 12, V(70, 55, 0)))
    shape = shape.cut(Part.makeBox(40, 6, 3, V(20, 55, 9)))
    shape.translate(V(-50, -40, -12))   # centred on the chuck axis (X0 Y0), Z0 = top face: polar setup
    part = doc.addObject("Part::Feature", "Plate")
    part.Shape = shape
    doc.recompute()
    job = PathJob.Create("Job", [part], None)
    tc = job.Tools.Group[0]
    tc.HorizFeed, tc.VertFeed, tc.SpindleSpeed = "800 mm/min", "250 mm/min", 12000
    doc.recompute()
    model = job.Model.Group[0]
    def face(test):
        for k, f in enumerate(model.Shape.Faces, 1):
            if test(f): return "Face%d" % k
    floor = face(lambda f: abs(f.CenterOfMass.z + 5) < 1e-6 and f.Area < 700)
    slot = face(lambda f: abs(f.CenterOfMass.z + 3) < 1e-6 and f.Area < 300)
    h8 = face(lambda f: f.Surface.__class__.__name__ == "Cylinder" and abs(f.Surface.Radius - 4) < 1e-6)
    h6 = face(lambda f: f.Surface.__class__.__name__ == "Cylinder" and abs(f.Surface.Radius - 3) < 1e-6)
    log.write("faces %s %s %s %s\n" % (floor, slot, h8, h6))
    made = []
    def make(label, module, base, **props):
        mod = __import__(module, fromlist=["Create"])
        op = mod.Create(label, parentJob=job) if "parentJob" in mod.Create.__code__.co_varnames else mod.Create(label)
        if base is not None: op.Base = base
        for k, v in props.items(): setattr(op, k, v)
        doc.recompute()
        log.write("%s: %d commands\n" % (label, len(op.Path.Commands)))
        made.append(op)
        return op
    for label, module, base, props in [
        ("Facing", "Path.Op.MillFacing", None, {}),
        ("PocketOffset", "Path.Op.PocketShape", [(model, (floor,))], {"ClearingPattern": "Offset"}),
        ("PocketZigZag", "Path.Op.PocketShape", [(model, (slot,))], {"ClearingPattern": "ZigZag"}),
        ("Adaptive", "Path.Op.Adaptive", [(model, (floor,))], {}),
        ("Helix", "Path.Op.Helix", [(model, (h8,))], {}),
        ("Drilling", "Path.Op.Drilling", [(model, (h8,)), (model, (h6,))], {}),
        ("Profile", "Path.Op.Profile", [(model, ())], {}),
    ]:
        try: make(label, module, base, **props)
        except Exception: log.write("%s FAILED\n%s" % (label, traceback.format_exc()))
    doc.recompute()
    for post, args, name in [
        ("grbl", "", "freecad_grbl"),
        ("grbl", "--inches", "freecad_grbl_inch"),
        ("grbl", "--no-translate_drill", "freecad_grbl_drillcycles"),
        ("grbl_legacy", "", "freecad_grbl_legacy"),
        ("linuxcnc", "", "freecad_linuxcnc"),
        ("linuxcnc_legacy", "", "freecad_linuxcnc_legacy"),
    ]:
        try:
            job.PostProcessorArgs = (args + " --no-show-editor").strip()
            res = PostProcessorFactory.get_post_processor(job, post).export()
            text = "".join(g for _sec, g in res) if isinstance(res, list) else res
            open(os.path.join(OUT, name + ".nc"), "w").write(text)
            log.write("%s: %d lines (args '%s')\n" % (name, text.count("\n"), args))
        except Exception:
            log.write("%s FAILED\n%s" % (name, traceback.format_exc()))
except Exception:
    log.write(traceback.format_exc())
log.close()
os._exit(0)

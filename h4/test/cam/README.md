# Real CAM programs for the H4 tests

Written by the CAM programs themselves, not by hand. `run_tests.sh` checks every one of them (`cam_check.py`).

| File | Made by | Settings |
|---|---|---|
| `freecad_grbl.nc.gz` | FreeCAD 26.3 (libs 26.3.0dev 2026-09-16), post `grbl` | defaults |
| `freecad_grbl_inch.nc.gz` | same, post `grbl` | `--inches` |
| `freecad_grbl_legacy.nc.gz` | same, post `grbl_legacy` | defaults |
| `freecad_linuxcnc.nc.gz` | same, post `linuxcnc` | defaults |
| `freecad_linuxcnc_legacy.nc.gz` | same, post `linuxcnc_legacy` | defaults |
| `*_polar.ngc.gz` | the files above through the [polar converter](https://github.com/Marioskiv/polar-cnc-converter) 2.26.1 | X 0 ... 113 mm (one side of the centre) |

The part (`freecad_export.py`, run with `freecadcmd`): a plate 100 x 80 x 12 mm centred on the chuck axis with a
pocket, a slot, two holes; operations Facing, Pocket (offset), Pocket (zig-zag), Adaptive, Helix, Drilling (G81),
Profile; tool 5 mm, 800 / 250 mm/min, 12000 rpm.

What is checked:
- the CAM files as written (X Y Z milling): the H4 reads every line up to the first move in Y and stops there with
  "this machine has no Y axis" (it has X, Z and C); nothing before it is refused;
- the converted X Z C programs: every line read; simulated step by step (path, speeds, accelerations, step
  buffer); corners within their bound; the self-test; and, with `RS274=`, the same positions as LinuxCNC.

Programs from Fusion 360 and other CAM programs go here the same way when they are available.

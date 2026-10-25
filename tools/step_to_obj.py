#!/usr/bin/env python3
"""Convert a STEP assembly into an OBJ mesh that Blender can import.

Writes one OBJ group (`o` line) per part instance, named after the SolidWorks
part, in metres and in the raw STEP axes (Y up). Blender's OBJ importer
converts Y-up to Z-up by default, so the result lands at real-world scale,
standing the right way up.

Blender cannot read STEP itself (nor the proprietary .SLDPRT/.SLDASM files),
so this script is the bridge. It needs the OpenCASCADE Python bindings (OCP),
installed outside the repository:

    uv venv --python 3.12 ~/.venvs/mecanum-cad
    VIRTUAL_ENV=~/.venvs/mecanum-cad uv pip install cadquery-ocp

Usage:
    ~/.venvs/mecanum-cad/bin/python tools/step_to_obj.py \
        "3D/Vehicle Assembled (1).STEP" /tmp/mecanum_cad/vehicle.obj 0.3

The trailing number is the linear deflection in millimetres: smaller is finer.
"""

import argparse
import os
import sys
import time

from OCP.BRep import BRep_Tool
from OCP.BRepMesh import BRepMesh_IncrementalMesh
from OCP.IFSelect import IFSelect_RetDone
from OCP.OCP.collections import Sequence_TDF_Label
from OCP.STEPCAFControl import STEPCAFControl_Reader
from OCP.TCollection import TCollection_ExtendedString
from OCP.TDataStd import TDataStd_Name
from OCP.TDF import TDF_Label
from OCP.TDocStd import TDocStd_Document
from OCP.TopAbs import TopAbs_FACE, TopAbs_REVERSED
from OCP.TopExp import TopExp_Explorer
from OCP.TopLoc import TopLoc_Location
from OCP.TopoDS import TopoDS
from OCP.XCAFApp import XCAFApp_Application
from OCP.XCAFDoc import XCAFDoc_DocumentTool

SCALE = 0.001  # the STEP is in millimetres; write metres
ANGLE_DEFLECTION = 0.3  # radians


def name_of(label):
    attr = TDataStd_Name()
    if label.FindAttribute(TDataStd_Name.GetID_s(), attr):
        return attr.Get().ToExtString()
    return ""


def ascii_prefix(text):
    """Keep the leading ASCII part of a name.

    SolidWorks configuration names here are Chinese and arrive as mojibake
    (e.g. 'Roller_<garbage>'), so cut the string at the first non-ASCII
    character and trim the separator: 'Roller_<garbage>' -> 'Roller'.
    """
    kept = []
    for ch in text:
        if ord(ch) > 126 or ch in "\\/":
            break
        kept.append(ch)
    return "".join(kept).strip().rstrip("_").strip()


def part_name(label, fallback):
    return ascii_prefix(name_of(label)) or ascii_prefix(fallback) or "part"


def collect_instances(shape_tool):
    """Walk the XCAF assembly tree and return [(name, placed_shape)]."""
    instances = []
    roots = Sequence_TDF_Label()
    shape_tool.GetFreeShapes(roots)

    def walk(label, parent_loc, tag):
        if shape_tool.IsAssembly_s(label):
            components = Sequence_TDF_Label()
            shape_tool.GetComponents_s(label, components, False)
            for i in range(1, components.Length() + 1):
                component = components.Value(i)
                location = shape_tool.GetLocation_s(component)
                placed = parent_loc.Multiplied(location)
                referred = TDF_Label()
                if shape_tool.GetReferredShape_s(component, referred):
                    walk(referred, placed, part_name(component, tag))
                else:
                    shape = shape_tool.GetShape_s(component)
                    instances.append((part_name(component, tag), shape.Moved(placed)))
        else:
            shape = shape_tool.GetShape_s(label)
            instances.append((part_name(label, tag), shape.Moved(parent_loc)))

    for i in range(1, roots.Length() + 1):
        walk(roots.Value(i), TopLoc_Location(), "root")
    return instances


def faces_of(shape):
    """Yield (points, triangles, reversed) for every triangulated face."""
    explorer = TopExp_Explorer(shape, TopAbs_FACE)
    while explorer.More():
        face = TopoDS.Face(explorer.Current())
        reversed_face = face.Orientation() == TopAbs_REVERSED
        location = TopLoc_Location()
        triangulation = BRep_Tool.Triangulation_s(face, location)
        if triangulation is not None:
            try:
                triangulation.LoadDeferredData()
            except Exception:
                pass
            transform = location.Transformation()
            points = [
                triangulation.Node(i).Transformed(transform)
                for i in range(1, triangulation.NbNodes() + 1)
            ]
            triangles = [
                triangulation.Triangle(i).Get()
                for i in range(1, triangulation.NbTriangles() + 1)
            ]
            yield points, triangles, reversed_face
        explorer.Next()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("step", help="input STEP file")
    parser.add_argument("out", help="output OBJ file")
    parser.add_argument(
        "deflection",
        nargs="?",
        type=float,
        default=0.3,
        help="linear deflection in mm, smaller is finer (default 0.3)",
    )
    args = parser.parse_args()

    app = XCAFApp_Application.GetApplication_s()
    document = TDocStd_Document(TCollection_ExtendedString("MDTV-XCAF"))
    app.NewDocument(TCollection_ExtendedString("MDTV-XCAF"), document)

    reader = STEPCAFControl_Reader()
    reader.SetNameMode(True)
    reader.SetColorMode(True)
    if reader.ReadFile(args.step) != IFSelect_RetDone:
        sys.exit(f"could not read {args.step}")
    if not reader.Transfer(document):
        sys.exit(f"could not transfer {args.step}")

    shape_tool = XCAFDoc_DocumentTool.ShapeTool_s(document.Main())
    instances = collect_instances(shape_tool)
    print(f"instances: {len(instances)}")
    if not instances:
        sys.exit("no part instances found")

    started = time.time()
    for _, shape in instances:
        BRepMesh_IncrementalMesh(shape, args.deflection, False, ANGLE_DEFLECTION, True)
    print(f"meshed in {time.time() - started:.1f}s")

    vertex_count = 0
    triangle_count = 0
    low = [float("inf")] * 3
    high = [float("-inf")] * 3
    seen = {}
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w") as obj:
        obj.write("# Converted from STEP by tools/step_to_obj.py (metres, STEP axes)\n")
        for name, shape in instances:
            safe = name.replace(" ", "_")
            seen[safe] = seen.get(safe, 0) + 1
            obj.write(f"o {safe}_{seen[safe]:03d}\n")
            for points, triangles, reversed_face in faces_of(shape):
                for point in points:
                    x, y, z = point.X() * SCALE, point.Y() * SCALE, point.Z() * SCALE
                    for axis, value in enumerate((x, y, z)):
                        low[axis] = min(low[axis], value)
                        high[axis] = max(high[axis], value)
                    obj.write(f"v {x:.6f} {y:.6f} {z:.6f}\n")
                for a, b, c in triangles:
                    if reversed_face:
                        a, b, c = a, c, b
                    obj.write(f"f {vertex_count + a} {vertex_count + b} {vertex_count + c}\n")
                    triangle_count += 1
                vertex_count += len(points)

    print(f"objects: {len(instances)} | unique names: {len(seen)}")
    print(f"wrote {args.out}: {vertex_count:,} vertices, {triangle_count:,} triangles")
    print(
        "size (m): "
        f"{high[0] - low[0]:.3f} x {high[1] - low[1]:.3f} x {high[2] - low[2]:.3f}"
        "  (X x Y x Z in STEP axes, Y is up)"
    )


if __name__ == "__main__":
    main()

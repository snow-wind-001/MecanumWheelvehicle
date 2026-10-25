# 机械文件与转换工具

最终发布保留 SolidWorks 零件/装配、STEP、实物照片，以及当前提供的 `3D/Vehicle Layout v15.blend`。旧 Blender 场景、布局候选脚本和过程文件只在本地保留。当前场景作为布局资料交付，本轮未重新验证机械干涉或修改其几何。

`step_to_obj.py` 使用 OpenCASCADE 将 STEP 网格化为 OBJ，供其他 CAD/可视化工具使用：

```bash
uv venv --python 3.12 ~/.venvs/mecanum-cad
VIRTUAL_ENV=~/.venvs/mecanum-cad uv pip install cadquery-ocp
~/.venvs/mecanum-cad/bin/python tools/step_to_obj.py "3D/Vehicle Assembled (1).STEP" /tmp/vehicle.obj 0.3
```

参数依次为输入 STEP、输出 OBJ、网格精度。它不重建 v15 的板卡摆放与机械约束；最终场景不能只靠这个转换器重现。不要改动装配引用的文件名；机械设计变更仍需 SolidWorks 装配重建与干涉验证。

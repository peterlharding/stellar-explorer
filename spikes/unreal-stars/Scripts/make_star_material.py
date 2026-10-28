"""Generates /Game/Generated/M_Star from the HLSL in ../Shaders.

The material is a build product: this script and the shader text are what is
committed. Run it with the editor's command line:

    UnrealEditor-Cmd StellarSpike.uproject -run=pythonscript -script=Scripts/make_star_material.py
"""

import re
from pathlib import Path

import unreal

FOLDER = "/Game/Generated"
NAME = "M_Star"
SHADERS = Path(__file__).resolve().parent.parent / "Shaders"
# LimitingMag is the faintest star meant to be visible, so Exposure puts it just
# above black; a magnitude 0 star is then 400 times brighter and blooms.
PARAMETERS = {"UnitsPerParsec": 100.0, "LimitingMag": 6.5, "BaseSize": 0.0012, "Exposure": 0.04}

library = unreal.MaterialEditingLibrary


def shader(name: str) -> str:
    """The shader text, with `#include "X"` replaced by X.hlsl: Custom nodes are one function body."""
    text = (SHADERS / f"{name}.hlsl").read_text()
    return re.sub(r'#include "(\w+)"', lambda m: shader(m.group(1)), text)


def node(material, cls, x, y, **properties):
    expression = library.create_material_expression(material, cls, x, y)
    for key, value in properties.items():
        expression.set_editor_property(key, value)
    return expression


def connect(source, target, input_name, output_name=""):
    if not library.connect_material_expressions(source, output_name, target, input_name):
        raise RuntimeError(f"cannot connect {source.get_name()} to {target.get_name()}.{input_name}")


def custom_input(name):
    custom_input = unreal.CustomInput()
    custom_input.set_editor_property("input_name", name)
    return custom_input


def custom(material, name, x, y, inputs):
    expression = node(
        material,
        unreal.MaterialExpressionCustom,
        x,
        y,
        code=shader(name),
        description=name,
        output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
        inputs=[custom_input(input_name) for input_name in inputs],
    )
    for input_name, source in inputs.items():
        connect(source, expression, input_name)
    return expression


def build():
    path = f"{FOLDER}/{NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    corner = node(material, unreal.MaterialExpressionTextureCoordinate, -1200, -200, coordinate_index=0)
    magnitude_uv = node(material, unreal.MaterialExpressionTextureCoordinate, -1200, 0, coordinate_index=1)
    absmag = node(material, unreal.MaterialExpressionComponentMask, -1000, 0, r=True, g=False, b=False, a=False)
    connect(magnitude_uv, absmag, "")
    colour = node(material, unreal.MaterialExpressionVertexColor, -1200, 200)

    # Camera to star, subtracted in double precision before it reaches the Custom nodes.
    world = node(material, unreal.MaterialExpressionWorldPosition, -1200, 400)
    camera = node(material, unreal.MaterialExpressionCameraPositionWS, -1200, 500)
    to_star = node(material, unreal.MaterialExpressionSubtract, -1000, 450)
    connect(world, to_star, "A")
    connect(camera, to_star, "B")

    # The camera's right and up directions in world space.
    axes = {}
    for index, (axis, vector) in enumerate((("Right", (1.0, 0.0, 0.0)), ("Up", (0.0, 1.0, 0.0)))):
        constant = node(material, unreal.MaterialExpressionConstant3Vector, -1200, 650 + 150 * index,
                        constant=unreal.LinearColor(*vector, 0.0))  # fmt: skip
        transform = node(
            material,
            unreal.MaterialExpressionTransform,
            -1000,
            650 + 150 * index,
            transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_VIEW,
            transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD,
        )
        connect(constant, transform, "")
        axes[axis] = transform

    parameters = {
        name: node(material, unreal.MaterialExpressionScalarParameter, -1200, 1000 + 100 * index,
                   parameter_name=name, default_value=value)  # fmt: skip
        for index, (name, value) in enumerate(PARAMETERS.items())
    }
    common = {"ToStar": to_star, "AbsMag": absmag, "UnitsPerParsec": parameters["UnitsPerParsec"],
              "LimitingMag": parameters["LimitingMag"], "BaseSize": parameters["BaseSize"]}  # fmt: skip

    offset = custom(material, "StarOffset", -600, 600, {"Corner": corner, **axes, **common})
    emissive = custom(material, "StarColour", -600, 0,
                      {"Corner": corner, "Colour": colour, "Exposure": parameters["Exposure"], **common})  # fmt: skip
    library.connect_material_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    library.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"could not save {path}")
    unreal.log(f"Generated {path}")


build()

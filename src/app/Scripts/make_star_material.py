"""Generates /Game/Generated/M_Star from the HLSL in ../Shaders, and M_Pointer and M_Label.

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
    """The shader text, `#include "X"` replaced by X.hlsl: a Custom node is one function body."""
    text = (SHADERS / f"{name}.hlsl").read_text()
    return re.sub(r'#include "(\w+)"', lambda m: shader(m.group(1)), text)


def node(material, cls, x, y, **properties):
    expression = library.create_material_expression(material, cls, x, y)
    for key, value in properties.items():
        expression.set_editor_property(key, value)
    return expression


def connect(source, target, input_name, output_name=""):
    if not library.connect_material_expressions(source, output_name, target, input_name):
        target_input = f"{target.get_name()}.{input_name}"
        raise RuntimeError(f"cannot connect {source.get_name()} to {target_input}")


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


def new_material(name):
    path = f"{FOLDER}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    return tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def save(material):
    library.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"could not save {material.get_path_name()}")
    unreal.log(f"Generated {material.get_path_name()}")


def build():
    material = new_material(NAME)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    corner = node(
        material, unreal.MaterialExpressionTextureCoordinate, -1200, -200, coordinate_index=0
    )
    magnitude_uv = node(
        material, unreal.MaterialExpressionTextureCoordinate, -1200, 0, coordinate_index=1
    )
    absmag = node(
        material,
        unreal.MaterialExpressionComponentMask,
        -1000,
        0,
        r=True,
        g=False,
        b=False,
        a=False,
    )
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
        constant = node(
            material,
            unreal.MaterialExpressionConstant3Vector,
            -1200,
            650 + 150 * index,
            constant=unreal.LinearColor(*vector, 0.0),
        )
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
        name: node(
            material,
            unreal.MaterialExpressionScalarParameter,
            -1200,
            1000 + 100 * index,
            parameter_name=name,
            default_value=value,
        )
        for index, (name, value) in enumerate(PARAMETERS.items())
    }
    common = {
        "ToStar": to_star,
        "AbsMag": absmag,
        "UnitsPerParsec": parameters["UnitsPerParsec"],
        "LimitingMag": parameters["LimitingMag"],
        "BaseSize": parameters["BaseSize"],
    }

    offset = custom(material, "StarOffset", -600, 600, {"Corner": corner, **axes, **common})
    emissive = custom(
        material,
        "StarColour",
        -600,
        0,
        {"Corner": corner, "Colour": colour, "Exposure": parameters["Exposure"], **common},
    )
    library.connect_material_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    save(material)


def build_pointer():
    """The pointer's dots and rings: one flat colour, set per use by the Colour parameter.

    Opaque, so the headset's compositor shows it (see SpawnSkySphere in StellarSpikeGameMode.h).
    """
    material = new_material("M_Pointer")
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    # The dots are instanced meshes. The editor adds this usage when it first meets one, but a
    # cooked build cannot, and silently draws them with the default lit material: black here.
    material.set_editor_property("used_with_instanced_static_meshes", True)
    colour = node(
        material,
        unreal.MaterialExpressionVectorParameter,
        -400,
        0,
        parameter_name="Colour",
        default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0),
    )
    library.connect_material_property(colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    save(material)


def build_label():
    """Text for the pointer's label: the engine's own text material, made unlit.

    The engine's is lit, and nothing lights this scene, so its text is black. The copy
    keeps the engine's handling of the font and shows the text's colour as emitted light.
    """
    path = f"{FOLDER}/M_Label"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    engine_text = "/Engine/EngineMaterials/DefaultTextMaterialOpaque"
    material = unreal.EditorAssetLibrary.duplicate_asset(engine_text, path)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    base_colour = unreal.MaterialProperty.MP_BASE_COLOR
    colour = library.get_material_property_input_node(material, base_colour)
    library.connect_material_property(colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    save(material)


build()
build_pointer()
build_label()

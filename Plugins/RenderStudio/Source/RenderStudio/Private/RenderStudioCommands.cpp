#include "RenderStudioCommands.h"

#include "RenderStudioStyle.h"

#define LOCTEXT_NAMESPACE "RenderStudioCommands"

FRenderStudioCommands::FRenderStudioCommands()
	: TCommands<FRenderStudioCommands>(
		TEXT("RenderStudio"),
		NSLOCTEXT("Contexts", "RenderStudio", "Render Studio"),
		NAME_None,
		FRenderStudioStyle::GetStyleSetName())
{
}

void FRenderStudioCommands::RegisterCommands()
{
	UI_COMMAND(NewScene, "New Scene", "Clear the scene and start over with the default setup.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::N));
	UI_COMMAND(ImportMesh, "Import Mesh...", "Import an FBX or OBJ mesh into the scene.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::I));
	UI_COMMAND(SaveAll, "Save All", "Save every modified material, instance and imported mesh.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::S));
	UI_COMMAND(SaveScreenshot, "Save Screenshot...", "Render the viewport to a PNG image.", EUserInterfaceActionType::Button, FInputChord(EKeys::F12));
	UI_COMMAND(Exit, "Exit", "Close Render Studio.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(DuplicateSelected, "Duplicate", "Duplicate the selected object.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::D));
	UI_COMMAND(DeleteSelected, "Delete", "Delete the selected object.", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));
	UI_COMMAND(FocusSelected, "Frame Selected", "Move the camera to frame the selected object.", EUserInterfaceActionType::Button, FInputChord(EKeys::F));

	UI_COMMAND(AddCube, "Cube", "Add a cube to the scene.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddSphere, "Sphere", "Add a sphere to the scene.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddCylinder, "Cylinder", "Add a cylinder to the scene.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddCone, "Cone", "Add a cone to the scene.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddPlane, "Plane", "Add a plane to the scene.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddPointLight, "Point Light", "Add a point light.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddSpotLight, "Spot Light", "Add a spot light.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddRectLight, "Area Light", "Add a rectangular area light.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(SelectMode, "Select", "Select objects without a transform gizmo.", EUserInterfaceActionType::RadioButton, FInputChord(EKeys::Q));
	UI_COMMAND(TranslateMode, "Move", "Move the selected object.", EUserInterfaceActionType::RadioButton, FInputChord(EKeys::W));
	UI_COMMAND(RotateMode, "Rotate", "Rotate the selected object.", EUserInterfaceActionType::RadioButton, FInputChord(EKeys::E));
	UI_COMMAND(ScaleMode, "Scale", "Scale the selected object.", EUserInterfaceActionType::RadioButton, FInputChord(EKeys::R));

	UI_COMMAND(ToggleGrid, "Grid", "Show or hide the ground grid.", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::G));
	UI_COMMAND(ToggleFloor, "Floor", "Show or hide the floor / shadow catcher.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleEnvironment, "Sky Backdrop", "Show or hide the HDRI backdrop (lighting is kept).", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleTurntable, "Turntable", "Slowly rotate the lighting environment around the scene.", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::T));
	UI_COMMAND(ResetCamera, "Reset Camera", "Frame the whole scene.", EUserInterfaceActionType::Button, FInputChord(EKeys::Home));
	UI_COMMAND(ResetLayout, "Reset Layout", "Restore the default panel layout.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(NewMaterial, "New Material", "Create a new PBR material (base color, metallic, roughness, normal).", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::M));
	UI_COMMAND(NewMaterialInstance, "New Material Instance", "Create an instance of the selected material for quick parameter tweaks.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(EditMaterial, "Open Material Editor", "Open the selected material in the UE4 node based material editor.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::E));
	UI_COMMAND(AssignMaterial, "Assign To Selection", "Assign the selected material to the selected object.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::A));

	UI_COMMAND(About, "About Render Studio", "Version and credits.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

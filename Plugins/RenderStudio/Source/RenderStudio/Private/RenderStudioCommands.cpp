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
	UI_COMMAND(OpenScene, "Open Scene...", "Open a Render Studio scene (.rss).", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::O));
	UI_COMMAND(SaveScene, "Save Scene", "Save the scene, plus every modified material and imported asset.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::S));
	UI_COMMAND(SaveSceneAs, "Save Scene As...", "Save the scene under a new name.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::S));
	UI_COMMAND(ImportMesh, "Import Mesh...", "Import FBX or OBJ meshes into the scene.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::I));
	UI_COMMAND(ImportEnvironment, "Import HDRI...", "Import an .hdr panorama and use it as the sky / image based lighting.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::I));
	UI_COMMAND(RenderImage, "Render...", "Render a final image or a turntable sequence at any resolution.", EUserInterfaceActionType::Button, FInputChord(EKeys::F9));
	UI_COMMAND(SaveScreenshot, "Save Screenshot...", "Save the viewport as it looks right now.", EUserInterfaceActionType::Button, FInputChord(EKeys::F12));
	UI_COMMAND(Exit, "Exit", "Close Render Studio.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(Undo, "Undo", "Undo the last change to the scene.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::Z));
	UI_COMMAND(Redo, "Redo", "Redo the last undone change.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::Y));
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

	UI_COMMAND(ViewPerspective, "Perspective", "Three-quarter view of the whole scene.", EUserInterfaceActionType::Button, FInputChord(EKeys::NumPadZero));
	UI_COMMAND(ViewFront, "Front", "Look at the scene from the front.", EUserInterfaceActionType::Button, FInputChord(EKeys::NumPadOne));
	UI_COMMAND(ViewBack, "Back", "Look at the scene from the back.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::NumPadOne));
	UI_COMMAND(ViewLeft, "Left", "Look at the scene from the left.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::NumPadThree));
	UI_COMMAND(ViewRight, "Right", "Look at the scene from the right.", EUserInterfaceActionType::Button, FInputChord(EKeys::NumPadThree));
	UI_COMMAND(ViewTop, "Top", "Look down at the scene.", EUserInterfaceActionType::Button, FInputChord(EKeys::NumPadSeven));

	// UI_COMMAND needs a plain member name, so the numbered bookmark commands are spelled out.
	UI_COMMAND(SaveBookmark[0], "Store Camera 1", "Remember the current camera in slot 1 (saved with the scene).", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::One));
	UI_COMMAND(SaveBookmark[1], "Store Camera 2", "Remember the current camera in slot 2 (saved with the scene).", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::Two));
	UI_COMMAND(SaveBookmark[2], "Store Camera 3", "Remember the current camera in slot 3 (saved with the scene).", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::Three));
	UI_COMMAND(SaveBookmark[3], "Store Camera 4", "Remember the current camera in slot 4 (saved with the scene).", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::Four));
	UI_COMMAND(RecallBookmark[0], "Camera 1", "Jump to the camera stored in slot 1.", EUserInterfaceActionType::Button, FInputChord(EKeys::One));
	UI_COMMAND(RecallBookmark[1], "Camera 2", "Jump to the camera stored in slot 2.", EUserInterfaceActionType::Button, FInputChord(EKeys::Two));
	UI_COMMAND(RecallBookmark[2], "Camera 3", "Jump to the camera stored in slot 3.", EUserInterfaceActionType::Button, FInputChord(EKeys::Three));
	UI_COMMAND(RecallBookmark[3], "Camera 4", "Jump to the camera stored in slot 4.", EUserInterfaceActionType::Button, FInputChord(EKeys::Four));

	UI_COMMAND(NewMaterial, "New Material", "Create a new material (an instance of the Render Studio PBR material) and assign it to the selection.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::M));
	UI_COMMAND(NewMaterialGraph, "New Material Graph", "Create a new base material with its own node graph, plus an instance for tweaking.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::M));
	UI_COMMAND(NewMaterialInstance, "New Material Instance", "Create an instance of the selected material for quick parameter tweaks.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(EditMaterial, "Open Material Editor", "Open the selected material in the UE4 node based material editor.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::E));
	UI_COMMAND(AssignMaterial, "Assign To Selection", "Assign the selected material to the selected object.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::A));

	UI_COMMAND(About, "About Render Studio", "Version and credits.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

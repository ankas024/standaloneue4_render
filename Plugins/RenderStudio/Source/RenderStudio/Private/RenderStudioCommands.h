#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/** Every user-facing action of the application (menus, toolbar and shortcuts all bind to these). */
class FRenderStudioCommands : public TCommands<FRenderStudioCommands>
{
public:
	FRenderStudioCommands();

	virtual void RegisterCommands() override;

	// File
	TSharedPtr<FUICommandInfo> NewScene;
	TSharedPtr<FUICommandInfo> OpenScene;
	TSharedPtr<FUICommandInfo> SaveScene;
	TSharedPtr<FUICommandInfo> SaveSceneAs;
	TSharedPtr<FUICommandInfo> ImportMesh;
	TSharedPtr<FUICommandInfo> ImportEnvironment;
	TSharedPtr<FUICommandInfo> RenderImage;
	TSharedPtr<FUICommandInfo> SaveScreenshot;
	TSharedPtr<FUICommandInfo> Exit;

	// Edit
	TSharedPtr<FUICommandInfo> Undo;
	TSharedPtr<FUICommandInfo> Redo;
	TSharedPtr<FUICommandInfo> DuplicateSelected;
	TSharedPtr<FUICommandInfo> DeleteSelected;
	TSharedPtr<FUICommandInfo> FocusSelected;

	// Create
	TSharedPtr<FUICommandInfo> AddCube;
	TSharedPtr<FUICommandInfo> AddSphere;
	TSharedPtr<FUICommandInfo> AddCylinder;
	TSharedPtr<FUICommandInfo> AddCone;
	TSharedPtr<FUICommandInfo> AddPlane;
	TSharedPtr<FUICommandInfo> AddPointLight;
	TSharedPtr<FUICommandInfo> AddSpotLight;
	TSharedPtr<FUICommandInfo> AddRectLight;

	// Transform tools
	TSharedPtr<FUICommandInfo> SelectMode;
	TSharedPtr<FUICommandInfo> TranslateMode;
	TSharedPtr<FUICommandInfo> RotateMode;
	TSharedPtr<FUICommandInfo> ScaleMode;

	// View
	TSharedPtr<FUICommandInfo> ToggleGrid;
	TSharedPtr<FUICommandInfo> ToggleFloor;
	TSharedPtr<FUICommandInfo> ToggleEnvironment;
	TSharedPtr<FUICommandInfo> ToggleTurntable;
	TSharedPtr<FUICommandInfo> ResetCamera;
	TSharedPtr<FUICommandInfo> ResetLayout;

	// Camera
	TSharedPtr<FUICommandInfo> ViewPerspective;
	TSharedPtr<FUICommandInfo> ViewFront;
	TSharedPtr<FUICommandInfo> ViewBack;
	TSharedPtr<FUICommandInfo> ViewLeft;
	TSharedPtr<FUICommandInfo> ViewRight;
	TSharedPtr<FUICommandInfo> ViewTop;
	TSharedPtr<FUICommandInfo> SaveBookmark[4];
	TSharedPtr<FUICommandInfo> RecallBookmark[4];

	// Material
	TSharedPtr<FUICommandInfo> NewMaterial;
	TSharedPtr<FUICommandInfo> NewMaterialGraph;
	TSharedPtr<FUICommandInfo> NewMaterialInstance;
	TSharedPtr<FUICommandInfo> EditMaterial;
	TSharedPtr<FUICommandInfo> AssignMaterial;

	// Help
	TSharedPtr<FUICommandInfo> About;
};

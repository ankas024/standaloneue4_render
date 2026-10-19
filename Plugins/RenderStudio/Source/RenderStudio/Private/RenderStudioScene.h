#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UObject/GCObject.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class FAdvancedPreviewScene;
class FEditPropertyChain;
class UMaterial;
class UMaterialInstanceConstant;
class UMaterialInterface;
class UPrimitiveComponent;
class URenderStudioSettings;
class UStaticMesh;
class UTextureCube;
class UWorld;
struct FPreviewSceneProfile;
struct FSlateBrush;

enum class ERenderStudioPrimitive : uint8
{
	Cube,
	Sphere,
	Cylinder,
	Cone,
	Plane,
};

enum class ERenderStudioLight : uint8
{
	Point,
	Spot,
	Rect,
};

/** A saved camera position (scene camera or one of the numbered bookmarks). */
struct FRenderStudioCamera
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float FieldOfView = 45.0f;
	bool bIsSet = false;
};

/** A ready-made look for the standard Render Studio material. */
struct FRenderStudioMaterialPreset
{
	FString Name;
	FLinearColor Albedo;
	float Metalness;
	float Roughness;
	float Specular;
};

/**
 * The document of the application: owns the preview world (HDRI sky, floor, sun and
 * post-processing come from FAdvancedPreviewScene), the user's objects, the selection,
 * the camera bookmarks, the undo history and the materials created in this session.
 *
 * Scenes are saved as JSON (.rss): every object is stored as its class, its label and the
 * editable properties of its components that differ from the class defaults, so anything
 * changed in the Properties panel round-trips without per-type code. The same snapshot
 * format drives undo / redo.
 */
class FRenderStudioScene : public TSharedFromThis<FRenderStudioScene>, public FGCObject
{
public:
	static const TCHAR* FileExtension;
	static const int32 NumBookmarks = 4;

	FRenderStudioScene();
	virtual ~FRenderStudioScene();

	/** Must be called right after construction (needs a shared ref to bind delegates). */
	void Initialize();

	TSharedRef<FAdvancedPreviewScene> GetPreviewScene() const { return PreviewScene.ToSharedRef(); }
	UWorld* GetWorld() const;
	URenderStudioSettings* GetSettings() const { return Settings; }
	FPreviewSceneProfile& GetProfile() const;

	// Document
	/** Clears everything and loads the default shader ball setup. */
	void NewScene();
	bool SaveScene(const FString& Filename, FText& OutError);
	bool LoadScene(const FString& Filename, FText& OutError);
	const FString& GetFilename() const { return Filename; }
	FText GetDisplayName() const;
	bool IsDirty() const { return bDirty; }
	void MarkDirty();

	// Undo / redo (scene objects, labels, transforms, properties and material assignments)
	/** Records the current state before a change. Nested calls are merged into the outermost one. */
	void BeginChange(const FText& Description);
	/** Closes a change. bDiscard drops the recorded step (the operation turned out to change nothing). */
	void EndChange(bool bDiscard = false);
	bool CanUndo() const { return UndoStack.Num() > 0; }
	bool CanRedo() const { return RedoStack.Num() > 0; }
	FText GetUndoDescription() const;
	FText GetRedoDescription() const;
	void Undo();
	void Redo();

	// Objects
	AActor* AddPrimitive(ERenderStudioPrimitive Primitive);
	AActor* AddStaticMesh(UStaticMesh* Mesh, const FString& Label, const FTransform& Transform = FTransform::Identity);
	AActor* AddLight(ERenderStudioLight LightType);
	/** Imports FBX / OBJ files (no options dialog) into the content folder and drops the meshes into the scene. Returns the number of meshes added. */
	int32 ImportMeshes(const TArray<FString>& Files);
	AActor* DuplicateObject(AActor* Actor);
	void DeleteObject(AActor* Actor);

	TArray<AActor*> GetObjects() const;
	bool IsSceneObject(const AActor* Actor) const;

	FString GetObjectLabel(const AActor* Actor) const;
	void SetObjectLabel(AActor* Actor, const FString& NewLabel);
	bool IsObjectVisible(const AActor* Actor) const;
	void SetObjectVisible(AActor* Actor, bool bVisible);
	const FSlateBrush* GetObjectIcon(const AActor* Actor) const;

	/** Bounds of the selection, or of every object if nothing is selected / bSelectionOnly is false. */
	FBox GetBounds(bool bSelectionOnly) const;

	// Selection
	AActor* GetSelectedObject() const { return SelectedObject.Get(); }
	void SetSelectedObject(AActor* Actor);

	// Cameras
	const FRenderStudioCamera& GetSceneCamera() const { return SceneCamera; }
	void SetSceneCamera(const FRenderStudioCamera& Camera) { SceneCamera = Camera; }
	const FRenderStudioCamera& GetBookmark(int32 Index) const;
	void SetBookmark(int32 Index, const FRenderStudioCamera& Camera);

	// Environment shortcuts (stored in the asset viewer profile so they persist)
	bool IsFloorVisible() const;
	void SetFloorVisible(bool bVisible);
	bool IsEnvironmentVisible() const;
	void SetEnvironmentVisible(bool bVisible);
	bool IsTurntableEnabled() const;
	void SetTurntableEnabled(bool bEnabled);
	/** Sky cubemaps that ship with the engine plus every cubemap in the content folder. */
	TArray<UTextureCube*> GetEnvironmentMaps() const;
	UTextureCube* GetEnvironmentMap() const;
	void SetEnvironmentMap(UTextureCube* Cubemap);
	/** Imports .hdr panoramas as cubemaps and makes the first one the sky. Returns the number imported. */
	int32 ImportEnvironmentMaps(const TArray<FString>& Files);

	// Materials
	/** The shared Render Studio master material (created in the content folder on first use). */
	UMaterial* GetOrCreateMasterMaterial();
	/** Creates a parameterised PBR material asset (Albedo/Metalness/Specular/Roughness/Normal/Emissive) in the content folder. */
	UMaterial* CreateMaterial(const FString& BaseName = TEXT("M_Material"));
	UMaterialInstanceConstant* CreateMaterialInstance(UMaterialInterface* Parent, const FString& BaseName = FString());
	/** Creates an instance of the master material set up with a preset look. */
	UMaterialInstanceConstant* CreateMaterialFromPreset(const FRenderStudioMaterialPreset& Preset);
	/** Applies a preset to an existing instance (only the parameters the material actually has). */
	void ApplyPreset(UMaterialInstanceConstant* Instance, const FRenderStudioMaterialPreset& Preset);
	static const TArray<FRenderStudioMaterialPreset>& GetMaterialPresets();
	void AssignMaterial(AActor* Actor, UMaterialInterface* Material, int32 SlotIndex = INDEX_NONE);
	/** Every material asset in the content folder plus the materials used by scene objects. */
	TArray<UMaterialInterface*> GetMaterials() const;
	UMaterialInterface* GetSelectedMaterial() const { return SelectedMaterial.Get(); }
	void SetSelectedMaterial(UMaterialInterface* Material);
	void OpenMaterialEditor(UMaterialInterface* Material) const;

	// Events
	FSimpleMulticastDelegate& OnObjectsChanged() { return ObjectsChangedEvent; }
	FSimpleMulticastDelegate& OnSelectionChanged() { return SelectionChangedEvent; }
	FSimpleMulticastDelegate& OnMaterialsChanged() { return MaterialsChangedEvent; }
	/** File name or dirty state changed (window title). */
	FSimpleMulticastDelegate& OnDocumentChanged() { return DocumentChangedEvent; }
	/** A scene was loaded or reset: the viewport should apply the scene camera. */
	FSimpleMulticastDelegate& OnSceneLoaded() { return SceneLoadedEvent; }

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FRenderStudioScene"); }

private:
	struct FUndoState
	{
		FText Description;
		TSharedPtr<FJsonObject> State;
	};

	/** Removes every user object (environment, floor and sun are kept). */
	void ClearObjects();
	void SpawnDefaultObjects();

	AActor* FinishSpawn(AActor* Actor, const FString& Label);
	void RegisterObject(AActor* Actor);
	bool IsComponentSelected(const UPrimitiveComponent* Component) const;
	void RefreshSelectionOutline(AActor* Actor) const;
	FString MakeUniqueLabel(const FString& BaseLabel) const;
	FVector GetDropLocation() const;

	// Serialization (RenderStudioSceneIO.cpp)
	TSharedRef<FJsonObject> ExportObjects() const;
	void ImportObjects(const TSharedRef<FJsonObject>& Json);
	TSharedRef<FJsonObject> ExportEnvironment() const;
	void ImportEnvironment(const TSharedRef<FJsonObject>& Json);
	static TSharedRef<FJsonObject> ExportCamera(const FRenderStudioCamera& Camera);
	static FRenderStudioCamera ImportCamera(const TSharedPtr<FJsonObject>& Json);

	void PushUndoState(const FText& Description);
	void OnPreObjectPropertyChanged(UObject* Object, const FEditPropertyChain& PropertyChain);
	void OnObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& Event);
	AActor* GetOwningSceneObject(UObject* Object) const;
	void ResetHistory();
	void SetFilename(const FString& NewFilename);

	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	URenderStudioSettings* Settings;
	UMaterialInterface* DefaultMaterial;

	TArray<TWeakObjectPtr<AActor>> Objects;
	TMap<TWeakObjectPtr<AActor>, FString> Labels;
	TWeakObjectPtr<AActor> SelectedObject;
	TWeakObjectPtr<UMaterialInterface> SelectedMaterial;

	FRenderStudioCamera SceneCamera;
	FRenderStudioCamera Bookmarks[NumBookmarks];

	FString Filename;
	bool bDirty;

	TArray<FUndoState> UndoStack;
	TArray<FUndoState> RedoStack;
	int32 ChangeDepth;
	/** True while the outermost open change has pushed an undo step */
	bool bChangePushed;
	bool bRestoringState;
	double LastPropertyChangeTime;
	TWeakObjectPtr<UObject> LastPropertyChangeObject;

	FDelegateHandle PreObjectPropertyChangedHandle;
	FDelegateHandle ObjectPropertyChangedHandle;

	FSimpleMulticastDelegate ObjectsChangedEvent;
	FSimpleMulticastDelegate SelectionChangedEvent;
	FSimpleMulticastDelegate MaterialsChangedEvent;
	FSimpleMulticastDelegate DocumentChangedEvent;
	FSimpleMulticastDelegate SceneLoadedEvent;
};

/** Records an undo step for the duration of a scope. */
struct FRenderStudioScopedChange
{
	FRenderStudioScopedChange(FRenderStudioScene& InScene, const FText& Description)
		: Scene(InScene)
	{
		Scene.BeginChange(Description);
	}

	~FRenderStudioScopedChange()
	{
		Scene.EndChange();
	}

private:
	FRenderStudioScene& Scene;
};

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"

class AActor;
class FAdvancedPreviewScene;
class UMaterial;
class UMaterialInstanceConstant;
class UMaterialInterface;
class UPrimitiveComponent;
class URenderStudioSettings;
class UStaticMesh;
class UWorld;
struct FPreviewSceneProfile;

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

/**
 * The document of the application: owns the preview world (HDRI sky, floor, sun and
 * post-processing come from FAdvancedPreviewScene), the user's objects, the selection
 * and the materials created in this session.
 */
class FRenderStudioScene : public TSharedFromThis<FRenderStudioScene>, public FGCObject
{
public:
	FRenderStudioScene();
	virtual ~FRenderStudioScene();

	/** Must be called right after construction (needs a shared ref to bind delegates). */
	void Initialize();

	TSharedRef<FAdvancedPreviewScene> GetPreviewScene() const { return PreviewScene.ToSharedRef(); }
	UWorld* GetWorld() const;
	URenderStudioSettings* GetSettings() const { return Settings; }
	FPreviewSceneProfile& GetProfile() const;

	// Objects
	AActor* AddPrimitive(ERenderStudioPrimitive Primitive);
	AActor* AddStaticMesh(UStaticMesh* Mesh, const FString& Label, const FTransform& Transform = FTransform::Identity);
	AActor* AddLight(ERenderStudioLight LightType);
	/** Imports FBX / OBJ files (no options dialog) into the content folder and drops the meshes into the scene. Returns the number of meshes added. */
	int32 ImportMeshes(const TArray<FString>& Files);
	AActor* DuplicateObject(AActor* Actor);
	void DeleteObject(AActor* Actor);
	/** Removes every user object (environment, floor and sun are kept). */
	void Clear();
	/** Clear() + the default shader ball setup. */
	void ResetToDefault();

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

	// Environment shortcuts (stored in the asset viewer profile so they persist)
	bool IsFloorVisible() const;
	void SetFloorVisible(bool bVisible);
	bool IsEnvironmentVisible() const;
	void SetEnvironmentVisible(bool bVisible);
	bool IsTurntableEnabled() const;
	void SetTurntableEnabled(bool bEnabled);

	// Materials
	/** Creates a parameterised PBR material asset (BaseColor/Metallic/Roughness/Normal) in the content folder. */
	UMaterial* CreateMaterial(const FString& BaseName = TEXT("M_Material"));
	UMaterialInstanceConstant* CreateMaterialInstance(UMaterialInterface* Parent);
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

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FRenderStudioScene"); }

private:
	AActor* FinishSpawn(AActor* Actor, const FString& Label);
	void RegisterObject(AActor* Actor);
	bool IsComponentSelected(const UPrimitiveComponent* Component) const;
	void RefreshSelectionOutline(AActor* Actor) const;
	FString MakeUniqueLabel(const FString& BaseLabel) const;
	FVector GetDropLocation() const;

	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	URenderStudioSettings* Settings;
	UMaterialInterface* DefaultMaterial;

	TArray<TWeakObjectPtr<AActor>> Objects;
	TMap<TWeakObjectPtr<AActor>, FString> Labels;
	TWeakObjectPtr<AActor> SelectedObject;
	TWeakObjectPtr<UMaterialInterface> SelectedMaterial;

	FSimpleMulticastDelegate ObjectsChangedEvent;
	FSimpleMulticastDelegate SelectionChangedEvent;
	FSimpleMulticastDelegate MaterialsChangedEvent;
};

#include "RenderStudioScene.h"

#include "AdvancedPreviewScene.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AssetViewerSettings.h"
#include "AutomatedAssetImportData.h"
#include "Components/LightComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Light.h"
#include "Engine/PointLight.h"
#include "Engine/RectLight.h"
#include "Engine/SpotLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "IAssetTools.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "RenderStudioSettings.h"
#include "RenderStudioStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "RenderStudioScene"

namespace RenderStudioScene
{
	static const TCHAR* GetPrimitivePath(ERenderStudioPrimitive Primitive)
	{
		switch (Primitive)
		{
		case ERenderStudioPrimitive::Sphere:	return TEXT("/Engine/BasicShapes/Sphere.Sphere");
		case ERenderStudioPrimitive::Cylinder:	return TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
		case ERenderStudioPrimitive::Cone:		return TEXT("/Engine/BasicShapes/Cone.Cone");
		case ERenderStudioPrimitive::Plane:		return TEXT("/Engine/BasicShapes/Plane.Plane");
		case ERenderStudioPrimitive::Cube:
		default:								return TEXT("/Engine/BasicShapes/Cube.Cube");
		}
	}

	static FString GetPrimitiveLabel(ERenderStudioPrimitive Primitive)
	{
		switch (Primitive)
		{
		case ERenderStudioPrimitive::Sphere:	return TEXT("Sphere");
		case ERenderStudioPrimitive::Cylinder:	return TEXT("Cylinder");
		case ERenderStudioPrimitive::Cone:		return TEXT("Cone");
		case ERenderStudioPrimitive::Plane:		return TEXT("Plane");
		case ERenderStudioPrimitive::Cube:
		default:								return TEXT("Cube");
		}
	}

	static FActorSpawnParameters MakeSpawnParams()
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags = RF_Transient;
		return Params;
	}
}

FRenderStudioScene::FRenderStudioScene()
	: Settings(GetMutableDefault<URenderStudioSettings>())
	, DefaultMaterial(nullptr)
{
	PreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));
	DefaultMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
}

FRenderStudioScene::~FRenderStudioScene()
{
	for (const TWeakObjectPtr<AActor>& Object : Objects)
	{
		if (AActor* Actor = Object.Get())
		{
			TInlineComponentArray<UPrimitiveComponent*> Components(Actor);
			for (UPrimitiveComponent* Component : Components)
			{
				Component->SelectionOverrideDelegate.Unbind();
			}
		}
	}
}

void FRenderStudioScene::Initialize()
{
	ResetToDefault();
}

UWorld* FRenderStudioScene::GetWorld() const
{
	return PreviewScene->GetWorld();
}

FPreviewSceneProfile& FRenderStudioScene::GetProfile() const
{
	UAssetViewerSettings* ViewerSettings = UAssetViewerSettings::Get();
	return ViewerSettings->Profiles[PreviewScene->GetCurrentProfileIndex()];
}

//////////////////////////////////////////////////////////////////////////
// Objects

AActor* FRenderStudioScene::AddPrimitive(ERenderStudioPrimitive Primitive)
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, RenderStudioScene::GetPrimitivePath(Primitive));
	if (!Mesh)
	{
		return nullptr;
	}

	FVector Location = GetDropLocation();
	Location.Z = Primitive == ERenderStudioPrimitive::Plane ? 1.0f : 50.0f;
	return AddStaticMesh(Mesh, RenderStudioScene::GetPrimitiveLabel(Primitive), FTransform(Location));
}

AActor* FRenderStudioScene::AddStaticMesh(UStaticMesh* Mesh, const FString& Label, const FTransform& Transform)
{
	AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform, RenderStudioScene::MakeSpawnParams());
	if (!Actor)
	{
		return nullptr;
	}

	UStaticMeshComponent* MeshComponent = Actor->GetStaticMeshComponent();
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetStaticMesh(Mesh);
	MeshComponent->bCastDynamicShadow = true;

	return FinishSpawn(Actor, Label);
}

AActor* FRenderStudioScene::AddLight(ERenderStudioLight LightType)
{
	UClass* LightClass = APointLight::StaticClass();
	FString Label = TEXT("Point Light");
	FRotator Rotation = FRotator::ZeroRotator;

	if (LightType == ERenderStudioLight::Spot)
	{
		LightClass = ASpotLight::StaticClass();
		Label = TEXT("Spot Light");
		Rotation = FRotator(-90.0f, 0.0f, 0.0f);
	}
	else if (LightType == ERenderStudioLight::Rect)
	{
		LightClass = ARectLight::StaticClass();
		Label = TEXT("Area Light");
		Rotation = FRotator(-90.0f, 0.0f, 0.0f);
	}

	const FBox Bounds = GetBounds(false);
	FVector Location(0.0f, 0.0f, 250.0f);
	if (Bounds.IsValid)
	{
		Location = Bounds.GetCenter() + FVector(-Bounds.GetExtent().X - 100.0f, 0.0f, Bounds.GetExtent().Z + 150.0f);
	}

	ALight* Light = GetWorld()->SpawnActor<ALight>(LightClass, FTransform(Rotation, Location), RenderStudioScene::MakeSpawnParams());
	if (!Light)
	{
		return nullptr;
	}

	if (ULightComponent* LightComponent = Light->GetLightComponent())
	{
		LightComponent->SetMobility(EComponentMobility::Movable);
		LightComponent->SetCastShadows(true);
	}

	return FinishSpawn(Light, Label);
}

int32 FRenderStudioScene::ImportMeshes(const TArray<FString>& Files)
{
	if (Files.Num() == 0)
	{
		return 0;
	}

	UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
	ImportData->bReplaceExisting = true;
	ImportData->DestinationPath = Settings->GetContentFolder() / TEXT("Meshes");
	ImportData->Filenames = Files;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	const TArray<UObject*> ImportedObjects = AssetTools.ImportAssetsAutomated(ImportData);

	// Meshes from one file keep their relative layout; the whole group is then dropped onto the floor.
	TArray<AActor*> NewActors;
	for (UObject* Object : ImportedObjects)
	{
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(Object))
		{
			if (AActor* Actor = AddStaticMesh(Mesh, Mesh->GetName()))
			{
				NewActors.Add(Actor);
			}
		}
	}

	if (NewActors.Num() > 0)
	{
		FBox GroupBounds(ForceInit);
		for (AActor* Actor : NewActors)
		{
			GroupBounds += Actor->GetComponentsBoundingBox(true);
		}

		if (GroupBounds.IsValid)
		{
			const FVector Offset(-GroupBounds.GetCenter().X, -GroupBounds.GetCenter().Y, -GroupBounds.Min.Z);
			for (AActor* Actor : NewActors)
			{
				Actor->AddActorWorldOffset(Offset);
			}
		}

		SetSelectedObject(NewActors.Last());
		MaterialsChangedEvent.Broadcast();
	}

	return NewActors.Num();
}

AActor* FRenderStudioScene::DuplicateObject(AActor* Actor)
{
	if (!IsSceneObject(Actor))
	{
		return nullptr;
	}

	FActorSpawnParameters Params = RenderStudioScene::MakeSpawnParams();
	Params.Template = Actor;

	FTransform Transform = Actor->GetActorTransform();
	Transform.AddToTranslation(FVector(0.0f, Actor->GetComponentsBoundingBox(true).GetSize().Y + 20.0f, 0.0f));

	AActor* NewActor = GetWorld()->SpawnActor<AActor>(Actor->GetClass(), Transform, Params);
	if (!NewActor)
	{
		return nullptr;
	}

	// Template spawning copies properties but not per-instance material overrides on the root mesh
	UMeshComponent* SourceMesh = Actor->FindComponentByClass<UMeshComponent>();
	UMeshComponent* TargetMesh = NewActor->FindComponentByClass<UMeshComponent>();
	if (SourceMesh && TargetMesh)
	{
		for (int32 Index = 0; Index < SourceMesh->GetNumMaterials(); ++Index)
		{
			TargetMesh->SetMaterial(Index, SourceMesh->GetMaterial(Index));
		}
	}

	return FinishSpawn(NewActor, GetObjectLabel(Actor));
}

void FRenderStudioScene::DeleteObject(AActor* Actor)
{
	if (!IsSceneObject(Actor))
	{
		return;
	}

	if (SelectedObject.Get() == Actor)
	{
		SetSelectedObject(nullptr);
	}

	Objects.Remove(Actor);
	Labels.Remove(Actor);
	GetWorld()->DestroyActor(Actor);

	ObjectsChangedEvent.Broadcast();
}

void FRenderStudioScene::Clear()
{
	SetSelectedObject(nullptr);

	for (const TWeakObjectPtr<AActor>& Object : Objects)
	{
		if (AActor* Actor = Object.Get())
		{
			GetWorld()->DestroyActor(Actor);
		}
	}
	Objects.Empty();
	Labels.Empty();

	ObjectsChangedEvent.Broadcast();
}

void FRenderStudioScene::ResetToDefault()
{
	Clear();

	// The classic look-dev starting point: a shader ball on the floor under the HDRI.
	if (AActor* Ball = AddPrimitive(ERenderStudioPrimitive::Sphere))
	{
		Ball->SetActorLocation(FVector(0.0f, 0.0f, 75.0f));
		Ball->SetActorScale3D(FVector(1.5f));
		SetObjectLabel(Ball, TEXT("Shader Ball"));
		SetSelectedObject(Ball);
	}
}

TArray<AActor*> FRenderStudioScene::GetObjects() const
{
	TArray<AActor*> Result;
	for (const TWeakObjectPtr<AActor>& Object : Objects)
	{
		if (AActor* Actor = Object.Get())
		{
			Result.Add(Actor);
		}
	}
	return Result;
}

bool FRenderStudioScene::IsSceneObject(const AActor* Actor) const
{
	return Actor && Objects.Contains(const_cast<AActor*>(Actor));
}

FString FRenderStudioScene::GetObjectLabel(const AActor* Actor) const
{
	if (const FString* Label = Labels.Find(const_cast<AActor*>(Actor)))
	{
		return *Label;
	}
	return Actor ? Actor->GetName() : FString();
}

void FRenderStudioScene::SetObjectLabel(AActor* Actor, const FString& NewLabel)
{
	if (IsSceneObject(Actor) && !NewLabel.IsEmpty())
	{
		Labels.Add(Actor, NewLabel);
		ObjectsChangedEvent.Broadcast();
	}
}

bool FRenderStudioScene::IsObjectVisible(const AActor* Actor) const
{
	return Actor && Actor->GetRootComponent() && Actor->GetRootComponent()->IsVisible();
}

void FRenderStudioScene::SetObjectVisible(AActor* Actor, bool bVisible)
{
	if (Actor && Actor->GetRootComponent())
	{
		Actor->GetRootComponent()->SetVisibility(bVisible, true);
		ObjectsChangedEvent.Broadcast();
	}
}

const FSlateBrush* FRenderStudioScene::GetObjectIcon(const AActor* Actor) const
{
	if (Actor && Actor->IsA<ALight>())
	{
		return FRenderStudioStyle::GetBrush("RenderStudio.Icon.Light");
	}
	return FRenderStudioStyle::GetBrush("RenderStudio.Icon.Mesh");
}

FBox FRenderStudioScene::GetBounds(bool bSelectionOnly) const
{
	FBox Bounds(ForceInit);

	if (bSelectionOnly && SelectedObject.IsValid())
	{
		Bounds = SelectedObject->GetComponentsBoundingBox(true);
		if (Bounds.IsValid)
		{
			return Bounds;
		}
	}

	for (const TWeakObjectPtr<AActor>& Object : Objects)
	{
		if (Object.IsValid() && IsObjectVisible(Object.Get()))
		{
			Bounds += Object->GetComponentsBoundingBox(true);
		}
	}
	return Bounds;
}

AActor* FRenderStudioScene::FinishSpawn(AActor* Actor, const FString& Label)
{
	RegisterObject(Actor);
	Labels.Add(Actor, MakeUniqueLabel(Label));
	ObjectsChangedEvent.Broadcast();
	SetSelectedObject(Actor);
	return Actor;
}

void FRenderStudioScene::RegisterObject(AActor* Actor)
{
	Objects.AddUnique(Actor);

	TInlineComponentArray<UPrimitiveComponent*> Components(Actor);
	for (UPrimitiveComponent* Component : Components)
	{
		Component->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateSP(this, &FRenderStudioScene::IsComponentSelected);
		Component->PushSelectionToProxy();
	}
}

bool FRenderStudioScene::IsComponentSelected(const UPrimitiveComponent* Component) const
{
	return Component && SelectedObject.IsValid() && Component->GetOwner() == SelectedObject.Get();
}

void FRenderStudioScene::RefreshSelectionOutline(AActor* Actor) const
{
	if (Actor)
	{
		TInlineComponentArray<UPrimitiveComponent*> Components(Actor);
		for (UPrimitiveComponent* Component : Components)
		{
			Component->PushSelectionToProxy();
		}
	}
}

void FRenderStudioScene::SetSelectedObject(AActor* Actor)
{
	if (Actor && !IsSceneObject(Actor))
	{
		Actor = nullptr;
	}

	if (SelectedObject.Get() == Actor)
	{
		return;
	}

	AActor* Previous = SelectedObject.Get();
	SelectedObject = Actor;

	RefreshSelectionOutline(Previous);
	RefreshSelectionOutline(Actor);

	SelectionChangedEvent.Broadcast();
}

FString FRenderStudioScene::MakeUniqueLabel(const FString& BaseLabel) const
{
	TSet<FString> Existing;
	for (const TPair<TWeakObjectPtr<AActor>, FString>& Pair : Labels)
	{
		Existing.Add(Pair.Value);
	}

	if (!Existing.Contains(BaseLabel))
	{
		return BaseLabel;
	}

	// Strip a trailing number so "Cube 2" duplicates to "Cube 3", not "Cube 2 2"
	FString Root = BaseLabel;
	int32 SpaceIndex = INDEX_NONE;
	if (Root.FindLastChar(TEXT(' '), SpaceIndex) && Root.Mid(SpaceIndex + 1).IsNumeric())
	{
		Root = Root.Left(SpaceIndex);
	}

	for (int32 Suffix = 2;; ++Suffix)
	{
		const FString Candidate = FString::Printf(TEXT("%s %d"), *Root, Suffix);
		if (!Existing.Contains(Candidate))
		{
			return Candidate;
		}
	}
}

FVector FRenderStudioScene::GetDropLocation() const
{
	const FBox Bounds = GetBounds(false);
	if (!Bounds.IsValid)
	{
		return FVector::ZeroVector;
	}
	// Line new objects up to the right of what is already there
	return FVector(Bounds.GetCenter().X, Bounds.Max.Y + 75.0f, 0.0f);
}

//////////////////////////////////////////////////////////////////////////
// Environment

bool FRenderStudioScene::IsFloorVisible() const
{
	return GetProfile().bShowFloor;
}

void FRenderStudioScene::SetFloorVisible(bool bVisible)
{
	PreviewScene->SetFloorVisibility(bVisible);
}

bool FRenderStudioScene::IsEnvironmentVisible() const
{
	return GetProfile().bShowEnvironment;
}

void FRenderStudioScene::SetEnvironmentVisible(bool bVisible)
{
	PreviewScene->SetEnvironmentVisibility(bVisible);
}

bool FRenderStudioScene::IsTurntableEnabled() const
{
	return GetProfile().bRotateLightingRig;
}

void FRenderStudioScene::SetTurntableEnabled(bool bEnabled)
{
	// FAdvancedPreviewScene::Tick reads this flag every frame and spins the sky + sun.
	GetProfile().bRotateLightingRig = bEnabled;
}

//////////////////////////////////////////////////////////////////////////
// Materials

UMaterial* FRenderStudioScene::CreateMaterial(const FString& BaseName)
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	FString PackageName;
	FString AssetName;
	AssetTools.CreateUniqueAssetName(Settings->GetContentFolder() / TEXT("Materials") / BaseName, TEXT(""), PackageName, AssetName);

	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	UMaterial* Material = Cast<UMaterial>(AssetTools.CreateAsset(AssetName, FPackageName::GetLongPackagePath(PackageName), UMaterial::StaticClass(), Factory));
	if (!Material)
	{
		return nullptr;
	}

	// Build the standard metal/rough graph so the material is immediately useful from the
	// parameter panel, while still being a regular UE4 material that can be opened in the node editor.
	UTexture* WhiteTexture = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
	UTexture* FlatNormal = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineMaterials/FlatNormal.FlatNormal"));

	auto MakeTexture = [Material](FName Name, FName Group, UTexture* Texture, EMaterialSamplerType SamplerType, int32 X, int32 Y)
	{
		UMaterialExpressionTextureSampleParameter2D* Expression = Cast<UMaterialExpressionTextureSampleParameter2D>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), X, Y));
		Expression->ParameterName = Name;
		Expression->Group = Group;
		Expression->Texture = Texture;
		Expression->SamplerType = SamplerType;
		return Expression;
	};

	auto MakeScalar = [Material](FName Name, FName Group, float Value, int32 X, int32 Y)
	{
		UMaterialExpressionScalarParameter* Expression = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), X, Y));
		Expression->ParameterName = Name;
		Expression->Group = Group;
		Expression->DefaultValue = Value;
		Expression->SliderMin = 0.0f;
		Expression->SliderMax = 1.0f;
		return Expression;
	};

	auto MakeColor = [Material](FName Name, FName Group, const FLinearColor& Value, int32 X, int32 Y)
	{
		UMaterialExpressionVectorParameter* Expression = Cast<UMaterialExpressionVectorParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass(), X, Y));
		Expression->ParameterName = Name;
		Expression->Group = Group;
		Expression->DefaultValue = Value;
		return Expression;
	};

	auto MakeMultiply = [Material](int32 X, int32 Y)
	{
		return Cast<UMaterialExpressionMultiply>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionMultiply::StaticClass(), X, Y));
	};

	// Albedo = AlbedoMap * AlbedoColor
	UMaterialExpressionTextureSampleParameter2D* AlbedoMap = MakeTexture(TEXT("AlbedoMap"), TEXT("01 Albedo"), WhiteTexture, SAMPLERTYPE_Color, -700, -300);
	UMaterialExpressionVectorParameter* AlbedoColor = MakeColor(TEXT("AlbedoColor"), TEXT("01 Albedo"), FLinearColor(0.6f, 0.6f, 0.6f), -700, -100);
	UMaterialExpressionMultiply* AlbedoMultiply = MakeMultiply(-350, -250);
	UMaterialEditingLibrary::ConnectMaterialExpressions(AlbedoMap, TEXT("RGB"), AlbedoMultiply, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(AlbedoColor, TEXT(""), AlbedoMultiply, TEXT("B"));
	UMaterialEditingLibrary::ConnectMaterialProperty(AlbedoMultiply, TEXT(""), MP_BaseColor);

	// Metalness / roughness / specular
	UMaterialExpressionScalarParameter* Metallic = MakeScalar(TEXT("Metalness"), TEXT("02 Reflectivity"), 0.0f, -350, -50);
	UMaterialExpressionScalarParameter* Specular = MakeScalar(TEXT("Specular"), TEXT("02 Reflectivity"), 0.5f, -350, 30);
	UMaterialExpressionScalarParameter* Roughness = MakeScalar(TEXT("Roughness"), TEXT("03 Microsurface"), 0.4f, -350, 110);
	UMaterialEditingLibrary::ConnectMaterialProperty(Metallic, TEXT(""), MP_Metallic);
	UMaterialEditingLibrary::ConnectMaterialProperty(Specular, TEXT(""), MP_Specular);
	UMaterialEditingLibrary::ConnectMaterialProperty(Roughness, TEXT(""), MP_Roughness);

	// Normal map
	UMaterialExpressionTextureSampleParameter2D* NormalMap = MakeTexture(TEXT("NormalMap"), TEXT("04 Surface"), FlatNormal, SAMPLERTYPE_Normal, -700, 200);
	UMaterialEditingLibrary::ConnectMaterialProperty(NormalMap, TEXT("RGB"), MP_Normal);

	// Emission
	UMaterialExpressionVectorParameter* Emissive = MakeColor(TEXT("EmissiveColor"), TEXT("05 Emission"), FLinearColor::Black, -350, 300);
	UMaterialEditingLibrary::ConnectMaterialProperty(Emissive, TEXT(""), MP_EmissiveColor);

	UMaterialEditingLibrary::RecompileMaterial(Material);
	Material->MarkPackageDirty();

	SetSelectedMaterial(Material);
	MaterialsChangedEvent.Broadcast();
	return Material;
}

UMaterialInstanceConstant* FRenderStudioScene::CreateMaterialInstance(UMaterialInterface* Parent)
{
	if (!Parent)
	{
		return nullptr;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	FString BaseName = Parent->GetName();
	BaseName.RemoveFromStart(TEXT("M_"));
	BaseName.RemoveFromStart(TEXT("MI_"));

	FString PackageName;
	FString AssetName;
	AssetTools.CreateUniqueAssetName(Settings->GetContentFolder() / TEXT("Materials") / (TEXT("MI_") + BaseName), TEXT(""), PackageName, AssetName);

	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = Parent;

	UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(
		AssetTools.CreateAsset(AssetName, FPackageName::GetLongPackagePath(PackageName), UMaterialInstanceConstant::StaticClass(), Factory));
	if (Instance)
	{
		Instance->MarkPackageDirty();
		SetSelectedMaterial(Instance);
		MaterialsChangedEvent.Broadcast();
	}
	return Instance;
}

void FRenderStudioScene::AssignMaterial(AActor* Actor, UMaterialInterface* Material, int32 SlotIndex)
{
	if (!IsSceneObject(Actor) || !Material)
	{
		return;
	}

	TInlineComponentArray<UMeshComponent*> MeshComponents(Actor);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		const int32 NumMaterials = FMath::Max(1, MeshComponent->GetNumMaterials());
		for (int32 Index = 0; Index < NumMaterials; ++Index)
		{
			if (SlotIndex == INDEX_NONE || SlotIndex == Index)
			{
				MeshComponent->SetMaterial(Index, Material);
			}
		}
	}

	MaterialsChangedEvent.Broadcast();
}

TArray<UMaterialInterface*> FRenderStudioScene::GetMaterials() const
{
	TArray<UMaterialInterface*> Result;

	// Assets living in the application's content folder
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter Filter;
	Filter.PackagePaths.Add(*Settings->GetContentFolder());
	Filter.bRecursivePaths = true;
	Filter.ClassNames.Add(UMaterial::StaticClass()->GetFName());
	Filter.ClassNames.Add(UMaterialInstanceConstant::StaticClass()->GetFName());

	TArray<FAssetData> Assets;
	AssetRegistryModule.Get().GetAssets(Filter, Assets);
	for (const FAssetData& Asset : Assets)
	{
		if (UMaterialInterface* Material = Cast<UMaterialInterface>(Asset.GetAsset()))
		{
			Result.AddUnique(Material);
		}
	}

	// Plus whatever the scene is currently using (engine defaults, imported FBX materials, ...)
	for (const TWeakObjectPtr<AActor>& Object : Objects)
	{
		if (AActor* Actor = Object.Get())
		{
			TInlineComponentArray<UMeshComponent*> MeshComponents(Actor);
			for (UMeshComponent* MeshComponent : MeshComponents)
			{
				TArray<UMaterialInterface*> Used;
				MeshComponent->GetUsedMaterials(Used);
				for (UMaterialInterface* Material : Used)
				{
					if (Material)
					{
						Result.AddUnique(Material);
					}
				}
			}
		}
	}

	Result.Sort([](const UMaterialInterface& A, const UMaterialInterface& B) { return A.GetName() < B.GetName(); });
	return Result;
}

void FRenderStudioScene::SetSelectedMaterial(UMaterialInterface* Material)
{
	SelectedMaterial = Material;
	MaterialsChangedEvent.Broadcast();
}

void FRenderStudioScene::OpenMaterialEditor(UMaterialInterface* Material) const
{
	if (Material && GEditor)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Material);
	}
}

void FRenderStudioScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Settings);
	Collector.AddReferencedObject(DefaultMaterial);
}

#undef LOCTEXT_NAMESPACE

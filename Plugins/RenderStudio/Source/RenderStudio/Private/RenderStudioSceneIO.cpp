// Scene documents (.rss) and undo snapshots.
//
// An object is stored as its class, its label and, per component, the editable properties
// that differ from the class defaults (exported as UE4 property text). That keeps the format
// small and lets any change made in the Properties panel round-trip without per-type code.

#include "RenderStudioScene.h"

#include "AdvancedPreviewScene.h"
#include "AssetViewerSettings.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "RenderStudioScene"

namespace RenderStudioSceneIO
{
	static const int32 FileVersion = 1;

	static bool ShouldSaveProperty(const FProperty* Property)
	{
		return Property->HasAnyPropertyFlags(CPF_Edit)
			&& !Property->HasAnyPropertyFlags(CPF_Transient | CPF_EditConst | CPF_Deprecated | CPF_DisableEditOnInstance);
	}

	static TSharedRef<FJsonObject> ExportComponent(UActorComponent* Component)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		const UObject* Archetype = Component->GetArchetype();

		for (TFieldIterator<FProperty> It(Component->GetClass()); It; ++It)
		{
			const FProperty* Property = *It;
			if (!ShouldSaveProperty(Property))
			{
				continue;
			}

			for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
			{
				if (Archetype && Property->Identical_InContainer(Component, Archetype, Index, PPF_None))
				{
					continue;
				}

				FString Value;
				Property->ExportText_InContainer(Index, Value, Component, nullptr, Component, PPF_None);
				const FString Key = Property->ArrayDim > 1 ? FString::Printf(TEXT("%s[%d]"), *Property->GetName(), Index) : Property->GetName();
				Json->SetStringField(Key, Value);
			}
		}
		return Json;
	}

	static void ImportComponent(UActorComponent* Component, const TSharedPtr<FJsonObject>& Json)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Json->Values)
		{
			FString Name = Field.Key;
			int32 Index = 0;
			int32 BracketIndex = INDEX_NONE;
			if (Name.FindChar(TEXT('['), BracketIndex))
			{
				Index = FCString::Atoi(*Name.Mid(BracketIndex + 1));
				Name = Name.Left(BracketIndex);
			}

			FProperty* Property = FindFProperty<FProperty>(Component->GetClass(), *Name);
			if (!Property || !ShouldSaveProperty(Property) || Index < 0 || Index >= Property->ArrayDim)
			{
				continue;
			}

			const FString Value = Field.Value->AsString();
			Property->ImportText(*Value, Property->ContainerPtrToValuePtr<void>(Component, Index), PPF_None, Component);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Objects

TSharedRef<FJsonObject> FRenderStudioScene::ExportObjects() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> ObjectValues;
	int32 SelectedIndex = INDEX_NONE;

	for (AActor* Actor : GetObjects())
	{
		TSharedRef<FJsonObject> ObjectJson = MakeShared<FJsonObject>();
		ObjectJson->SetStringField(TEXT("Class"), Actor->GetClass()->GetPathName());
		ObjectJson->SetStringField(TEXT("Label"), GetObjectLabel(Actor));

		TSharedRef<FJsonObject> ComponentsJson = MakeShared<FJsonObject>();
		TInlineComponentArray<UActorComponent*> Components(Actor);
		for (UActorComponent* Component : Components)
		{
			if (Component->IsEditorOnly())
			{
				continue;
			}

			TSharedRef<FJsonObject> ComponentJson = RenderStudioSceneIO::ExportComponent(Component);
			if (ComponentJson->Values.Num() > 0)
			{
				ComponentsJson->SetObjectField(Component->GetName(), ComponentJson);
			}
		}
		ObjectJson->SetObjectField(TEXT("Components"), ComponentsJson);

		if (Actor == SelectedObject.Get())
		{
			SelectedIndex = ObjectValues.Num();
		}
		ObjectValues.Add(MakeShared<FJsonValueObject>(ObjectJson));
	}

	Json->SetArrayField(TEXT("Objects"), ObjectValues);
	Json->SetNumberField(TEXT("Selected"), SelectedIndex);
	return Json;
}

void FRenderStudioScene::ImportObjects(const TSharedRef<FJsonObject>& Json)
{
	TGuardValue<bool> Restoring(bRestoringState, true);

	ClearObjects();

	AActor* ToSelect = nullptr;
	const int32 SelectedIndex = static_cast<int32>(Json->GetNumberField(TEXT("Selected")));

	const TArray<TSharedPtr<FJsonValue>>* ObjectValues = nullptr;
	if (Json->TryGetArrayField(TEXT("Objects"), ObjectValues))
	{
		for (int32 ObjectIndex = 0; ObjectIndex < ObjectValues->Num(); ++ObjectIndex)
		{
			const TSharedPtr<FJsonObject> ObjectJson = (*ObjectValues)[ObjectIndex]->AsObject();
			if (!ObjectJson.IsValid())
			{
				continue;
			}

			UClass* Class = StaticLoadClass(AActor::StaticClass(), nullptr, *ObjectJson->GetStringField(TEXT("Class")), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Class || Class->HasAnyClassFlags(CLASS_Abstract))
			{
				continue;
			}

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.ObjectFlags = RF_Transient;
			AActor* Actor = GetWorld()->SpawnActor<AActor>(Class, FTransform::Identity, Params);
			if (!Actor)
			{
				continue;
			}

			const TSharedPtr<FJsonObject>* ComponentsJson = nullptr;
			if (ObjectJson->TryGetObjectField(TEXT("Components"), ComponentsJson))
			{
				TInlineComponentArray<UActorComponent*> Components(Actor);
				for (UActorComponent* Component : Components)
				{
					const TSharedPtr<FJsonObject>* ComponentJson = nullptr;
					if ((*ComponentsJson)->TryGetObjectField(Component->GetName(), ComponentJson))
					{
						RenderStudioSceneIO::ImportComponent(Component, *ComponentJson);
					}
				}

				// Re-register so transforms, meshes, materials and light settings reach the renderer
				for (UActorComponent* Component : Components)
				{
					if (Component->IsRegistered())
					{
						Component->ReregisterComponent();
					}
				}
			}

			RegisterObject(Actor);
			Labels.Add(Actor, ObjectJson->GetStringField(TEXT("Label")));

			if (ObjectIndex == SelectedIndex)
			{
				ToSelect = Actor;
			}
		}
	}

	ObjectsChangedEvent.Broadcast();
	SetSelectedObject(ToSelect);
	MaterialsChangedEvent.Broadcast();
}

//////////////////////////////////////////////////////////////////////////
// Environment and cameras

TSharedRef<FJsonObject> FRenderStudioScene::ExportEnvironment() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	FJsonObjectConverter::UStructToJsonObject(FPreviewSceneProfile::StaticStruct(), &GetProfile(), Json);
	return Json;
}

void FRenderStudioScene::ImportEnvironment(const TSharedRef<FJsonObject>& Json)
{
	FPreviewSceneProfile& Profile = GetProfile();

	// The profile keeps its own name: a scene only carries the look, not the user's profile list.
	const FString ProfileName = Profile.ProfileName;
	const bool bSharedProfile = Profile.bSharedProfile;
	FJsonObjectConverter::JsonObjectToUStruct(Json, FPreviewSceneProfile::StaticStruct(), &Profile);
	Profile.ProfileName = ProfileName;
	Profile.bSharedProfile = bSharedProfile;

	if (!Profile.EnvironmentCubeMapPath.IsEmpty() && Profile.EnvironmentCubeMap.IsNull())
	{
		Profile.EnvironmentCubeMap = TSoftObjectPtr<UTextureCube>(FSoftObjectPath(Profile.EnvironmentCubeMapPath));
	}
	else if (!Profile.EnvironmentCubeMap.IsNull())
	{
		Profile.EnvironmentCubeMapPath = Profile.EnvironmentCubeMap.ToSoftObjectPath().ToString();
	}

	PreviewScene->UpdateScene(Profile);
}

TSharedRef<FJsonObject> FRenderStudioScene::ExportCamera(const FRenderStudioCamera& Camera)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("Set"), Camera.bIsSet);
	Json->SetStringField(TEXT("Location"), Camera.Location.ToString());
	Json->SetStringField(TEXT("Rotation"), Camera.Rotation.ToString());
	Json->SetNumberField(TEXT("FieldOfView"), Camera.FieldOfView);
	return Json;
}

FRenderStudioCamera FRenderStudioScene::ImportCamera(const TSharedPtr<FJsonObject>& Json)
{
	FRenderStudioCamera Camera;
	if (Json.IsValid())
	{
		Camera.bIsSet = Json->GetBoolField(TEXT("Set"));
		Camera.Location.InitFromString(Json->GetStringField(TEXT("Location")));
		Camera.Rotation.InitFromString(Json->GetStringField(TEXT("Rotation")));
		Camera.FieldOfView = FMath::Clamp(static_cast<float>(Json->GetNumberField(TEXT("FieldOfView"))), 5.0f, 170.0f);
	}
	return Camera;
}

//////////////////////////////////////////////////////////////////////////
// Files

bool FRenderStudioScene::SaveScene(const FString& InFilename, FText& OutError)
{
	TSharedRef<FJsonObject> Json = ExportObjects();
	Json->SetNumberField(TEXT("Version"), RenderStudioSceneIO::FileVersion);
	Json->SetObjectField(TEXT("Environment"), ExportEnvironment());
	Json->SetObjectField(TEXT("Camera"), ExportCamera(SceneCamera));

	TArray<TSharedPtr<FJsonValue>> BookmarkValues;
	for (const FRenderStudioCamera& Bookmark : Bookmarks)
	{
		BookmarkValues.Add(MakeShared<FJsonValueObject>(ExportCamera(Bookmark)));
	}
	Json->SetArrayField(TEXT("Bookmarks"), BookmarkValues);

	FString Text;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Text);
	if (!FJsonSerializer::Serialize(Json, Writer))
	{
		OutError = LOCTEXT("SerializeFailed", "The scene could not be serialized.");
		return false;
	}

	if (!FFileHelper::SaveStringToFile(Text, *InFilename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FText::Format(LOCTEXT("WriteFailed", "Could not write {0}. Is the folder writable?"), FText::FromString(InFilename));
		return false;
	}

	SetFilename(InFilename);
	return true;
}

bool FRenderStudioScene::LoadScene(const FString& InFilename, FText& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *InFilename))
	{
		OutError = FText::Format(LOCTEXT("ReadFailed", "Could not read {0}."), FText::FromString(InFilename));
		return false;
	}

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid() || !Json->HasField(TEXT("Objects")))
	{
		OutError = FText::Format(LOCTEXT("ParseFailed", "{0} is not a Render Studio scene."), FText::FromString(InFilename));
		return false;
	}

	if (Json->GetNumberField(TEXT("Version")) > RenderStudioSceneIO::FileVersion)
	{
		OutError = LOCTEXT("NewerVersion", "This scene was saved by a newer version of Render Studio.");
		return false;
	}

	ImportObjects(Json.ToSharedRef());

	const TSharedPtr<FJsonObject>* EnvironmentJson = nullptr;
	if (Json->TryGetObjectField(TEXT("Environment"), EnvironmentJson))
	{
		ImportEnvironment(EnvironmentJson->ToSharedRef());
	}

	const TSharedPtr<FJsonObject>* CameraJson = nullptr;
	SceneCamera = Json->TryGetObjectField(TEXT("Camera"), CameraJson) ? ImportCamera(*CameraJson) : FRenderStudioCamera();

	const TArray<TSharedPtr<FJsonValue>>* BookmarkValues = nullptr;
	Json->TryGetArrayField(TEXT("Bookmarks"), BookmarkValues);
	for (int32 Index = 0; Index < NumBookmarks; ++Index)
	{
		Bookmarks[Index] = (BookmarkValues && BookmarkValues->IsValidIndex(Index)) ? ImportCamera((*BookmarkValues)[Index]->AsObject()) : FRenderStudioCamera();
	}

	ResetHistory();
	SetFilename(InFilename);
	SceneLoadedEvent.Broadcast();
	return true;
}

#undef LOCTEXT_NAMESPACE

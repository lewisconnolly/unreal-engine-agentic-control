#include "AgenticControlServer.h"

#include "Common/TcpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/DirectionalLight.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerStart.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Async/Async.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AutomatedAssetImportData.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Factories/TextureFactory.h"
#include "Engine/Texture2D.h"
#include "UObject/SavePackage.h"

FAgenticControlServer::FAgenticControlServer(int32 InPort)
	: Port(InPort)
{
}

FAgenticControlServer::~FAgenticControlServer()
{
	Stop();
}

void FAgenticControlServer::Start()
{
	Thread = FRunnableThread::Create(this, TEXT("AgenticControlServer"));
}

void FAgenticControlServer::Stop()
{
	bStopping = true;

	if (ClientSocket)
	{
		ClientSocket->Close();
	}

	if (ListenerSocket)
	{
		ListenerSocket->Close();
	}

	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}

	if (ListenerSocket)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket);
		ListenerSocket = nullptr;
	}
}

bool FAgenticControlServer::Init()
{
	return true;
}

uint32 FAgenticControlServer::Run()
{
	FIPv4Endpoint Endpoint(FIPv4Address::Any, Port);

	ListenerSocket = FTcpSocketBuilder(TEXT("AgenticControlListener"))
		.AsReusable()
		.BoundToEndpoint(Endpoint)
		.Listening(8);

	if (!ListenerSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("AgenticControl: Failed to create listener socket on port %d"), Port);
		return 1;
	}

	UE_LOG(LogTemp, Log, TEXT("AgenticControl: Listening on port %d"), Port);

	while (!bStopping)
	{
		bool bHasPendingConnection = false;
		ListenerSocket->HasPendingConnection(bHasPendingConnection);

		if (!bHasPendingConnection)
		{
			FPlatformProcess::Sleep(0.1f);
			continue;
		}

		TSharedRef<FInternetAddr> RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		ClientSocket = ListenerSocket->Accept(*RemoteAddr, TEXT("AgenticControlClient"));

		if (!ClientSocket)
		{
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("AgenticControl: Client connected from %s"), *RemoteAddr->ToString(true));

		// Read data from client until disconnect
		TArray<uint8> Buffer;
		FString Accumulated;

		while (!bStopping)
		{
			// Check if the client is still connected
			ESocketConnectionState ConnState = ClientSocket->GetConnectionState();
			if (ConnState == SCS_ConnectionError)
			{
				UE_LOG(LogTemp, Log, TEXT("AgenticControl: Client connection lost"));
				break;
			}

			uint32 PendingDataSize = 0;
			if (!ClientSocket->HasPendingData(PendingDataSize))
			{
				// No data available — do a zero-byte Recv to detect disconnect.
				// On a cleanly-closed TCP socket, Recv returns 0.
				uint8 Probe = 0;
				int32 ProbeRead = 0;
				if (!ClientSocket->Recv(&Probe, 0, ProbeRead, ESocketReceiveFlags::Peek))
				{
					UE_LOG(LogTemp, Log, TEXT("AgenticControl: Client disconnected (recv failed)"));
					break;
				}
				FPlatformProcess::Sleep(0.01f);
				continue;
			}

			Buffer.SetNumUninitialized(PendingDataSize);
			int32 BytesRead = 0;
			ClientSocket->Recv(Buffer.GetData(), PendingDataSize, BytesRead);

			if (BytesRead <= 0)
			{
				break;
			}

			Accumulated += FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData())));

			// Process complete lines (newline-delimited JSON)
			FString Line;
			while (Accumulated.Split(TEXT("\n"), &Line, &Accumulated))
			{
				FString Response = HandleCommand(Line.TrimStartAndEnd());
				Response += TEXT("\n");

				FTCHARToUTF8 Converter(*Response);
				int32 BytesSent = 0;
				ClientSocket->Send(
					reinterpret_cast<const uint8*>(Converter.Get()),
					Converter.Length(),
					BytesSent
				);
			}
		}

		ClientSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
		ClientSocket = nullptr;
		UE_LOG(LogTemp, Log, TEXT("AgenticControl: Client disconnected"));
	}

	return 0;
}

void FAgenticControlServer::Exit()
{
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

UClass* FAgenticControlServer::GetActorClassFromType(const FString& ActorType)
{
	static const TMap<FString, UClass*> TypeMap = {
		{ TEXT("StaticMeshActor"), AStaticMeshActor::StaticClass() },
		{ TEXT("PointLight"),      APointLight::StaticClass() },
		{ TEXT("SpotLight"),       ASpotLight::StaticClass() },
		{ TEXT("DirectionalLight"),ADirectionalLight::StaticClass() },
		{ TEXT("CameraActor"),     ACameraActor::StaticClass() },
		{ TEXT("PlayerStart"),     APlayerStart::StaticClass() },
	};

	const UClass* const* Found = TypeMap.Find(ActorType);
	return Found ? const_cast<UClass*>(*Found) : nullptr;
}

AActor* FAgenticControlServer::FindActorByLabel(UWorld* World, const FString& ActorLabel)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetActorLabel() == ActorLabel)
		{
			return *It;
		}
	}
	return nullptr;
}

FString FAgenticControlServer::SerializeTransform(const FTransform& Transform)
{
	const FVector& Loc = Transform.GetLocation();
	const FRotator Rot = Transform.Rotator();
	const FVector& Scale = Transform.GetScale3D();

	return FString::Printf(
		TEXT("{\"location\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},"
			 "\"rotation\":{\"pitch\":%.2f,\"yaw\":%.2f,\"roll\":%.2f},"
			 "\"scale\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f}}"),
		Loc.X, Loc.Y, Loc.Z,
		Rot.Pitch, Rot.Yaw, Rot.Roll,
		Scale.X, Scale.Y, Scale.Z
	);
}

// ---------------------------------------------------------------------------
// Command router
// ---------------------------------------------------------------------------

FString FAgenticControlServer::HandleCommand(const FString& JsonCommand)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonCommand);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return TEXT("{\"success\":false,\"error\":\"Invalid JSON\"}");
	}

	FString Command;
	if (!JsonObject->TryGetStringField(TEXT("command"), Command))
	{
		return TEXT("{\"success\":false,\"error\":\"Missing command field\"}");
	}

	const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;

	if (Command == TEXT("spawn_actor"))
	{
		if (JsonObject->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr)
		{
			return HandleSpawnActor(*ParamsPtr);
		}
		return TEXT("{\"success\":false,\"error\":\"Missing params for spawn_actor\"}");
	}
	else if (Command == TEXT("get_scene_info"))
	{
		return HandleGetSceneInfo();
	}
	else if (Command == TEXT("delete_actor"))
	{
		if (JsonObject->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr)
		{
			return HandleDeleteActor(*ParamsPtr);
		}
		return TEXT("{\"success\":false,\"error\":\"Missing params for delete_actor\"}");
	}
	else if (Command == TEXT("set_transform"))
	{
		if (JsonObject->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr)
		{
			return HandleSetTransform(*ParamsPtr);
		}
		return TEXT("{\"success\":false,\"error\":\"Missing params for set_transform\"}");
	}
	else if (Command == TEXT("import_asset"))
	{
		if (JsonObject->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr)
		{
			return HandleImportAsset(*ParamsPtr);
		}
		return TEXT("{\"success\":false,\"error\":\"Missing params for import_asset\"}");
	}
	else if (Command == TEXT("apply_material"))
	{
		if (JsonObject->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr)
		{
			return HandleApplyMaterial(*ParamsPtr);
		}
		return TEXT("{\"success\":false,\"error\":\"Missing params for apply_material\"}");
	}

	return TEXT("{\"success\":false,\"error\":\"Unknown command\"}");
}

// ---------------------------------------------------------------------------
// spawn_actor — dispatches to game thread, spawns a real actor
// ---------------------------------------------------------------------------

FString FAgenticControlServer::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorType;
	Params->TryGetStringField(TEXT("actor_type"), ActorType);

	double X = 0, Y = 0, Z = 0;
	Params->TryGetNumberField(TEXT("x"), X);
	Params->TryGetNumberField(TEXT("y"), Y);
	Params->TryGetNumberField(TEXT("z"), Z);

	UE_LOG(LogTemp, Log, TEXT("AgenticControl: spawn_actor type=%s pos=(%.1f, %.1f, %.1f)"),
		*ActorType, X, Y, Z);

	FString ResultJson;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World)
		{
			ResultJson = TEXT("{\"success\":false,\"error\":\"No editor world available\"}");
			DoneEvent->Trigger();
			return;
		}

		UClass* ActorClass = GetActorClassFromType(ActorType);
		if (!ActorClass)
		{
			ResultJson = FString::Printf(
				TEXT("{\"success\":false,\"error\":\"Unknown actor type: %s\"}"), *ActorType);
			DoneEvent->Trigger();
			return;
		}

		FVector Location(X, Y, Z);
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* NewActor = World->SpawnActor(ActorClass, &Location, nullptr, SpawnParams);
		if (!NewActor)
		{
			ResultJson = TEXT("{\"success\":false,\"error\":\"SpawnActor returned null\"}");
			DoneEvent->Trigger();
			return;
		}

		// For StaticMeshActor, assign a default cube mesh so it's visible
		if (AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(NewActor))
		{
			UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(
				nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			if (CubeMesh && MeshActor->GetStaticMeshComponent())
			{
				MeshActor->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
			}
		}

		FString ActorLabel = NewActor->GetActorLabel();
		FString Transform = SerializeTransform(NewActor->GetActorTransform());

		ResultJson = FString::Printf(
			TEXT("{\"success\":true,\"actor_id\":\"%s\",\"actor_type\":\"%s\",\"transform\":%s}"),
			*ActorLabel, *ActorType, *Transform);

		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	return ResultJson;
}

// ---------------------------------------------------------------------------
// get_scene_info — dispatches to game thread, iterates all actors
// ---------------------------------------------------------------------------

FString FAgenticControlServer::HandleGetSceneInfo()
{
	UE_LOG(LogTemp, Log, TEXT("AgenticControl: get_scene_info"));

	FString ResultJson;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World)
		{
			ResultJson = TEXT("{\"success\":false,\"error\":\"No editor world available\"}");
			DoneEvent->Trigger();
			return;
		}

		FString ActorsArray = TEXT("[");
		bool bFirst = true;

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor)
			{
				continue;
			}

			if (!bFirst)
			{
				ActorsArray += TEXT(",");
			}
			bFirst = false;

			FString Label = Actor->GetActorLabel();
			FString ClassName = Actor->GetClass()->GetName();
			FString Transform = SerializeTransform(Actor->GetActorTransform());

			ActorsArray += FString::Printf(
				TEXT("{\"actor_id\":\"%s\",\"class\":\"%s\",\"transform\":%s}"),
				*Label, *ClassName, *Transform);
		}

		ActorsArray += TEXT("]");
		ResultJson = FString::Printf(TEXT("{\"success\":true,\"actors\":%s}"), *ActorsArray);

		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	return ResultJson;
}

// ---------------------------------------------------------------------------
// delete_actor — dispatches to game thread, destroys actor by label
// ---------------------------------------------------------------------------

FString FAgenticControlServer::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorId;
	Params->TryGetStringField(TEXT("actor_id"), ActorId);

	UE_LOG(LogTemp, Log, TEXT("AgenticControl: delete_actor id=%s"), *ActorId);

	FString ResultJson;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World)
		{
			ResultJson = TEXT("{\"success\":false,\"error\":\"No editor world available\"}");
			DoneEvent->Trigger();
			return;
		}

		AActor* Actor = FindActorByLabel(World, ActorId);
		if (!Actor)
		{
			ResultJson = FString::Printf(
				TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *ActorId);
			DoneEvent->Trigger();
			return;
		}

		bool bDestroyed = World->DestroyActor(Actor);
		if (bDestroyed)
		{
			ResultJson = FString::Printf(
				TEXT("{\"success\":true,\"actor_id\":\"%s\"}"), *ActorId);
		}
		else
		{
			ResultJson = FString::Printf(
				TEXT("{\"success\":false,\"error\":\"Failed to destroy actor: %s\"}"), *ActorId);
		}

		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	return ResultJson;
}

// ---------------------------------------------------------------------------
// set_transform — dispatches to game thread, applies partial transform update
// ---------------------------------------------------------------------------

FString FAgenticControlServer::HandleSetTransform(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorId;
	Params->TryGetStringField(TEXT("actor_id"), ActorId);

	UE_LOG(LogTemp, Log, TEXT("AgenticControl: set_transform id=%s"), *ActorId);

	FString ResultJson;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	// Capture param values on this thread before dispatching
	TSharedPtr<FJsonObject> ParamsCopy = Params;

	AsyncTask(ENamedThreads::GameThread, [&ResultJson, &ActorId, ParamsCopy, DoneEvent]()
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World)
		{
			ResultJson = TEXT("{\"success\":false,\"error\":\"No editor world available\"}");
			DoneEvent->Trigger();
			return;
		}

		AActor* Actor = FindActorByLabel(World, ActorId);
		if (!Actor)
		{
			ResultJson = FString::Printf(
				TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *ActorId);
			DoneEvent->Trigger();
			return;
		}

		FTransform CurrentTransform = Actor->GetActorTransform();
		FVector Location = CurrentTransform.GetLocation();
		FRotator Rotation = CurrentTransform.Rotator();
		FVector Scale = CurrentTransform.GetScale3D();

		// Apply only provided params (partial update)
		double Val;
		if (ParamsCopy->TryGetNumberField(TEXT("x"), Val))       Location.X = Val;
		if (ParamsCopy->TryGetNumberField(TEXT("y"), Val))       Location.Y = Val;
		if (ParamsCopy->TryGetNumberField(TEXT("z"), Val))       Location.Z = Val;
		if (ParamsCopy->TryGetNumberField(TEXT("yaw"), Val))     Rotation.Yaw = Val;
		if (ParamsCopy->TryGetNumberField(TEXT("pitch"), Val))   Rotation.Pitch = Val;
		if (ParamsCopy->TryGetNumberField(TEXT("roll"), Val))    Rotation.Roll = Val;
		if (ParamsCopy->TryGetNumberField(TEXT("scale_x"), Val)) Scale.X = Val;
		if (ParamsCopy->TryGetNumberField(TEXT("scale_y"), Val)) Scale.Y = Val;
		if (ParamsCopy->TryGetNumberField(TEXT("scale_z"), Val)) Scale.Z = Val;

		FTransform NewTransform;
		NewTransform.SetLocation(Location);
		NewTransform.SetRotation(Rotation.Quaternion());
		NewTransform.SetScale3D(Scale);

		Actor->SetActorTransform(NewTransform);

		FString Transform = SerializeTransform(Actor->GetActorTransform());
		ResultJson = FString::Printf(
			TEXT("{\"success\":true,\"actor_id\":\"%s\",\"transform\":%s}"),
			*ActorId, *Transform);

		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	return ResultJson;
}

// ---------------------------------------------------------------------------
// import_asset — imports a file from disk into /Game/Generated/
// ---------------------------------------------------------------------------

FString FAgenticControlServer::HandleImportAsset(const TSharedPtr<FJsonObject>& Params)
{
	FString FilePath;
	Params->TryGetStringField(TEXT("file_path"), FilePath);

	FString AssetName;
	Params->TryGetStringField(TEXT("asset_name"), AssetName);

	UE_LOG(LogTemp, Log, TEXT("AgenticControl: import_asset file=%s name=%s"), *FilePath, *AssetName);

	FString ResultJson;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&ResultJson, FilePath, AssetName, DoneEvent]()
	{
		FString DestinationPath = FString::Printf(TEXT("/Game/Generated/%s"), *AssetName);

		UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
		ImportData->Filenames.Add(FilePath);
		ImportData->DestinationPath = TEXT("/Game/Generated");
		ImportData->bReplaceExisting = true;

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		TArray<UObject*> ImportedAssets = AssetTools.ImportAssetsAutomated(ImportData);

		if (ImportedAssets.Num() > 0 && ImportedAssets[0])
		{
			FString AssetPath = ImportedAssets[0]->GetPathName();
			ResultJson = FString::Printf(
				TEXT("{\"success\":true,\"asset_path\":\"%s\"}"), *AssetPath);
		}
		else
		{
			ResultJson = FString::Printf(
				TEXT("{\"success\":false,\"error\":\"Failed to import asset from: %s\"}"), *FilePath);
		}

		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	return ResultJson;
}

// ---------------------------------------------------------------------------
// apply_material — creates a material from a texture and applies to an actor
// ---------------------------------------------------------------------------

FString FAgenticControlServer::HandleApplyMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorId;
	Params->TryGetStringField(TEXT("actor_id"), ActorId);

	FString TextureAssetPath;
	Params->TryGetStringField(TEXT("texture_asset_path"), TextureAssetPath);

	UE_LOG(LogTemp, Log, TEXT("AgenticControl: apply_material actor=%s texture=%s"),
		*ActorId, *TextureAssetPath);

	FString ResultJson;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();

	AsyncTask(ENamedThreads::GameThread, [&ResultJson, ActorId, TextureAssetPath, DoneEvent]()
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World)
		{
			ResultJson = TEXT("{\"success\":false,\"error\":\"No editor world available\"}");
			DoneEvent->Trigger();
			return;
		}

		AActor* Actor = FindActorByLabel(World, ActorId);
		if (!Actor)
		{
			ResultJson = FString::Printf(
				TEXT("{\"success\":false,\"error\":\"Actor not found: %s\"}"), *ActorId);
			DoneEvent->Trigger();
			return;
		}

		// Load the texture asset
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TextureAssetPath);
		if (!Texture)
		{
			ResultJson = FString::Printf(
				TEXT("{\"success\":false,\"error\":\"Texture not found: %s\"}"), *TextureAssetPath);
			DoneEvent->Trigger();
			return;
		}

		// Create a material package
		FString MaterialName = FString::Printf(TEXT("M_%s"), *Actor->GetActorLabel());
		FString MaterialPackagePath = FString::Printf(TEXT("/Game/Generated/%s"), *MaterialName);

		UPackage* MaterialPackage = CreatePackage(*MaterialPackagePath);
		UMaterial* Material = NewObject<UMaterial>(MaterialPackage, *MaterialName,
			RF_Public | RF_Standalone);

		// Create a texture sample expression and wire to base color
		UMaterialExpressionTextureSample* TextureSample =
			NewObject<UMaterialExpressionTextureSample>(Material);
		TextureSample->Texture = Texture;
		Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(TextureSample);
		Material->GetEditorOnlyData()->BaseColor.Expression = TextureSample;

		Material->PreEditChange(nullptr);
		Material->PostEditChange();

		// Save the material package
		FString PackageFilename = FPackageName::LongPackageNameToFilename(
			MaterialPackagePath, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(MaterialPackage, Material, *PackageFilename, SaveArgs);

		// Apply to the actor's static mesh component
		UStaticMeshComponent* MeshComp = Actor->FindComponentByClass<UStaticMeshComponent>();
		if (MeshComp)
		{
			MeshComp->SetMaterial(0, Material);
			ResultJson = FString::Printf(
				TEXT("{\"success\":true,\"actor_id\":\"%s\",\"material_path\":\"%s\"}"),
				*ActorId, *MaterialPackagePath);
		}
		else
		{
			ResultJson = FString::Printf(
				TEXT("{\"success\":false,\"error\":\"Actor %s has no StaticMeshComponent\"}"),
				*ActorId);
		}

		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	return ResultJson;
}

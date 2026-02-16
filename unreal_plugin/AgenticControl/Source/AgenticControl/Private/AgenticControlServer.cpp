#include "AgenticControlServer.h"

#include "Common/TcpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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
		FSocket* ClientSocket = ListenerSocket->Accept(*RemoteAddr, TEXT("AgenticControlClient"));

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
			uint32 PendingDataSize = 0;
			if (!ClientSocket->HasPendingData(PendingDataSize))
			{
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
		UE_LOG(LogTemp, Log, TEXT("AgenticControl: Client disconnected"));
	}

	return 0;
}

void FAgenticControlServer::Exit()
{
}

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

	if (Command == TEXT("spawn_actor"))
	{
		const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
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

	return TEXT("{\"success\":false,\"error\":\"Unknown command\"}");
}

FString FAgenticControlServer::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
	// Stub implementation — returns a hardcoded success response.
	// Real UE API calls will be wired up in a follow-up phase.
	FString ActorType;
	Params->TryGetStringField(TEXT("actor_type"), ActorType);

	double X = 0, Y = 0, Z = 0;
	Params->TryGetNumberField(TEXT("x"), X);
	Params->TryGetNumberField(TEXT("y"), Y);
	Params->TryGetNumberField(TEXT("z"), Z);

	UE_LOG(LogTemp, Log, TEXT("AgenticControl: spawn_actor type=%s pos=(%.1f, %.1f, %.1f)"),
		*ActorType, X, Y, Z);

	// TODO: Dispatch to game thread and actually spawn the actor
	// AsyncTask(ENamedThreads::GameThread, [=]() { ... });

	return FString::Printf(
		TEXT("{\"success\":true,\"actor_id\":\"%s_1\",\"actor_type\":\"%s\",\"position\":{\"x\":%.1f,\"y\":%.1f,\"z\":%.1f}}"),
		*ActorType, *ActorType, X, Y, Z
	);
}

FString FAgenticControlServer::HandleGetSceneInfo()
{
	// Stub implementation — returns an empty actor list.
	// Real UE API calls will be wired up in a follow-up phase.
	UE_LOG(LogTemp, Log, TEXT("AgenticControl: get_scene_info"));

	// TODO: Dispatch to game thread and query actual scene
	return TEXT("{\"success\":true,\"actors\":[]}");
}

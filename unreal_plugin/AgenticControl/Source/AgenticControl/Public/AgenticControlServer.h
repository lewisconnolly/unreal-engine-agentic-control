#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Sockets.h"

/**
 * TCP server that listens for JSON commands from the MCP server
 * and dispatches them to the game thread for UE API execution.
 *
 * Runs on a background thread (FRunnable) to avoid blocking the Editor.
 * Protocol: newline-delimited JSON over TCP.
 */
class AGENTICCONTROL_API FAgenticControlServer : public FRunnable
{
public:
	FAgenticControlServer(int32 InPort = 9000);
	virtual ~FAgenticControlServer();

	/** Start the TCP listener on a background thread. */
	void Start();

	/** Signal the server to stop and wait for the thread to finish. */
	void Stop();

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;

private:
	/** Process a single JSON command string and return a JSON response. */
	FString HandleCommand(const FString& JsonCommand);

	/** Handle spawn_actor command. Returns JSON response. */
	FString HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);

	/** Handle get_scene_info command. Returns JSON response. */
	FString HandleGetSceneInfo();

	int32 Port;
	FSocket* ListenerSocket = nullptr;
	FRunnableThread* Thread = nullptr;
	FThreadSafeBool bStopping = false;
};

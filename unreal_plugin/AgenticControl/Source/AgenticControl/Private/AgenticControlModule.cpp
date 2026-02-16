#include "Modules/ModuleManager.h"
#include "AgenticControlServer.h"

class FAgenticControlModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Server = MakeUnique<FAgenticControlServer>(9000);
		Server->Start();
		UE_LOG(LogTemp, Log, TEXT("AgenticControl: Module started"));
	}

	virtual void ShutdownModule() override
	{
		if (Server)
		{
			Server->Stop();
			Server.Reset();
		}
		UE_LOG(LogTemp, Log, TEXT("AgenticControl: Module shut down"));
	}

private:
	TUniquePtr<FAgenticControlServer> Server;
};

IMPLEMENT_MODULE(FAgenticControlModule, AgenticControl)

#include "AutomationGameInstance.h"

#include "DummyViewport.h"
#include "AutomationWorld.h"
#include "GameFramework/GameModeBase.h"

void UAutomationGameInstance::InitializeForAutomation(const FAutomationWorldInitParams& InitParams, const FWorldInitializationValues& InitValues)
{
	// create world context for minimal world
	WorldContext = &GetEngine()->CreateNewWorldContext(EWorldType::Game);
	WorldContext->OwningGameInstance = this;

	static uint32 Counter = 0;

	// create world package with unique name
	const FName PackageName = *FString::Printf(TEXT("%s%d"), TEXT("AutomationWorld"), Counter++);
	UPackage* WorldPackage = NewObject<UPackage>(nullptr, PackageName, RF_Transient);
	WorldPackage->ThisContainsMap();
	// add PlayInEditor flag to disable dirtying world package
	WorldPackage->SetPackageFlags(PKG_PlayInEditor);

	// create world and initialize game instance with world and world context
	UWorld* AutomationWorld = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, WorldPackage, true, ERHIFeatureLevel::Num, &InitValues, true);
	WorldContext->SetCurrentWorld(AutomationWorld);
	AutomationWorld->SetGameInstance(this);

	// invoke callbacks that should happen before world is fully initialized
	if (InitParams.InitWorld)
	{
		Invoke(InitParams.InitWorld, AutomationWorld);
	}
	if (InitParams.InitWorldSettings)
	{
		Invoke(InitParams.InitWorldSettings, AutomationWorld->GetWorldSettings());
	}

	// finalize world initialization
	AutomationWorld->InitWorld(InitValues);
	AutomationWorld->PersistentLevel->UpdateModelComponents();
	AutomationWorld->UpdateWorldComponents(true, false);

	// create game viewport client to avoid ensures
	UGameViewportClient* NewViewport = NewObject<UGameViewportClient>(GetEngine());
	NewViewport->Init(*WorldContext, this, false);

	// Set the overlay widget, to avoid an ensure
	TSharedRef<SOverlay> DudOverlay = SNew(SOverlay);

	NewViewport->SetViewportOverlayWidget(nullptr, DudOverlay);

	// Set the internal FViewport, for the new game viewport, to avoid another bit of auto-exit code
	NewViewport->Viewport = new FDummyViewport(NewViewport);
	
	// Set the world context game viewport, to match the newly created viewport, in order to prevent crashes
	WorldContext->GameViewport = NewViewport;

	Init();
}

AGameModeBase* UAutomationGameInstance::CreateGameModeForURL(FURL InURL, UWorld* InWorld)
{
	if (FAutomationWorld::IsRunningAutomationWorld() && DefaultGameModeClass != nullptr)
	{
		// Spawn the GameMode.
		UE_LOG(LogLoad, Log, TEXT("Game class is '%s'"), *DefaultGameModeClass->GetName());
		
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save game modes into a map

		return InWorld->SpawnActor<AGameModeBase>(DefaultGameModeClass, SpawnInfo);
	}
	
	return Super::CreateGameModeForURL(InURL, InWorld);
}

#include "AutomationWorld.h"

#include "AutomationGameInstance.h"
#include "CommonAutomationSettings.h"
#include "DummyViewport.h"
#include "EngineUtils.h"
#include "GameInstanceAutomationSupport.h"
#include "GameMapsSettings.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameModeBase.h"
#include "Streaming/LevelStreamingDelegates.h"
#include "Subsystems/LocalPlayerSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogAutomationWorld, Log, Log);

template <typename TSubsystemType>
struct FScopeDisableSubsystemCreation
{
	FScopeDisableSubsystemCreation(TConstArrayView<UClass*> InEnabledSubsystems)
		: EnabledSubsystems(InEnabledSubsystems)
	{
		DisabledSubsystems = UCommonAutomationSettings::Get()->GetDisabledSubsystems<TSubsystemType>();
		// filter enabled subsystems from default settings
		for (auto ClassIt = DisabledSubsystems.CreateIterator(); ClassIt; ++ClassIt)
		{
			if (InEnabledSubsystems.Contains(*ClassIt))
			{
				ClassIt.RemoveCurrentSwap();
			}
		}

		// apply CLASS_Abstract flag so that subsystems are skipped during subsystem collection initialization
		for (UClass* SubsystemClass: DisabledSubsystems)
		{
			SubsystemClass->ClassFlags |= CLASS_Abstract;
		}
	}

	~FScopeDisableSubsystemCreation()
	{
		// remove applied abstract flag
		for (UClass* SubsystemClass: DisabledSubsystems)
		{
			SubsystemClass->ClassFlags &= ~CLASS_Abstract;
		}
	}

private:
	TConstArrayView<UClass*> EnabledSubsystems;
	TArray<UClass*> DisabledSubsystems;
};

template <typename TSubsystemType, typename T>
FObjectSubsystemCollection<TSubsystemType>* GetSubsystemCollection(T* Owner)
{
	static_assert(sizeof(FObjectSubsystemCollection<TSubsystemType>) == sizeof(FSubsystemCollectionBase));
	// @todo: this works on assumption that FSubsystemCollection is the last in object's memory layout
	// we can't detect if it has been changed, although this part of Engine code is stable for the last n revisions
	using TCollectionType = FObjectSubsystemCollection<TSubsystemType>;
	constexpr int32 CollectionOffset = sizeof(T) - sizeof(TCollectionType);
	
	return reinterpret_cast<TCollectionType*>(reinterpret_cast<uint8*>(Owner) + CollectionOffset);
}

bool FAutomationWorld::bExists = false;
UGameInstance* FAutomationWorld::SharedGameInstance = nullptr;

FAutomationWorldInitParams FAutomationWorldInitParams::Minimal{EWorldType::Game, EWorldInitFlags::Minimal};
FAutomationWorldInitParams FAutomationWorldInitParams::WithGameInstance{EWorldType::Game, EWorldInitFlags::WithGameInstance};
FAutomationWorldInitParams FAutomationWorldInitParams::WithLocalPlayer{EWorldType::Game, EWorldInitFlags::WithLocalPlayer};

FAutomationWorldInitParams& FAutomationWorldInitParams::SetWorldPackage(const FString& InWorldPackage)
{
	WorldPackage = InWorldPackage;
	return *this;
}

FAutomationWorldInitParams& FAutomationWorldInitParams::SetWorldPackage(FSoftObjectPath InWorldPath)
{
	UAssetRegistryHelpers::FixupRedirectedAssetPath(InWorldPath);
	WorldPackage = InWorldPath.GetLongPackageName();
	
	return *this;
}

FWorldInitializationValues FAutomationWorldInitParams::CreateWorldInitValues() const
{
	FWorldInitializationValues InitValues{};
	InitValues.InitializeScenes(ShouldInitScene())
			  .AllowAudioPlayback(!!(InitFlags & EWorldInitFlags::InitAudio))
			  .RequiresHitProxies(!!(InitFlags & EWorldInitFlags::InitHitProxy))
			  .CreatePhysicsScene(!!(InitFlags & EWorldInitFlags::InitPhysics))
			  .CreateNavigation(!!(InitFlags & EWorldInitFlags::InitNavigation))
			  .CreateAISystem(!!(InitFlags & EWorldInitFlags::InitAI))
			  .ShouldSimulatePhysics(!!(InitFlags & EWorldInitFlags::InitWeldedBodies))
			  .EnableTraceCollision(!!(InitFlags & EWorldInitFlags::InitCollision))
			  .SetTransactional(false) // @todo: does transactional ever matters for game worlds?
			  .CreateFXSystem(!!(InitFlags & EWorldInitFlags::InitFX))
			  .CreateWorldPartition(!!(InitFlags & EWorldInitFlags::InitWorldPartition));
	if (DefaultGameMode != nullptr)
	{
		// override world settings game mode. If DefaultGM is null and WorldSettings GM null, then project's default GM will be used
		InitValues.SetDefaultGameMode(DefaultGameMode);
	}

	return InitValues;
}

bool FAutomationWorldInitParams::ShouldInitScene() const
{
	constexpr EWorldInitFlags ShouldInitScene = EWorldInitFlags::InitScene | EWorldInitFlags::InitPhysics | EWorldInitFlags::InitWeldedBodies |
												EWorldInitFlags::InitHitProxy | EWorldInitFlags::InitCollision | EWorldInitFlags::InitFX;
	return !!(InitFlags & ShouldInitScene);
}

FAutomationWorld::FAutomationWorld(UWorld* InWorld, const FAutomationWorldInitParams& InitParams)
{
	bExists = true;
	InitialFrameCounter = GFrameCounter;

	FLevelStreamingDelegates::OnLevelStreamingStateChanged.AddRaw(this, &FAutomationWorld::HandleLevelStreamingStateChange);

	// create game instance if it was requested by user. Game instance is required for game mode
	// @note: use fallback game instance to create game mode? Don't create game instance if not explicitly specified?
	if (InitParams.CreateGameInstance() || InitParams.DefaultGameMode != nullptr)
	{
		check(InitParams.WorldType == EWorldType::Game);
		CreateGameInstance(InitParams);
	}

	// initialize automation world with new game world
	InitializeNewWorld(InWorld, InitParams);

	// create viewport client if game instance is specified
	if (GameInstance != nullptr && WorldContext != nullptr)
	{
		check(InitParams.WorldType == EWorldType::Game);
		CreateViewportClient();
	}

	// conditionally start play
	if (InitParams.RouteStartPlay())
	{
		RouteStartPlay();
	}

	// conditionally create primary player.
	// @note: create before StartPlay? add an option?
	if (InitParams.CreatePrimaryPlayer())
	{
		GetOrCreatePrimaryPlayer();
	}
}

void FAutomationWorld::InitializeNewWorld(UWorld* InWorld, const FAutomationWorldInitParams& InitParams)
{
	World = InWorld;
	World->AddToRoot();
	World->SetGameInstance(GameInstance);
	
	// Step 1: swap GWorld to point to a newly created world
	PrevGWorld = GWorld;
	GWorld = World;

	// Step 2: create and initialize world context
	WorldContext = &GEngine->CreateNewWorldContext(InitParams.WorldType);
	WorldContext->SetCurrentWorld(World);
	WorldContext->OwningGameInstance = GameInstance;
	if (GameInstance != nullptr)
	{
		// notify game instance that it is initialized for automation (primarily to set world context)
		CastChecked<IGameInstanceAutomationSupport>(GameInstance)->InitForAutomation(WorldContext);
	}
	
	// Step 3: initialize world settings
	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!InitParams.HasWorldPackage() && InitParams.DefaultGameMode)
	{
		// override default game mode if the world is created and not loaded
		WorldSettings->DefaultGameMode = InitParams.DefaultGameMode;
	}
	
	// Step 4: invoke callbacks that should happen before world is fully initialized
	InitParams.InitWorld.ExecuteIfBound(World);
	InitParams.InitWorldSettings.ExecuteIfBound(WorldSettings);
	
	// Step 5: finish world initialization (see FScopedEditorWorld::Init)
	World->WorldType = InitParams.WorldType;
	// tick viewports only in editor worlds
	TickType = InitParams.WorldType == EWorldType::Game ? LEVELTICK_All : LEVELTICK_ViewportsOnly;

	{
		;
		FScopeDisableSubsystemCreation<UWorldSubsystem> Scope{InitParams.WorldSubsystems};
		World->InitWorld(InitParams.CreateWorldInitValues());
		WorldCollection = GetSubsystemCollection<UWorldSubsystem>(World);
	}
	
	if (GameInstance != nullptr)
	{
		World->SetGameMode({});
	}
	World->PersistentLevel->UpdateModelComponents();
	World->UpdateWorldComponents(true, false);
	World->UpdateLevelStreaming();
}

void FAutomationWorld::CreateGameInstance(const FAutomationWorldInitParams& InitParams)
{
	if (SharedGameInstance == nullptr)
	{
		const UClass* GameInstanceClass = GetDefault<UGameMapsSettings>()->GameInstanceClass.TryLoadClass<UGameInstance>();
		if (GameInstanceClass == nullptr || !GameInstanceClass->ImplementsInterface(UGameInstanceAutomationSupport::StaticClass()))
		{
			// If an invalid or unsupported class type is specified we fall back to the default.
			GameInstanceClass = UAutomationGameInstance::StaticClass();
		}

		// create shared game instance
		FScopeDisableSubsystemCreation<UGameInstanceSubsystem> Scope{InitParams.GameSubsystems};
		SharedGameInstance = NewObject<UGameInstance>(GEngine, GameInstanceClass, TEXT("SharedGameInstance"), RF_Transient);
#if REUSE_GAME_INSTANCE
		// add to root so game instance lives between automation worlds
		SharedGameInstance->AddToRoot();
#endif
	}

	GameInstance = SharedGameInstance;
	GameInstanceCollection = GetSubsystemCollection<UGameInstanceSubsystem>(GameInstance);
	check(GameInstance);
}

void FAutomationWorld::CreateViewportClient()
{
	check(WorldContext && GameInstance);
	// create game viewport client to avoid ensures
	UGameViewportClient* NewViewport = NewObject<UGameViewportClient>(GameInstance->GetEngine());
	NewViewport->Init(*WorldContext, GameInstance, false);

	// Set the overlay widget, to avoid an ensure
	TSharedRef<SOverlay> DudOverlay = SNew(SOverlay);

	NewViewport->SetViewportOverlayWidget(nullptr, DudOverlay);

	// Set the internal FViewport, for the new game viewport, to avoid another bit of auto-exit code
	NewViewport->Viewport = new FDummyViewport(NewViewport);
	
	// Set the world context game viewport, to match the newly created viewport, in order to prevent crashes
	WorldContext->GameViewport = NewViewport;
}

const TArray<UWorldSubsystem*>& FAutomationWorld::GetWorldSubsystems() const
{
	return WorldCollection->GetSubsystemArray<UWorldSubsystem>(UWorldSubsystem::StaticClass());
}

void FAutomationWorld::HandleLevelStreamingStateChange(UWorld* OtherWorld, const ULevelStreaming* LevelStreaming, ULevel* LevelIfLoaded,  ELevelStreamingState PrevState, ELevelStreamingState NewState)
{
	if (World != OtherWorld)
	{
		return;
	}
	
	if (LevelIfLoaded)
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			// Clear RF_Standalone flag that keeps tile worlds from being GC'd in editor
			UWorld* LevelOuterWorld = LevelIfLoaded->GetTypedOuter<UWorld>();
			// sanity check that tile world is not our main world
			check(LevelOuterWorld != World);
			LevelOuterWorld->ClearFlags(GARBAGE_COLLECTION_KEEPFLAGS);
		}
#endif
	}
}

FAutomationWorld::~FAutomationWorld()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAutomationWorld::DestroyWorld);
	
	check(IsValid(World));
	FLevelStreamingDelegates::OnLevelStreamingStateChanged.RemoveAll(this);
	
	if (World->GetBegunPlay())
	{
		RouteEndPlay();
	}

	// shutdown game instance
	if (GameInstance != nullptr)
	{
		GameInstance->Shutdown();
	}

	// destroy world and world context
	GEngine->ShutdownWorldNetDriver(World);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);

	// null pointers to subsystem collections
	WorldCollection = nullptr;
	GameInstanceCollection = nullptr;

	World = nullptr;
	WorldContext = nullptr;
	GameInstance = nullptr;
#if !REUSE_GAME_INSTANCE
	if (SharedGameInstance != nullptr)
	{
		SharedGameInstance->RemoveFromRoot();
		SharedGameInstance = nullptr;
	}
#endif

	// restore globals and garbage collect the world
	GFrameCounter = InitialFrameCounter;
	GWorld = PrevGWorld;
	GEngine->ForceGarbageCollection();
	
	bExists = false;
}

FAutomationWorldPtr FAutomationWorld::CreateWorld(const FAutomationWorldInitParams& InitParams)
{
	if (Exists())
	{
		UE_LOG(LogAutomationWorld, Fatal, TEXT("%s: Tring to create second automation world"), *FString(__FUNCTION__));
		return nullptr;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FAutomationWorld::CreateWorld);
	
	UWorld* NewWorld = nullptr;
	// load game world flow
	if (InitParams.HasWorldPackage())
	{
		if (!FPackageName::IsValidLongPackageName(InitParams.WorldPackage))
		{
			// world package name is ill formed
			UE_LOG(LogAutomationWorld, Error, TEXT("%s: Specified package name %s is not a valid long package name"), *FString(__FUNCTION__), *InitParams.WorldPackage);
			return nullptr;
		}
		
		if (!FPackageName::DoesPackageExist(InitParams.WorldPackage))
		{
			// world package doesn't exist on disk
			UE_LOG(LogAutomationWorld, Error, TEXT("%s: Specified package name %s doesn't exist on disk"), *FString(__FUNCTION__), *InitParams.WorldPackage);
			return nullptr;
		}

		const FName WorldPackageName{InitParams.WorldPackage};
		UWorld::WorldTypePreLoadMap.FindOrAdd(WorldPackageName) = InitParams.WorldType;
		UPackage* WorldPackage = LoadPackage(nullptr, *InitParams.WorldPackage, InitParams.LoadFlags);
		UWorld::WorldTypePreLoadMap.Remove(WorldPackageName);

		if (WorldPackage == nullptr)
		{
			UE_LOG(LogAutomationWorld, Error, TEXT("%s: Failed to load package %s"), *FString(__FUNCTION__), *InitParams.WorldPackage);
		}
		
		NewWorld = UWorld::FindWorldInPackage(WorldPackage);
		if (NewWorld == nullptr)
		{
			NewWorld = UWorld::FollowWorldRedirectorInPackage(WorldPackage);
		}
		check(NewWorld);
	}
	
	bool bWithGameInstance = !!(InitParams.InitFlags & EWorldInitFlags::CreateGameInstance);
	if (NewWorld == nullptr)
	{
		UPackage* WorldPackage = nullptr;
		if (bWithGameInstance)
		{
			static uint32 PackageNameCounter = 0;
			// create unique package name for an empty world. Add /Temp/ prefix to avoid "package always doesn't exist" warning
			const FName PackageName = *FString::Printf(TEXT("/Temp/%s%d"), TEXT("AutomationWorld"), PackageNameCounter++);
			/** */
			WorldPackage = NewObject<UPackage>(nullptr, PackageName, RF_Transient);
			// mark as map package
			WorldPackage->ThisContainsMap();
			// add PlayInEditor flag to disable dirtying world package
			WorldPackage->SetPackageFlags(PKG_PlayInEditor);
		}

		FWorldInitializationValues InitValues = InitParams.CreateWorldInitValues();
		NewWorld = UWorld::CreateWorld(InitParams.WorldType, false, NAME_None, WorldPackage, true, ERHIFeatureLevel::Num, &InitValues, true);
	}

	if (NewWorld == nullptr)
	{
		UE_LOG(LogAutomationWorld, Error, TEXT("%s: Failed to create world for automation."), *FString(__FUNCTION__));
		return nullptr;
	}

	FAutomationWorldPtr AutomationWorld = MakeShareable(new FAutomationWorld(NewWorld, InitParams));
	
	return AutomationWorld;
}

FAutomationWorldPtr FAutomationWorld::CreateGameWorld(EWorldInitFlags InitFlags)
{
	FAutomationWorldInitParams InitParams{EWorldType::Game, InitFlags};
	return CreateWorld(InitParams);
}

FAutomationWorldPtr FAutomationWorld::CreateGameWorldWithGameInstance(TSubclassOf<AGameModeBase> DefaultGameMode, EWorldInitFlags InitFlags)
{
	return CreateWorld(FAutomationWorldInitParams{EWorldType::Game, EWorldInitFlags::WithGameInstance | InitFlags}.SetGameMode(DefaultGameMode));
}

FAutomationWorldPtr FAutomationWorld::CreateGameWorldWithPlayer(TSubclassOf<AGameModeBase> DefaultGameMode, EWorldInitFlags InitFlags)
{
	return CreateWorld(FAutomationWorldInitParams{EWorldType::Game, EWorldInitFlags::WithLocalPlayer | InitFlags}.SetGameMode(DefaultGameMode));
}

FAutomationWorldPtr FAutomationWorld::CreateEditorWorld(EWorldInitFlags InitFlags)
{
	FAutomationWorldInitParams InitParams{EWorldType::Editor, InitFlags};
	return CreateWorld(InitParams);
}

FAutomationWorldPtr FAutomationWorld::LoadGameWorld(const FString& WorldPackage, EWorldInitFlags InitFlags)
{
	return CreateWorld(FAutomationWorldInitParams{EWorldType::Game, InitFlags}.SetWorldPackage(WorldPackage));
}

FAutomationWorldPtr FAutomationWorld::LoadGameWorld(const FSoftObjectPath& WorldPath, EWorldInitFlags InitFlags)
{
	return CreateWorld(FAutomationWorldInitParams{EWorldType::Game, InitFlags}.SetWorldPackage(WorldPath));
}

FAutomationWorldPtr FAutomationWorld::LoadEditorWorld(const FString& WorldPackage, EWorldInitFlags InitFlags)
{
	FAutomationWorldInitParams InitParams{EWorldType::Editor, InitFlags};
	InitParams.WorldPackage = WorldPackage;

	return CreateWorld(InitParams);
}

FAutomationWorldPtr FAutomationWorld::LoadEditorWorld(const FSoftObjectPath& WorldPath, EWorldInitFlags InitFlags)
{
	return CreateWorld(FAutomationWorldInitParams{EWorldType::Editor, InitFlags}.SetWorldPackage(WorldPath));
}

bool FAutomationWorld::Exists()
{
	return bExists;
}

UGameInstanceSubsystem* FAutomationWorld::GetOrCreateSubsystem(TSubclassOf<UGameInstanceSubsystem> SubsystemClass)
{
	check(World && World->bIsWorldInitialized);
	check(GameInstanceCollection);
	
	if (GameInstance == nullptr || SubsystemClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}
	
	UGameInstanceSubsystem* Subsystem = GameInstance->GetSubsystemBase(SubsystemClass);
	if (Subsystem == nullptr)
	{
		Subsystem = GameInstanceCollection->GetSubsystem(SubsystemClass);
	}

	if (Subsystem == nullptr)
	{
		// create subsystem only if it should be created
		if (const UGameInstanceSubsystem* CDO = GetDefault<UGameInstanceSubsystem>(SubsystemClass); CDO->ShouldCreateSubsystem(GameInstance))
		{
			Subsystem = CastChecked<UGameInstanceSubsystem>(AddAndInitializeSubsystem(GameInstanceCollection, SubsystemClass, GameInstance));
		}
	}

	return Subsystem;
}

UWorldSubsystem* FAutomationWorld::GetOrCreateSubsystem(TSubclassOf<UWorldSubsystem> SubsystemClass)
{
	check(World && World->bIsWorldInitialized);
	check(WorldCollection);

	if (SubsystemClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}
	
	UWorldSubsystem* Subsystem = World->GetSubsystemBase(SubsystemClass);
	if (Subsystem == nullptr)
	{
		Subsystem = WorldCollection->GetSubsystem(SubsystemClass);
	}
	
	if (Subsystem == nullptr)
	{
		// create subsystem only if it should be created for a given world
		if (const UWorldSubsystem* CDO = GetDefault<UWorldSubsystem>(SubsystemClass); CDO->ShouldCreateSubsystem(World))
		{
			Subsystem = CastChecked<UWorldSubsystem>(AddAndInitializeSubsystem(WorldCollection, SubsystemClass, World));
			
			Subsystem->PostInitialize();
			Subsystem->OnWorldComponentsUpdated(*World);
			if (World->HasBegunPlay())
			{
				Subsystem->OnWorldBeginPlay(*World);
			}
		}
	}
	
	return Subsystem;
}

USubsystem* FAutomationWorld::AddAndInitializeSubsystem(FSubsystemCollectionBase* Collection, TSubclassOf<USubsystem> SubsystemClass, UObject* Outer)
{
	// This relies on FSubsystemCollectionBase having following memory alignment:
	// vpointer
	// FSubsystemMap SubsystemMap
	// FSubsystemArrayMap SubsystemArrayMap
	// Other members..
	// If it crashes, update implementation accordingly
	
	using FSubsystemMap = TMap<TObjectPtr<UClass>, TObjectPtr<USubsystem>>;
	using FSubsystemArrayMap =TMap<UClass*, TArray<USubsystem*>>;

	constexpr int32 SubsystemMapOffset = sizeof(void*); // vpointer offset
	constexpr int32 SubsystemArrayOffset = sizeof(FSubsystemMap) + SubsystemMapOffset;
	
	USubsystem* Subsystem = NewObject<USubsystem>(Outer, SubsystemClass);
	
	// This is a direct implementation from AddAndInitializeSubsystem
	// add subsystem to the collection's subsystem map
	FSubsystemMap* SubsystemMap = (FSubsystemMap*)(reinterpret_cast<uint8*>(Collection) + SubsystemMapOffset);
	FSubsystemArrayMap* SubsystemArrayMap = (FSubsystemArrayMap*)(reinterpret_cast<uint8*>(Collection) + SubsystemArrayOffset);
	
	SubsystemMap->Add(SubsystemClass, Subsystem);

	// initialize subsystem
	Subsystem->Initialize(*Collection);
	
	// Add this new subsystem to any existing maps of base classes to lists of subsystems
	for (TPair<UClass*, TArray<USubsystem*>>& Pair : *SubsystemArrayMap)
	{
		if (SubsystemClass->IsChildOf(Pair.Key))
		{
			Pair.Value.Add(Subsystem);
		}
	}

	return Subsystem;
}

ULocalPlayer* FAutomationWorld::GetOrCreatePrimaryPlayer()
{
	check(World && WorldContext);
	if (UNLIKELY(World->WorldType == EWorldType::Editor))
	{
		return nullptr;
	}
	
	if (ULocalPlayer* PrimaryPlayer = GEngine->GetFirstGamePlayer(World))
	{
		return PrimaryPlayer;
	}

	return CreateLocalPlayer();
}

ULocalPlayer* FAutomationWorld::CreateLocalPlayer()
{
	check(World && WorldContext);
	if (UNLIKELY(World->WorldType == EWorldType::Editor))
	{
		return nullptr;
	}
	
	if (GameInstance != nullptr)
	{
		// GameInstance, GameMode and GameSession are required to create a LocalPlayer/PlayerController pair
		if (auto GameMode = World->GetAuthGameMode(); GameMode != nullptr && GameMode->GameSession != nullptr)
		{
			// @todo: check World->bBegunPlay as well?

			const TArray<ULocalPlayer*>& LocalPlayers = GEngine->GetGamePlayers(World);
			
			FString Error;
			FScopeDisableSubsystemCreation<ULocalPlayerSubsystem> Scope{PlayerSubsystems};
			ULocalPlayer* LocalPlayer = GameInstance->CreateLocalPlayer(LocalPlayers.Num(), Error, true);

			checkf(Error.IsEmpty(), TEXT("%s"), *Error);

			return LocalPlayer;
		}
	}
	
	return nullptr;
}

void FAutomationWorld::DestroyLocalPlayer(ULocalPlayer* LocalPlayer)
{
	if (UNLIKELY(World->WorldType == EWorldType::Editor))
	{
		return;
	}
	
	const TArray<ULocalPlayer*>& LocalPlayers = GEngine->GetGamePlayers(World);
	if (LocalPlayers.Contains(LocalPlayer))
	{
		World->GetGameInstance()->RemoveLocalPlayer(LocalPlayer);
	}
}

void FAutomationWorld::RouteStartPlay() const
{
	check(World && World->bIsWorldInitialized);
	if (UNLIKELY(World->WorldType == EWorldType::Editor))
	{
		return;
	}
	
	if (World->GetBegunPlay())
	{
		return;
	}
	
	FURL URL{};
	World->InitializeActorsForPlay(URL);

	// call OnWorldBeginPlay for world subsystems and StartPlay for GameMode
	World->BeginPlay();
	
	if (World->GetAuthGameMode() == nullptr)
	{
		// call BeginPlay for actors
		TActorIterator<AWorldSettings> WorldSettings(World);
		WorldSettings->NotifyBeginPlay();
	}
	
	check(World->GetBegunPlay());
}

void FAutomationWorld::RouteEndPlay() const
{
	check(World && World->bIsWorldInitialized);
	if (UNLIKELY(World->WorldType == EWorldType::Editor))
	{
		return;
	}
	
	if (!World->GetBegunPlay())
	{
		return;
	}
	
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		// don't know which EEndPlayReason to select. Let's assume it mimics PIE
		It->RouteEndPlay(EEndPlayReason::EndPlayInEditor);
	}

	World->SetBegunPlay(false);
}

void FAutomationWorld::TickWorld(int32 NumFrames)
{
	constexpr float DeltaTime = 1.0 / 60.0;
	while (NumFrames > 0)
	{
		World->Tick(TickType, DeltaTime);
		
		// tick streamable manager
		FTickableGameObject::TickObjects(nullptr, LEVELTICK_All, false, DeltaTime);
		// update level streaming, as we're not drawing viewport which usually updates it
		World->UpdateLevelStreaming();

		// tick for FAsyncMixin
		FTSTicker::GetCoreTicker().Tick(DeltaTime);
		++GFrameCounter;
		--NumFrames;
	}
}

UWorld* FAutomationWorld::GetWorld() const
{
	return World;
}

FWorldContext* FAutomationWorld::GetWorldContext() const
{
	return WorldContext;
}

UGameInstance* FAutomationWorld::GetGameInstance() const
{
	return GameInstance;
}


#include "AutomationWorldTests.h"

#include "AutomationTestDefinition.h"
#include "AutomationWorld.h"
#include "EngineUtils.h"
#include "GameInstanceAutomationSupport.h"
#include "NavigationSystem.h"
#include "AI/NavigationSystemBase.h"
#include "GameFramework/GameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"
#include "WorldPartition/WorldPartition.h"

constexpr auto AutomationTestFlags = EAutomationTestFlags::EngineFilter | EAutomationTestFlags::EditorContext | EAutomationTestFlags::CriticalPriority;

namespace UE::Private
{
	static bool bTestSubsystemEnabled = false;
}

bool UTestWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return UE::Private::bTestSubsystemEnabled;
}

bool UTestGameInstanceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return UE::Private::bTestSubsystemEnabled;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationWorld_CreateWorldUniqueTest, "CommonAutomation.AutomationWorld.CreateWorld returns unique world", AutomationTestFlags)

bool FAutomationWorld_CreateWorldUniqueTest::RunTest(const FString& Parameters)
{
	FAutomationWorldPtr ScopedWorld = FAutomationWorld::CreateGameWorld(EWorldInitFlags::WithGameInstance);
	UTEST_TRUE("Automation world is valid", ScopedWorld.IsValid());

	const FObjectKey WorldKey{ScopedWorld->GetWorld()};
	const FObjectKey GameInstanceKey{ScopedWorld->GetGameInstance()};
	const FObjectKey PackageKey{ScopedWorld->GetWorld()->GetPackage()};

	UTEST_TRUE("World is valid",		 WorldKey != FObjectKey{});
	UTEST_TRUE("World package is valid", PackageKey != FObjectKey{});
	UTEST_TRUE("Game instance is valid", GameInstanceKey != FObjectKey{});

	ScopedWorld.Reset();
	ScopedWorld = FAutomationWorld::CreateGameWorld(EWorldInitFlags::WithGameInstance);

	UTEST_TRUE("World is unique",			WorldKey		!= FObjectKey{ScopedWorld->GetWorld()});
	UTEST_TRUE("World package is unique",	PackageKey		!= FObjectKey{ScopedWorld->GetWorld()->GetPackage()});
#if !REUSE_GAME_INSTANCE
	UTEST_TRUE("Game instance is unique",	GameInstanceKey	!= FObjectKey{ScopedWorld->GetGameInstance()});
#endif
	
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationWorld_LoadWorldUniqueTest, "CommonAutomation.AutomationWorld.LoadWorld returns unique world", AutomationTestFlags)

bool FAutomationWorld_LoadWorldUniqueTest::RunTest(const FString& Parameters)
{
	const FString TestMapPackageName{TEXT("/Engine/Maps/Entry")};
	FAutomationWorldPtr ScopedWorld = FAutomationWorld::LoadGameWorld(TestMapPackageName, EWorldInitFlags::WithGameInstance);
	UTEST_TRUE("Automation world is valid", ScopedWorld.IsValid());
	
	const FObjectKey WorldKey{ScopedWorld->GetWorld()};
	const FObjectKey GameInstanceKey{ScopedWorld->GetGameInstance()};
	const FObjectKey PackageKey{ScopedWorld->GetWorld()->GetPackage()};

	UTEST_TRUE("World is valid",		 WorldKey != FObjectKey{});
	UTEST_TRUE("World package is valid", PackageKey != FObjectKey{});
	UTEST_TRUE("Game instance is valid", GameInstanceKey != FObjectKey{});

	ScopedWorld.Reset();
	ScopedWorld = FAutomationWorld::LoadGameWorld(TestMapPackageName, EWorldInitFlags::WithGameInstance);

	UTEST_TRUE("World is unique",			WorldKey		!= FObjectKey{ScopedWorld->GetWorld()});
	UTEST_TRUE("Game instance is unique",	GameInstanceKey	!= FObjectKey{ScopedWorld->GetGameInstance()});
#if !REUSE_GAME_INSTANCE
	UTEST_TRUE("World package is unique",	PackageKey		!= FObjectKey{ScopedWorld->GetWorld()->GetPackage()});
#endif
	
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationWorldBehaviorTests, "CommonAutomation.AutomationWorld.Behavior", AutomationTestFlags)

bool FAutomationWorldBehaviorTests::RunTest(const FString& Parameters)
{
	{
		// guarantees with a basic game world.
		FAutomationWorldPtr WorldPtr = FAutomationWorld::CreateGameWorld();

		const UWorld* World = WorldPtr->GetWorld();
		UTEST_TRUE("Automation world is running", FAutomationWorld::Exists() == true);
		UTEST_TRUE("Automation world returns a valid world", IsValid(World));
		UTEST_TRUE("World is part of a root set", World->IsRooted() == true);
		UTEST_TRUE("World is initialized", World->bIsWorldInitialized);
		UTEST_TRUE("GWorld equals to new world", GWorld == World);
		UTEST_TRUE("World has world context", GEngine->GetWorldContextFromWorld(World) != nullptr);
	
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, false);
		UTEST_TRUE("Automation world is valid after garbage collection", IsValid(World));

		WorldPtr.Reset();
		UTEST_TRUE("Automation world no longer running running", FAutomationWorld::Exists() == false);
		UTEST_TRUE("World is not longer a part of a root set", World->IsRooted() == false);
		// UTEST_TRUE("Automation world is no longer valid", !IsValid(World)); // @todo: not guaranteed if gc hasn't run. Mark world as garbage?
	}

	{
		// game mode is not created by default
		// @todo: this is not true, because project game mode will override game mode from world settings. Is this a desired behavior?
		FAutomationWorldPtr WorldPtr = FAutomationWorld::CreateGameWorld();
		UTEST_TRUE("World doesnt have game mode", WorldPtr->GetWorld()->GetAuthGameMode() == nullptr);
	}

	{
		// game mode is created when specified
		FAutomationWorldPtr WorldPtr = FAutomationWorld::CreateGameWorldWithGameInstance<AGameMode>();
		UTEST_TRUE("World contains game mode", WorldPtr->GetWorld()->GetAuthGameMode() != nullptr);
		UTEST_TRUE("Game mode matches requested", WorldPtr->GetWorld()->GetAuthGameMode()->IsA<AGameMode>());
	}

	{
		// game instance creation functionality
		FAutomationWorldPtr WorldPtr = FAutomationWorld::CreateGameWorldWithGameInstance();
		const UGameInstance* GameInstance = WorldPtr->GetWorld()->GetGameInstance();
	
		UTEST_TRUE("World has game instance", IsValid(GameInstance));
		// game instance is guaranteed. If project default GI doesn't implement required interface, UAutomationGameInstance is selected
		UTEST_TRUE("Game instance supports auto tests", GameInstance->Implements<UGameInstanceAutomationSupport>());
	}

	{
		// player creation functionality
		FAutomationWorldPtr WorldPtr = FAutomationWorld::CreateGameWorldWithPlayer();
		const APlayerController* PC = UGameplayStatics::GetPlayerController(WorldPtr->GetWorld(), 0);

		UTEST_TRUE("World has first player controller", IsValid(PC));
		UTEST_TRUE("First player controller has a pawn", IsValid(PC->GetPawn()));
		UTEST_TRUE("World has first local player", IsValid(PC->GetLocalPlayer()));
	}
	
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationWorldTest_WorldSubsystem, "CommonAutomation.AutomationWorld.WorldSubsystem", AutomationTestFlags)

bool FAutomationWorldTest_WorldSubsystem::RunTest(const FString& Parameters)
{
	const FWorldInitParams Params{EWorldType::Game, EWorldInitFlags::InitScene};
	FAutomationWorldPtr WorldPtr = FAutomationWorld::CreateWorld(Params);
	
	UTestWorldSubsystem* Subsystem = WorldPtr->GetWorld()->GetSubsystem<UTestWorldSubsystem>();
	UTEST_FALSE("Test subsystem is not created", IsValid(Subsystem));

	// enable test subsystems
	UE::Private::bTestSubsystemEnabled = true;
	Subsystem = WorldPtr->GetOrCreateSubsystem<UTestWorldSubsystem>();
	UTEST_TRUE("Test subsystem is created", IsValid(Subsystem));
	UTEST_TRUE("Subsystem is initialized", Subsystem->bInitialized);
	UTEST_TRUE("Subsystem is post initialized", Subsystem->bPostInitialized);

	UTestWorldSubsystem* OtherSubsystem = WorldPtr->GetWorld()->GetSubsystem<UTestWorldSubsystem>();
	UTEST_EQUAL("Can receive world subsystem directly from world", Subsystem, OtherSubsystem);
		
	UTEST_FALSE("Subsystem has not begun play", Subsystem->bBeginPlayCalled);
	WorldPtr->RouteStartPlay();
	UTEST_TRUE("Subsystem has begun play", Subsystem->bBeginPlayCalled);

	WorldPtr->TickWorld(1);
	UTEST_TRUE("Subsystem updates streaming state", Subsystem->bStreamingStateUpdated);

	bool bDeinitialized = false;
	Subsystem->DeinitDelegate = FSimpleDelegate::CreateLambda([&bDeinitialized] { bDeinitialized = true; });

	WorldPtr.Reset();
	UTEST_TRUE("Subsystem is deinitialized", bDeinitialized);

	WorldPtr = Init(Params).EnableSubsystem<UTestWorldSubsystem>().Create();
	Subsystem = WorldPtr->GetSubsystem<UTestWorldSubsystem>();
	UTEST_TRUE("Test subsystem is created", IsValid(Subsystem));
	
	// disable test subsystems
	UE::Private::bTestSubsystemEnabled = false;
	
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationWorldTest_GameInstanceSubsystem, "CommonAutomation.AutomationWorld.GameInstanceSubsystem", AutomationTestFlags)

bool FAutomationWorldTest_GameInstanceSubsystem::RunTest(const FString& Parameters)
{
	FWorldInitParams Params{EWorldType::Game, EWorldInitFlags::InitScene | EWorldInitFlags::CreateGameInstance};
	FAutomationWorldPtr WorldPtr = FAutomationWorld::CreateWorld(Params);
	
	UTestGameInstanceSubsystem* Subsystem = WorldPtr->GetGameInstance()->GetSubsystem<UTestGameInstanceSubsystem>();
	UTEST_FALSE("Test subsystem is not created", IsValid(Subsystem));

	// enable test subsystems
	UE::Private::bTestSubsystemEnabled = true;
	Subsystem = WorldPtr->GetOrCreateSubsystem<UTestGameInstanceSubsystem>();
	UTEST_TRUE("Test subsystem is created", IsValid(Subsystem));
	UTEST_TRUE("Subsystem is initialized", Subsystem->bInitialized);
	
	UTestGameInstanceSubsystem* OtherSubsystem = WorldPtr->GetGameInstance()->GetSubsystem<UTestGameInstanceSubsystem>();
	UTEST_EQUAL("Can receive game instance subsystem directly from game instance", Subsystem, OtherSubsystem);

	bool bDeinitialized = false;
	Subsystem->DeinitDelegate = FSimpleDelegate::CreateLambda([&bDeinitialized] { bDeinitialized = true; });

	WorldPtr.Reset();
	UTEST_TRUE("Subsystem is deinitialized", bDeinitialized);

	WorldPtr = Init(Params).EnableSubsystem<UTestGameInstanceSubsystem>().Create();
	Subsystem = WorldPtr->GetSubsystem<UTestGameInstanceSubsystem>();
	UTEST_TRUE("Test subsystem is created", IsValid(Subsystem));

	// disable test subsystems
	UE::Private::bTestSubsystemEnabled = false;
	
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationWorld_NavigationSystemTest, "CommonAutomation.AutomationWorld.NavigationSystem", AutomationTestFlags)

bool FAutomationWorld_NavigationSystemTest::RunTest(const FString& Parameters)
{
	{
		FAutomationWorldPtr ScopedWorld = FAutomationWorld::CreateEditorWorld(EWorldInitFlags::InitScene | EWorldInitFlags::InitNavigation);

		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(*ScopedWorld);
		UTEST_TRUE("UCominoNavigationSystem is created for editor world", IsValid(NavSys));
		UTEST_TRUE("Navigation system is initialized for world", NavSys->IsInitialized() && NavSys->IsWorldInitDone());
	}

	{
		FAutomationWorldPtr ScopedWorld = FAutomationWorld::CreateGameWorld(EWorldInitFlags::InitScene | EWorldInitFlags::InitNavigation);

		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(*ScopedWorld);
		UTEST_TRUE("Navigation system is not initialized before BeginPlay", !NavSys->IsInitialized() && !NavSys->IsWorldInitDone());
		ScopedWorld->RouteStartPlay();
		UTEST_TRUE("Navigation system is initialized for world", NavSys->IsInitialized() && NavSys->IsWorldInitDone());
	}

	return !HasAnyErrors();
}

BEGIN_SIMPLE_AUTOMATION_TEST(FAutomationWorldFlagsTests, "CommonAutomation.AutomationWorld.Flags", AutomationTestFlags)
	void TestFlag(EWorldInitFlags Flag, TFunction<bool(UWorld*)> Pred);
END_SIMPLE_AUTOMATION_TEST(FAutomationWorldFlagsTests)

void FAutomationWorldFlagsTests::TestFlag(EWorldInitFlags Flag, TFunction<bool(UWorld*)> Pred)
{
	{
		auto WorldPtr = FAutomationWorld::CreateGameWorld(Flag);
		TestTrue(FString::Printf(TEXT("Flag %d works as expected"), static_cast<uint32>(Flag)), Pred(WorldPtr->GetWorld()));
	}

	{
		auto WorldPtr = FAutomationWorld::CreateGameWorld(EWorldInitFlags::None);
		TestFalse(FString::Printf(TEXT("Flag %d not specified"), static_cast<uint32>(Flag)), Pred(WorldPtr->GetWorld()));
	}
}

bool FAutomationWorldFlagsTests::RunTest(const FString& Parameters)
{
	TestFlag(EWorldInitFlags::StartPlay, [](UWorld* World) -> bool {
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->HasActorBegunPlay())
			{
				return false;
			}
		}
		return World->HasBegunPlay();
	});
	TestFlag(EWorldInitFlags::InitScene, [](UWorld* World) {
		return World->Scene != nullptr;
	});
	TestFlag(EWorldInitFlags::InitPhysics, [](UWorld* World) {
		return World->GetPhysicsScene() != nullptr;
	});
	TestFlag(EWorldInitFlags::InitHitProxy, [](UWorld* World) {
    	return World->Scene != nullptr && World->RequiresHitProxies();
    });
	TestFlag(EWorldInitFlags::InitCollision, [](UWorld* World) -> bool {
		return World->bEnableTraceCollision;
	});
	TestFlag(EWorldInitFlags::InitWeldedBodies, [](UWorld* World) -> bool {
		return World->bShouldSimulatePhysics;
	});
	TestFlag(EWorldInitFlags::InitNavigation, [](UWorld* World) {
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		return NavSys != nullptr;
	});
	TestFlag(EWorldInitFlags::InitAI, [](UWorld* World) {
		return World->GetAISystem() != nullptr;
	});
	TestFlag(EWorldInitFlags::InitAudio, [](UWorld* World) {
		return World->AllowAudioPlayback();
	});
	TestFlag(EWorldInitFlags::InitFX, [](UWorld* World) {
		return World->FXSystem != nullptr;
	});
	TestFlag(EWorldInitFlags::InitWorldPartition, [](UWorld* World) {
		return World->GetWorldPartition() != nullptr && World->GetWorldPartition()->IsInitialized();
	});

	return !HasAnyErrors();
}

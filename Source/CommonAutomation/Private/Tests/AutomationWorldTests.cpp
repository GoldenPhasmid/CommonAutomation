
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationWorldBehaviorTests, "TestFramework.AutomationWorld.Behavior", AutomationTestFlags)

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
		UTEST_TRUE("World has first local player", IsValid(PC->GetLocalPlayer()));
	}
	
	return !HasAnyErrors();
}

BEGIN_SIMPLE_AUTOMATION_TEST(FAutomationWorldFlagsTests, "TestFramework.AutomationWorld.Flags", AutomationTestFlags)
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



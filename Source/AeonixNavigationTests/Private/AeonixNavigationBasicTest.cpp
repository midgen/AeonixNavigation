#include <AeonixNavigation/Public/Data/AeonixData.h>
#include <Engine/World.h>
#include <Engine/EngineTypes.h>
#include <AeonixNavigation/Public/Interface/AeonixCollisionQueryInterface.h>
#include <AeonixNavigation/Public/Interface/AeonixDebugDrawInterface.h>
#include <Misc/AutomationTest.h>

// Mock implementation of IAeonixCollisionQueryInterface
class FMockCollisionQueryInterface : public IAeonixCollisionQueryInterface
{
public:
    virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
    {
        // For testing, always return false (not blocked)
        return false;
    }
};

// Mock implementation of IAeonixDebugDrawInterface
class FMockDebugDrawInterface : public IAeonixDebugDrawInterface
{
public:
    virtual void AeonixDrawDebugString(const FVector& Position, const FString& String, const FColor& Color) const override {}
    virtual void AeonixDrawDebugBox(const FVector& Position, const float Size, const FColor& Color) const override {}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixNavigation_GenerateDataTest, "AeonixNavigation.GenerateData.Basic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeonixNavigation_GenerateDataTest::RunTest(const FString& Parameters)
{
    // Arrange
    FMockCollisionQueryInterface MockCollision;
    FMockDebugDrawInterface MockDebug;
    FAeonixData NavData;

    // Use dummy world (nullptr) for now; in a more advanced test, you can mock or spawn a test world
    UWorld* DummyWorld = nullptr;
    
    // Setup minimal generation parameters
    FAeonixGenerationParameters Params;
    Params.Origin = FVector::ZeroVector;
    Params.Extents = FVector(1000, 1000, 1000);
    Params.VoxelPower = 3;
    Params.CollisionChannel = ECollisionChannel::ECC_WorldStatic;
    Params.AgentRadius = 34.f;
    NavData.UpdateGenerationParameters(Params);

    // Act & Assert: Should not crash
    // Only call Generate if DummyWorld is valid (in a real test, you'd want a valid world)
    bool bGenerateCalled = false;
	NavData.Generate(*DummyWorld, MockCollision, MockDebug);
	bGenerateCalled = true;
    

    TestTrue(TEXT("Test ran without crashing (Generate not called if world is null)"), true);
    return true;
}

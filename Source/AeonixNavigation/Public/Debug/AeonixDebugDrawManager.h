// Copyright Aeonix. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AeonixDebugDrawManager.generated.h"

/**
 * Categories for organizing debug primitives
 */
UENUM(BlueprintType)
enum class EAeonixDebugCategory : uint8
{
	Octree,
	Paths,
	Tests,
	General
};

/**
 * Internal structure for storing debug line data
 */
USTRUCT()
struct FAeonixDebugLine
{
	GENERATED_BODY()

	FVector Start;
	FVector End;
	FColor Color;
	float Thickness;
	EAeonixDebugCategory Category;

	FAeonixDebugLine()
		: Start(FVector::ZeroVector)
		, End(FVector::ZeroVector)
		, Color(FColor::White)
		, Thickness(0.f)
		, Category(EAeonixDebugCategory::General)
	{}

	FAeonixDebugLine(const FVector& InStart, const FVector& InEnd, const FColor& InColor, float InThickness, EAeonixDebugCategory InCategory)
		: Start(InStart)
		, End(InEnd)
		, Color(InColor)
		, Thickness(InThickness)
		, Category(InCategory)
	{}
};

/**
 * Internal structure for storing debug box data
 */
USTRUCT()
struct FAeonixDebugBox
{
	GENERATED_BODY()

	FVector Center;
	FVector Extent;
	FQuat Rotation;
	FColor Color;
	EAeonixDebugCategory Category;

	FAeonixDebugBox()
		: Center(FVector::ZeroVector)
		, Extent(FVector::ZeroVector)
		, Rotation(FQuat::Identity)
		, Color(FColor::White)
		, Category(EAeonixDebugCategory::General)
	{}

	FAeonixDebugBox(const FVector& InCenter, const FVector& InExtent, const FQuat& InRotation, const FColor& InColor, EAeonixDebugCategory InCategory)
		: Center(InCenter)
		, Extent(InExtent)
		, Rotation(InRotation)
		, Color(InColor)
		, Category(InCategory)
	{}
};

/**
 * Internal structure for storing debug sphere data
 */
USTRUCT()
struct FAeonixDebugSphere
{
	GENERATED_BODY()

	FVector Center;
	float Radius;
	int32 Segments;
	FColor Color;
	EAeonixDebugCategory Category;

	FAeonixDebugSphere()
		: Center(FVector::ZeroVector)
		, Radius(0.f)
		, Segments(12)
		, Color(FColor::White)
		, Category(EAeonixDebugCategory::General)
	{}

	FAeonixDebugSphere(const FVector& InCenter, float InRadius, int32 InSegments, const FColor& InColor, EAeonixDebugCategory InCategory)
		: Center(InCenter)
		, Radius(InRadius)
		, Segments(InSegments)
		, Color(InColor)
		, Category(InCategory)
	{}
};

/**
 * Internal structure for storing debug arrow data
 */
USTRUCT()
struct FAeonixDebugArrow
{
	GENERATED_BODY()

	FVector Start;
	FVector End;
	float ArrowSize;
	FColor Color;
	float Thickness;
	EAeonixDebugCategory Category;

	FAeonixDebugArrow()
		: Start(FVector::ZeroVector)
		, End(FVector::ZeroVector)
		, ArrowSize(20.f)
		, Color(FColor::White)
		, Thickness(0.f)
		, Category(EAeonixDebugCategory::General)
	{}

	FAeonixDebugArrow(const FVector& InStart, const FVector& InEnd, float InArrowSize, const FColor& InColor, float InThickness, EAeonixDebugCategory InCategory)
		: Start(InStart)
		, End(InEnd)
		, ArrowSize(InArrowSize)
		, Color(InColor)
		, Thickness(InThickness)
		, Category(InCategory)
	{}
};

/**
 * Internal structure for storing debug string data
 */
USTRUCT()
struct FAeonixDebugString
{
	GENERATED_BODY()

	FVector Location;
	FString Text;
	FColor Color;
	float Scale;
	EAeonixDebugCategory Category;

	FAeonixDebugString()
		: Location(FVector::ZeroVector)
		, Text(TEXT(""))
		, Color(FColor::White)
		, Scale(1.f)
		, Category(EAeonixDebugCategory::General)
	{}

	FAeonixDebugString(const FVector& InLocation, const FString& InText, const FColor& InColor, float InScale, EAeonixDebugCategory InCategory)
		: Location(InLocation)
		, Text(InText)
		, Color(InColor)
		, Scale(InScale)
		, Category(InCategory)
	{}
};

/**
 * World subsystem for managing Aeonix Navigation debug drawing
 * Provides isolated debug visualization that doesn't interfere with other systems
 *
 * Uses persistent debug lines for performance - primitives are drawn once when added
 * and only redrawn when a category is cleared.
 */
UCLASS()
class AEONIXNAVIGATION_API UAeonixDebugDrawManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Subsystem lifecycle
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Add debug primitives
	void AddLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness = 0.f, EAeonixDebugCategory Category = EAeonixDebugCategory::General);
	void AddBox(const FVector& Center, const FVector& Extent, const FQuat& Rotation, const FColor& Color, EAeonixDebugCategory Category = EAeonixDebugCategory::General);
	void AddSphere(const FVector& Center, float Radius, int32 Segments, const FColor& Color, EAeonixDebugCategory Category = EAeonixDebugCategory::General);
	void AddArrow(const FVector& Start, const FVector& End, float ArrowSize, const FColor& Color, float Thickness = 0.f, EAeonixDebugCategory Category = EAeonixDebugCategory::General);
	void AddString(const FVector& Location, const FString& Text, const FColor& Color, float Scale = 1.f, EAeonixDebugCategory Category = EAeonixDebugCategory::General);

	// Clear debug primitives
	void Clear(EAeonixDebugCategory Category);
	void ClearAll();

	// Enable/disable rendering
	UFUNCTION(BlueprintCallable, Category = "Aeonix|Debug")
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Aeonix|Debug")
	bool IsEnabled() const { return bEnabled; }

	// Category visibility
	UFUNCTION(BlueprintCallable, Category = "Aeonix|Debug")
	void SetCategoryVisible(EAeonixDebugCategory Category, bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Aeonix|Debug")
	bool IsCategoryVisible(EAeonixDebugCategory Category) const;

private:
	// Storage for debug primitives
	UPROPERTY()
	TArray<FAeonixDebugLine> Lines;

	UPROPERTY()
	TArray<FAeonixDebugBox> Boxes;

	UPROPERTY()
	TArray<FAeonixDebugSphere> Spheres;

	UPROPERTY()
	TArray<FAeonixDebugArrow> Arrows;

	UPROPERTY()
	TArray<FAeonixDebugString> Strings;

	// Visibility flags
	UPROPERTY()
	TMap<EAeonixDebugCategory, bool> CategoryVisibility;

	// Master enable flag
	UPROPERTY()
	bool bEnabled = true;

	// Helper to redraw all stored primitives as persistent lines
	void RedrawAllPrimitives();
};

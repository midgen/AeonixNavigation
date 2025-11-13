// Copyright Aeonix. All Rights Reserved.

#include "Debug/AeonixDebugDrawManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

void UAeonixDebugDrawManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Initialize category visibility (all visible by default)
	CategoryVisibility.Add(EAeonixDebugCategory::Octree, true);
	CategoryVisibility.Add(EAeonixDebugCategory::Paths, true);
	CategoryVisibility.Add(EAeonixDebugCategory::Tests, true);
	CategoryVisibility.Add(EAeonixDebugCategory::General, true);
}

void UAeonixDebugDrawManager::Deinitialize()
{
	ClearAll();
	Super::Deinitialize();
}

void UAeonixDebugDrawManager::AddLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness, EAeonixDebugCategory Category)
{
	UWorld* World = GetWorld();
	if (!World || !bEnabled || !IsCategoryVisible(Category))
	{
		return;
	}

	Lines.Add(FAeonixDebugLine(Start, End, Color, Thickness, Category));

	// Draw immediately as persistent line
	DrawDebugLine(World, Start, End, Color, true, -1.f, 0, Thickness);
}

void UAeonixDebugDrawManager::AddBox(const FVector& Center, const FVector& Extent, const FQuat& Rotation, const FColor& Color, EAeonixDebugCategory Category)
{
	UWorld* World = GetWorld();
	if (!World || !bEnabled || !IsCategoryVisible(Category))
	{
		return;
	}

	Boxes.Add(FAeonixDebugBox(Center, Extent, Rotation, Color, Category));

	// Draw immediately as persistent box
	DrawDebugBox(World, Center, Extent, Rotation, Color, true, -1.f);
}

void UAeonixDebugDrawManager::AddSphere(const FVector& Center, float Radius, int32 Segments, const FColor& Color, EAeonixDebugCategory Category)
{
	UWorld* World = GetWorld();
	if (!World || !bEnabled || !IsCategoryVisible(Category))
	{
		return;
	}

	Spheres.Add(FAeonixDebugSphere(Center, Radius, Segments, Color, Category));

	// Draw immediately as persistent sphere
	DrawDebugSphere(World, Center, Radius, Segments, Color, true, -1.f);
}

void UAeonixDebugDrawManager::AddArrow(const FVector& Start, const FVector& End, float ArrowSize, const FColor& Color, float Thickness, EAeonixDebugCategory Category)
{
	UWorld* World = GetWorld();
	if (!World || !bEnabled || !IsCategoryVisible(Category))
	{
		return;
	}

	Arrows.Add(FAeonixDebugArrow(Start, End, ArrowSize, Color, Thickness, Category));

	// Draw immediately as persistent arrow
	DrawDebugDirectionalArrow(World, Start, End, ArrowSize, Color, true, -1.f, 0, Thickness);
}

void UAeonixDebugDrawManager::AddString(const FVector& Location, const FString& Text, const FColor& Color, float Scale, EAeonixDebugCategory Category)
{
	UWorld* World = GetWorld();
	if (!World || !bEnabled || !IsCategoryVisible(Category))
	{
		return;
	}

	Strings.Add(FAeonixDebugString(Location, Text, Color, Scale, Category));

	// Draw immediately as persistent string
	DrawDebugString(World, Location, Text, nullptr, Color, -1.f, false, Scale);
}

void UAeonixDebugDrawManager::Clear(EAeonixDebugCategory Category)
{
	// Remove primitives of the specified category
	Lines.RemoveAll([Category](const FAeonixDebugLine& Line) { return Line.Category == Category; });
	Boxes.RemoveAll([Category](const FAeonixDebugBox& Box) { return Box.Category == Category; });
	Spheres.RemoveAll([Category](const FAeonixDebugSphere& Sphere) { return Sphere.Category == Category; });
	Arrows.RemoveAll([Category](const FAeonixDebugArrow& Arrow) { return Arrow.Category == Category; });
	Strings.RemoveAll([Category](const FAeonixDebugString& String) { return String.Category == Category; });

	// Flush all persistent debug lines and redraw remaining categories
	// Note: This will temporarily clear other systems' debug lines, but it's a one-time cost
	// when clearing a category, not a per-frame cost
	if (UWorld* World = GetWorld())
	{
		FlushPersistentDebugLines(World);
		RedrawAllPrimitives();
	}
}

void UAeonixDebugDrawManager::ClearAll()
{
	Lines.Empty();
	Boxes.Empty();
	Spheres.Empty();
	Arrows.Empty();
	Strings.Empty();

	// Flush all persistent debug lines
	if (UWorld* World = GetWorld())
	{
		FlushPersistentDebugLines(World);
	}
}

void UAeonixDebugDrawManager::RedrawAllPrimitives()
{
	if (!bEnabled)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Redraw all stored primitives as persistent lines
	for (const FAeonixDebugLine& Line : Lines)
	{
		if (IsCategoryVisible(Line.Category))
		{
			DrawDebugLine(World, Line.Start, Line.End, Line.Color, true, -1.f, 0, Line.Thickness);
		}
	}

	for (const FAeonixDebugBox& Box : Boxes)
	{
		if (IsCategoryVisible(Box.Category))
		{
			DrawDebugBox(World, Box.Center, Box.Extent, Box.Rotation, Box.Color, true, -1.f);
		}
	}

	for (const FAeonixDebugSphere& Sphere : Spheres)
	{
		if (IsCategoryVisible(Sphere.Category))
		{
			DrawDebugSphere(World, Sphere.Center, Sphere.Radius, Sphere.Segments, Sphere.Color, true, -1.f);
		}
	}

	for (const FAeonixDebugArrow& Arrow : Arrows)
	{
		if (IsCategoryVisible(Arrow.Category))
		{
			DrawDebugDirectionalArrow(World, Arrow.Start, Arrow.End, Arrow.ArrowSize, Arrow.Color, true, -1.f, 0, Arrow.Thickness);
		}
	}

	for (const FAeonixDebugString& String : Strings)
	{
		if (IsCategoryVisible(String.Category))
		{
			DrawDebugString(World, String.Location, String.Text, nullptr, String.Color, -1.f, false, String.Scale);
		}
	}
}

void UAeonixDebugDrawManager::SetCategoryVisible(EAeonixDebugCategory Category, bool bVisible)
{
	bool bOldValue = IsCategoryVisible(Category);
	CategoryVisibility.FindOrAdd(Category) = bVisible;

	// If visibility changed, flush and redraw to reflect the change
	if (bOldValue != bVisible)
	{
		if (UWorld* World = GetWorld())
		{
			FlushPersistentDebugLines(World);
			RedrawAllPrimitives();
		}
	}
}

bool UAeonixDebugDrawManager::IsCategoryVisible(EAeonixDebugCategory Category) const
{
	const bool* Visible = CategoryVisibility.Find(Category);
	return Visible ? *Visible : true;
}

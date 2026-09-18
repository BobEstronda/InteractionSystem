// Copyright 2025 Alessandro Cristoffer

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

// http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "InteractableComponent.h"
#include "InteractionBlockerComponent.h"
#include "InteractionSubsystem.generated.h"

class UPlayerInteractionComponent;

INTERACTIONSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogInteractionSystem, Log, All);

inline FBoxSphereBounds GetRobustActorBounds(const AActor* Actor, bool bPreferFullActorBounds = false)
{
	if (!Actor)
	{
		return FBoxSphereBounds(FVector::ZeroVector, FVector(1.f, 1.f, 1.f), 1.f);
	}

	if (!bPreferFullActorBounds && Actor->GetRootComponent())
	{
		const FBoxSphereBounds RootBounds = Actor->GetRootComponent()->Bounds;
		if (!RootBounds.BoxExtent.IsNearlyZero())
		{
			return RootBounds;
		}

		
		UE_LOG(LogInteractionSystem, Warning,
			TEXT("'%s': root component bounds are ~zero (likely an empty scene-root actor with mesh children). ")
			TEXT("Falling back to full actor bounds. Consider enabling 'Use Full Actor Bounds' on its ")
			TEXT("InteractionBlockerComponent, or making the mesh component the actor's root."),
			*Actor->GetName());
		
	}

	return FBoxSphereBounds(Actor->GetComponentsBoundingBox());
}

// Interactable Octree

struct FInteractableOctreeElement
{
	FGuid UniqueID;

	FOctreeElementId ElementId;

	UInteractableComponent* Interactable;
	FBoxSphereBounds Bounds;

	FInteractableOctreeElement() : UniqueID(FGuid::NewGuid())
	{
		Interactable = nullptr;
		Bounds = FBoxSphereBounds(FVector(0.0f, 0.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f), 1.0f);
	}

	FInteractableOctreeElement(UInteractableComponent* InInteractable) : Interactable(InInteractable)
	{
		Bounds = GetRobustActorBounds(Interactable->GetOwner());
	}
};

inline bool operator==(const FInteractableOctreeElement& first, const FInteractableOctreeElement& second)
{
	return (first.UniqueID == second.UniqueID);
}

inline uint32 GetTypeHash(const FInteractableOctreeElement& other)
{
	return GetTypeHash(other.UniqueID);
}

struct FInteractableOctreeSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>;
	static TMap<FInteractableOctreeElement, FOctreeElementId2> OctreeIds;

	static FORCEINLINE const FBoxSphereBounds& GetBoundingBox(const FInteractableOctreeElement& Element)
	{
		return Element.Bounds;
	}

	static FORCEINLINE bool AreElementsEqual(const FInteractableOctreeElement& A, const FInteractableOctreeElement& B)
	{
		return A.Interactable == B.Interactable;
	}

	static void SetElementId(const FInteractableOctreeElement& Element, FOctreeElementId2 Id)
	{
		OctreeIds.Add(Element, Id);
	}
};

using FInteractableOctree = TOctree2<FInteractableOctreeElement, FInteractableOctreeSemantics>;

// Blocker Octree : Static geometry, 3D assets to can occlude Interactable 

struct FBlockerOctreeElement
{
	FGuid UniqueID;

	TWeakObjectPtr<UInteractionBlockerComponent> Blocker;
	FBoxSphereBounds Bounds;

	FBlockerOctreeElement() : UniqueID(FGuid::NewGuid())
	{
		Bounds = FBoxSphereBounds(FVector::ZeroVector, FVector(1.0f, 1.0f, 1.0f), 1.0f);
	}

	FBlockerOctreeElement(UInteractionBlockerComponent* InBlocker)
		: UniqueID(FGuid::NewGuid()), Blocker(InBlocker)
	{
		Bounds = InBlocker->GetBlockerBounds();
	}
};

inline bool operator==(const FBlockerOctreeElement& first, const FBlockerOctreeElement& second)
{
	return (first.UniqueID == second.UniqueID);
}

inline uint32 GetTypeHash(const FBlockerOctreeElement& other)
{
	return GetTypeHash(other.UniqueID);
}

struct FBlockerOctreeSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>;
	static TMap<FBlockerOctreeElement, FOctreeElementId2> OctreeIds;

	static FORCEINLINE const FBoxSphereBounds& GetBoundingBox(const FBlockerOctreeElement& Element)
	{
		return Element.Bounds;
	}

	static FORCEINLINE bool AreElementsEqual(const FBlockerOctreeElement& A, const FBlockerOctreeElement& B)
	{
		return A.UniqueID == B.UniqueID;
	}

	static void SetElementId(const FBlockerOctreeElement& Element, FOctreeElementId2 Id)
	{
		OctreeIds.Add(Element, Id);
	}
};

using FBlockerOctree = TOctree2<FBlockerOctreeElement, FBlockerOctreeSemantics>;

// Aim Mode : first/third person and top down

UENUM(BlueprintType)
enum class EInteractionAimMode : uint8
{
	CameraForward,
	PawnForward,

	// From a mouse cursor deprojected onto the ground plane
	ExternalDirection
};

// Debbug 

struct FInteractionFrameStats
{
	// Whole-system cost for this frame.
	double TickTotalMicroseconds = 0.0;
	double UpdateInteractionsMicroseconds = 0.0;

	// Per-phase totals across ALL players/candidates evaluated this frame.
	double OctreeQueryMicroseconds = 0.0;
	double ScoreCalculationMicroseconds = 0.0;
	double OcclusionTestMicroseconds = 0.0;
	double OverlapTestMicroseconds = 0.0;
	double ViewVisibilityMicroseconds = 0.0;

	// Call counts, so cost-per-call can be derived (Total / Count).
	int32 NumPlayersProcessed = 0;
	int32 NumOctreeQueries = 0;
	int32 NumCandidatesEvaluated = 0;
	int32 NumScoreCalculations = 0;
	int32 NumOcclusionTests = 0;
	int32 NumOcclusionBlocked = 0;
	int32 NumOverlapTests = 0;
	int32 NumViewVisibilityTests = 0;

	// Registered object counts (cheap, informative for scaling analysis).
	int32 NumRegisteredInteractables = 0;
	int32 NumRegisteredBlockers = 0;
	int32 NumRegisteredPlayers = 0;

	double AvgMicrosecondsPerPlayer() const { return NumPlayersProcessed > 0 ? TickTotalMicroseconds / NumPlayersProcessed : 0.0; }
	double AvgMicrosecondsPerScore() const { return NumScoreCalculations > 0 ? ScoreCalculationMicroseconds / NumScoreCalculations : 0.0; }
	double AvgMicrosecondsPerOcclusionTest() const { return NumOcclusionTests > 0 ? OcclusionTestMicroseconds / NumOcclusionTests : 0.0; }
	double AvgMicrosecondsPerOverlapTest() const { return NumOverlapTests > 0 ? OverlapTestMicroseconds / NumOverlapTests : 0.0; }
	double AvgMicrosecondsPerOctreeQuery() const { return NumOctreeQueries > 0 ? OctreeQueryMicroseconds / NumOctreeQueries : 0.0; }
};

UCLASS()
class INTERACTIONSYSTEM_API UInteractionSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<UPlayerInteractionComponent*> RegisteredPlayers;

	float MaxRelevanceDistance;
	float MaxInteractionDistance;
	float OverlapBoundsPadding;
	float CenterScreenBias;
	float ViewAngleThreshold;
	float MinScreenAngleThreshold;
	float MaxScreenAngleThreshold;

	bool bUseGeometryOcclusion = true;

	TMap<UInteractableComponent*, bool> PreviousOverlapStates;

	TUniquePtr<FInteractableOctree> InteractableOctree;
	TUniquePtr<FBlockerOctree> BlockerOctree;

	TMap<UInteractableComponent*, FInteractableOctreeElement> RegisteredElements;
	TMap<UInteractionBlockerComponent*, FBlockerOctreeElement> RegisteredBlockerElements;

	// Debbug
	mutable FInteractionFrameStats CurrentFrameStats;
	FInteractionFrameStats LastFrameStats;

public:
	// Subsystem 
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	// Interaction Subsystem
	//
	// Interactables
	void RegisterInteractable(UInteractableComponent* InteractableComponent);
	void UnregisterInteractable(UInteractableComponent* InteractableComponent);

	// Occlusion blockers
	void RegisterBlocker(UInteractionBlockerComponent* BlockerComponent);
	void UnregisterBlocker(UInteractionBlockerComponent* BlockerComponent);
	void UpdateBlockerBounds(UInteractionBlockerComponent* BlockerComponent);

	// Player
	void RegisterPlayer(UPlayerInteractionComponent* PlayerComponent);
	void UnregisterPlayer(UPlayerInteractionComponent* PlayerComponent);

	UInteractableComponent* GetBestInteractableForPlayer(UPlayerInteractionComponent* PlayerComponent);

	void UpdateInteractableBounds(UInteractableComponent* InteractableComponent);

	// Debbug
	const FInteractionFrameStats& GetLastFrameStats() const { return LastFrameStats; }

private:
	void UpdateInteractions();
	float CalculateInteractionScore(UPlayerInteractionComponent* Player, UInteractableComponent* Interactable);
	bool IsInViewAndVisible(const UInteractableComponent * Interactable, const float Dot) const;
	bool IsOcclusionBlocked(const FVector& From, const FVector& To, const UInteractableComponent* Ignore) const;
	bool IsOverlappingBounds(const UPlayerInteractionComponent* Player, const UInteractableComponent* Interactable) const;
	void GetInteractionAimRay(const UPlayerInteractionComponent* Player, FVector& OutOrigin, FVector& OutDirection) const;
	static bool SegmentIntersectsAABB(const FVector& Start, const FVector& End, const FBox& Box);

	// Debbug
	// console command: interaction.debug.registered
	// interactable (cyan)
	// blocker (orange)
	//
	void DrawDebugRegisteredBounds() const;

	// console command: interaction.debug.stats
	void DrawDebugStatsOnScreen() const;
};

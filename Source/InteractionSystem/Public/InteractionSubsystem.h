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
#include "InteractionSubsystem.generated.h"

//class UInteractableComponent;
class UPlayerInteractionComponent;

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
		Bounds = Interactable->GetOwner()->GetRootComponent()->Bounds;
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

UCLASS()
class INTERACTIONSYSTEM_API UInteractionSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
	
	/*UPROPERTY()
	TArray<UInteractableComponent*> RegisteredInteractables;*/

	UPROPERTY()
	TArray<UPlayerInteractionComponent*> RegisteredPlayers;

	float MaxRelevanceDistance = 800.f;
	float MaxInteractionDistance = 300.f;
	float OverlapThreshold = 150.f;
	float CenterScreenBias = 2.f;
	float ViewAngleThreshold = 40.f;
	float MinScreenAngleThreshold = 10.f;
	float MaxScreenAngleThreshold = 30.f;

	TMap<UInteractableComponent*, bool> PreviousOverlapStates;

	TUniquePtr<FInteractableOctree> InteractableOctree;

	TMap<UInteractableComponent*, FInteractableOctreeElement> RegisteredElements;

public:
	// Subsystem 
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	// InteractionSubsystem
	void RegisterInteractable(UInteractableComponent* InteractableComponent);
	void UnregisterInteractable(UInteractableComponent* InteractableComponent);

	void RegisterPlayer(UPlayerInteractionComponent* PlayerComponent);
	void UnregisterPlayer(UPlayerInteractionComponent* PlayerComponent);

	UInteractableComponent* GetBestInteractableForPlayer(UPlayerInteractionComponent* PlayerComponent);

private:
	void UpdateInteractions();
	float CalculateInteractionScore(UPlayerInteractionComponent* Player, UInteractableComponent* Interactable);
	bool IsInViewAndVisible(const UInteractableComponent * Interactable, const float Dot) const;
};

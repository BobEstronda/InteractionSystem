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


#include "InteractionSubsystem.h"

#include "PlayerInteractionComponent.h"
#include "InteractableComponent.h"

//DECLARE_CYCLE_STAT(TEXT("Update Interactions"), STAT_UpdateInteractions, STATGROUP_Game);
//DECLARE_CYCLE_STAT(TEXT("Interaction Score"), STAT_CalculateInteractionScore, STATGROUP_Game);

TMap<FInteractableOctreeElement, FOctreeElementId2> FInteractableOctreeSemantics::OctreeIds;

void UInteractionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Warning, TEXT("InteractionSubsystem Initialized"));

	FBox WorldBounds(FVector(-100000), FVector(100000));
	InteractableOctree = MakeUnique<FInteractableOctree>(WorldBounds.GetCenter(), WorldBounds.GetExtent().X);
}

void UInteractionSubsystem::Deinitialize()
{
	//RegisteredInteractables.Empty();
	RegisteredPlayers.Empty();
	RegisteredElements.Empty();
	PreviousOverlapStates.Empty();

	UE_LOG(LogTemp, Warning, TEXT("InteractionSubsystem Deinitialize"));

	Super::Deinitialize();
}

void UInteractionSubsystem::Tick(float DeltaTime)
{
	UpdateInteractions();
}

bool UInteractionSubsystem::IsTickable() const
{
	return !IsTemplate() && GetWorld() && !GetWorld()->IsNetMode(NM_DedicatedServer);
}

TStatId UInteractionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UInteractionSubsystem, STATGROUP_Tickables);
}

void UInteractionSubsystem::RegisterInteractable(UInteractableComponent* InteractableComponent)
{
	if (InteractableComponent && !RegisteredElements.Contains(InteractableComponent))
	{
		//RegisteredInteractables.Add(InteractableComponent);
		FInteractableOctreeElement InteractableElement = FInteractableOctreeElement(InteractableComponent);
		InteractableOctree->AddElement(InteractableElement);
		RegisteredElements.Add(InteractableComponent, InteractableElement);
	}
}

void UInteractionSubsystem::UnregisterInteractable(UInteractableComponent* InteractableComponent)
{
	if (InteractableComponent)
	{
		FInteractableOctreeElement& InteractableElement = *RegisteredElements.Find(InteractableComponent);
		if (FInteractableOctreeSemantics::OctreeIds.Contains(InteractableElement))
		{
			FOctreeElementId2* ElementId2 = FInteractableOctreeSemantics::OctreeIds.Find(InteractableElement);
			if (ElementId2)
			{
				InteractableOctree->RemoveElement(*ElementId2);
				FInteractableOctreeSemantics::OctreeIds.Remove(InteractableElement);
			}
		}

		//RegisteredInteractables.Remove(InteractableComponent);
		PreviousOverlapStates.Remove(InteractableComponent);
		RegisteredElements.Remove(InteractableComponent);

		for (UPlayerInteractionComponent* Player : RegisteredPlayers)
		{
			if (Player && Player->GetCurrentBestInteractable() == InteractableComponent)
			{
				Player->SetBestInteractable(nullptr);
			}
		}
	}
}

void UInteractionSubsystem::RegisterPlayer(UPlayerInteractionComponent* PlayerComponent)
{
	if (PlayerComponent && !RegisteredPlayers.Contains(PlayerComponent))
	{
		RegisteredPlayers.Add(PlayerComponent);
	}
}

void UInteractionSubsystem::UnregisterPlayer(UPlayerInteractionComponent* PlayerComponent)
{
	if (PlayerComponent)
	{
		RegisteredPlayers.Remove(PlayerComponent);
	}
}

UInteractableComponent* UInteractionSubsystem::GetBestInteractableForPlayer(UPlayerInteractionComponent* PlayerComponent)
{
	if (!IsValid(PlayerComponent)) return nullptr;

	UInteractableComponent* BestInteractable = nullptr;
	float BestScore = 0.f;

	FBox QueryBox = FBox::BuildAABB(PlayerComponent->GetOwner()->GetActorLocation(), FVector(1000.f));

	TArray<FInteractableOctreeElement> NearbyElements;
	InteractableOctree->FindElementsWithBoundsTest(QueryBox,
		[&NearbyElements](const FInteractableOctreeElement& Element)
		{
			NearbyElements.Add(Element);
		});
	
	for (FInteractableOctreeElement OctreeElement : NearbyElements)
	{
		if (!IsValid(OctreeElement.Interactable) || !OctreeElement.Interactable->bCanInteract)
		{
			continue;
		}

		float Score = CalculateInteractionScore(PlayerComponent, OctreeElement.Interactable);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestInteractable = OctreeElement.Interactable;
		}
	}

	return BestInteractable;
}

void UInteractionSubsystem::UpdateInteractions()
{
	//SCOPE_CYCLE_COUNTER(STAT_UpdateInteractions);

	//RegisteredInteractables.RemoveAll([](UInteractableComponent* Comp) { return !IsValid(Comp); });
	RegisteredPlayers.RemoveAll([](UPlayerInteractionComponent* Comp) { return !IsValid(Comp); });

	for (UPlayerInteractionComponent* Player : RegisteredPlayers)
	{
		if (!IsValid(Player))
		{
			continue;
		}

		UInteractableComponent* NewBest = GetBestInteractableForPlayer(Player);

		if (Player->GetCurrentBestInteractable() != NewBest)
		{
			Player->SetBestInteractable(NewBest);
		}
	}
}

float UInteractionSubsystem::CalculateInteractionScore(UPlayerInteractionComponent* Player, UInteractableComponent* Interactable)
{
	//SCOPE_CYCLE_COUNTER(STAT_CalculateInteractionScore);

	if (!Player || !Interactable) return 0.f;
	
	const FVector PlayerLocation = Player->GetCameraLocation();
	const FVector PlayerForward = Player->GetCameraForward();
	const FVector InteractableLocation = Interactable->GetInteractionLocation();

	const float DistSq = FVector::DistSquared(PlayerLocation, InteractableLocation);

	static const float RelevanceDistanceSq = FMath::Square(MaxRelevanceDistance);

	if (DistSq > RelevanceDistanceSq)
	{
		Interactable->SetRelevance(false);
		return 0.f;
	}

	const bool bShouldBeOverlapping = (DistSq < FMath::Square(OverlapThreshold));
	bool& bPreviouslyOverlapping = PreviousOverlapStates.FindOrAdd(Interactable);

	if (bPreviouslyOverlapping != bShouldBeOverlapping)
	{
		if (bShouldBeOverlapping)
		{
			Interactable->OnReceivedOverlap(Player->GetOwner());
		}
		else 
		{
			Interactable->OnReceivedEndOverlap(Player->GetOwner());
		}
		bPreviouslyOverlapping = bShouldBeOverlapping;
	}

	const FVector ToInteractable = InteractableLocation - PlayerLocation;
	const float InvLen = FMath::InvSqrt(ToInteractable.SizeSquared());
	const FVector DirectionToInteractable = ToInteractable * InvLen;

	const float DotProduct = FVector::DotProduct(PlayerForward, DirectionToInteractable);

	const bool bIsInView = IsInViewAndVisible(Interactable, DotProduct);
	Interactable->SetRelevance(bIsInView);

	if (!bIsInView) return 0.f;

	static const float InteractionDistanceSq = FMath::Square(MaxInteractionDistance);

	if ( DistSq > InteractionDistanceSq) return 0.f;

	// Base score inversely proportional to distance
	float BaseScore = 1.f - ( DistSq / InteractionDistanceSq);

	static const float MinScreenDotThreshold = FMath::Cos(FMath::DegreesToRadians(MinScreenAngleThreshold));
	static const float MaxScreenDotThreshold = FMath::Cos(FMath::DegreesToRadians(MaxScreenAngleThreshold));

	// Apply center screen bias
	float CenterBias = 1.f;
	if (DotProduct > MinScreenDotThreshold)
	{
		CenterBias = CenterScreenBias;
	}
	else if (DotProduct > MaxScreenDotThreshold)
	{
		CenterBias = 1.5f;
	}

	return BaseScore * CenterBias;
}

bool UInteractionSubsystem::IsInViewAndVisible(const UInteractableComponent * Interactable, const float Dot) const
{
	if (!Interactable) return false;

	static const float ViewDotThreshold = FMath::Cos(FMath::DegreesToRadians(ViewAngleThreshold));

	if (Dot < ViewDotThreshold) return false;

	const AActor* InteractableActor = Interactable->GetOwner();
	if (InteractableActor)
	{
		UPrimitiveComponent* PrimitiveComponent = InteractableActor->FindComponentByClass<UPrimitiveComponent>();
		if (PrimitiveComponent)
		{
			//FPrimitiveSceneInfoData& SceneInfoData = PrimitiveComponent->GetSceneData();
			const bool bIsVisible = (GetWorld()->GetTimeSeconds() - /*SceneInfoData.LastRenderTimeOnScreen*/ PrimitiveComponent->GetLastRenderTimeOnScreen()) <= .05f;
			if (bIsVisible)
			{
				return true;
			}
		}
	}

	return false; 
}

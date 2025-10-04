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


#include "InteractableComponent.h"

#include "InteractionSubsystem.h"

UInteractableComponent::UInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UInteractableComponent::BeginPlay()
{
	Super::BeginPlay();

    if (UWorld* World = GetWorld())
    {
        if (UInteractionSubsystem* InteractionSubsystem = World->GetSubsystem<UInteractionSubsystem>())
        {
            InteractionSubsystem->RegisterInteractable(this);
        }
    }
}

void UInteractableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UInteractionSubsystem* InteractionSubsystem = World->GetSubsystem<UInteractionSubsystem>())
        {
            InteractionSubsystem->UnregisterInteractable(this);
        }
    }

	Super::EndPlay(EndPlayReason);
}

void UInteractableComponent::SetRelevance(const bool bRelevant)
{
    if (bRelevant == bIsRelevant) return;

    bIsRelevant = bRelevant;

    OnInteractionStateChanged.Broadcast(bIsRelevant);
}

FVector UInteractableComponent::GetInteractionLocation() const
{
    if (GetOwner())
    {
        return GetOwner()->GetActorLocation();
    }

    return FVector::ZeroVector;
}

void UInteractableComponent::OnInteract_Implementation(AActor* InteractingActor)
{
    OnInteraction.Broadcast(InteractingActor);
}

void UInteractableComponent::OnReceivedOverlap_Implementation(AActor* InteractingActor)
{
    if (bIsOverlapping) return;

    //UE_LOG(LogTemp, Warning, TEXT("OnReceivedOverlap: %s"), *InteractingActor->GetName());

    bIsOverlapping = true;
    OnOverlap.Broadcast(InteractingActor);

    //UE_LOG(LogTemp, Warning, TEXT("OnOverlap.Broadcast chamado!"));
}

void UInteractableComponent::OnReceivedEndOverlap_Implementation(AActor* InteractingActor)
{
    if (!bIsOverlapping) return;

    //UE_LOG(LogTemp, Warning, TEXT("OnReceivedEndOverlap: %s"), *InteractingActor->GetName());

    bIsOverlapping = false;
    OnEndOverlap.Broadcast(InteractingActor);

    //UE_LOG(LogTemp, Warning, TEXT("OnEndOverlap.Broadcast chamado!"));
}


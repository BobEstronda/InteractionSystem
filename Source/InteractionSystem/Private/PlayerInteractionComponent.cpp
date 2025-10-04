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


#include "PlayerInteractionComponent.h"

#include "InteractionSubsystem.h"
#include "InteractableComponent.h"
#include "Camera/CameraComponent.h"

UPlayerInteractionComponent::UPlayerInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPlayerInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	
    OwnerPawn = Cast<APawn>(GetOwner());

    if (OwnerPawn)
    {
        CameraComponent = OwnerPawn->FindComponentByClass<UCameraComponent>();
    }

    if (UWorld* World = GetWorld())
    {
        if (UInteractionSubsystem* InteractionSubsystem = World->GetSubsystem<UInteractionSubsystem>())
        {
            InteractionSubsystem->RegisterPlayer(this);
        }
    }
}

void UPlayerInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UInteractionSubsystem* InteractionSubsystem = World->GetSubsystem<UInteractionSubsystem>())
        {
            InteractionSubsystem->UnregisterPlayer(this);
        }
    }

	Super::EndPlay(EndPlayReason);
}

void UPlayerInteractionComponent::TryInteract()
{
    if (CurrentBestInteractable && CurrentBestInteractable->bCanInteract)
    {
        CurrentBestInteractable->OnInteract(GetOwner());
    }
}

FVector UPlayerInteractionComponent::GetCameraLocation() const
{
    if (CameraComponent)
    {
        return CameraComponent->GetComponentLocation();
    }

    if (OwnerPawn)
    {
        return OwnerPawn->GetActorLocation();
    }

    return FVector::ZeroVector;
}

FVector UPlayerInteractionComponent::GetCameraForward() const
{
    if (CameraComponent)
    {
        return CameraComponent->GetForwardVector();
    }

    if (OwnerPawn)
    {
        return OwnerPawn->GetActorForwardVector();
    }

    return FVector::ForwardVector;
}

void UPlayerInteractionComponent::SetBestInteractable(UInteractableComponent* NewBest)
{
    if (CurrentBestInteractable == NewBest) return;

    CurrentBestInteractable = NewBest;

    OnBestInteractableChanged.Broadcast(CurrentBestInteractable);

    /*if (CurrentBestInteractable != NewBest)
    {
        if (CurrentBestInteractable)
        {
            CurrentBestInteractable->SetHighlighted(false);
        }

        CurrentBestInteractable = NewBest;

        if (CurrentBestInteractable)
        {
            CurrentBestInteractable->SetHighlighted(true);
        }

        OnBestInteractableChanged.Broadcast(CurrentBestInteractable);
    }*/
}

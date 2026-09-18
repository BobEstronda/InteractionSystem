// Fill out your copyright notice in the Description page of Project Settings.


#include "InteractionBlockerComponent.h"
#include "InteractionSubsystem.h"
#include "Engine/World.h"

UInteractionBlockerComponent::UInteractionBlockerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
}

void UInteractionBlockerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UInteractionSubsystem* Subsystem = World->GetSubsystem<UInteractionSubsystem>())
		{
			Subsystem->RegisterBlocker(this);
		}
	}
}

void UInteractionBlockerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UInteractionSubsystem* Subsystem = World->GetSubsystem<UInteractionSubsystem>())
		{
			Subsystem->UnregisterBlocker(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

FBoxSphereBounds UInteractionBlockerComponent::GetBlockerBounds() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FBoxSphereBounds(FVector::ZeroVector, FVector(1.f, 1.f, 1.f), 1.f);
	}

	FBoxSphereBounds Bounds;
	if (bUseFullActorBounds)
	{
		Bounds = Owner->GetComponentsBoundingBox();
	}
	else if (Owner->GetRootComponent())
	{
		Bounds = Owner->GetRootComponent()->Bounds;
	}
	else
	{
		Bounds = FBoxSphereBounds(Owner->GetActorLocation(), FVector(1.f, 1.f, 1.f), 1.f);
	}

	if (BoundsPadding != 0.f)
	{
		Bounds = FBoxSphereBounds(Bounds.Origin, Bounds.BoxExtent + FVector(BoundsPadding), Bounds.SphereRadius + BoundsPadding);
	}

	return Bounds;
}

void UInteractionBlockerComponent::RefreshBounds()
{
	if (UWorld* World = GetWorld())
	{
		if (UInteractionSubsystem* Subsystem = World->GetSubsystem<UInteractionSubsystem>())
		{
			Subsystem->UpdateBlockerBounds(this);
		}
	}
}

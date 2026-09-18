// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionBlockerComponent.generated.h"

// Attach this to any actor that should occlude interactables from the player
// 
// This is intentionally NOT a physics trace: it is a cheap, deterministic
// AABB test driven by the same octree/broadphase pattern

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class INTERACTIONSYSTEM_API UInteractionBlockerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UInteractionBlockerComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UPROPERTY(EditAnywhere, Category = "Interaction|Blocker")
	bool bUseFullActorBounds = false;

	UPROPERTY(EditAnywhere, Category = "Interaction|Blocker")
	float BoundsPadding = 0.f;

	FBoxSphereBounds GetBlockerBounds() const;

	// for actors that move
	UFUNCTION(BlueprintCallable, Category = "Interaction|Blocker")
	void RefreshBounds();

		
};

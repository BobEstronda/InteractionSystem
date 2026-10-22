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

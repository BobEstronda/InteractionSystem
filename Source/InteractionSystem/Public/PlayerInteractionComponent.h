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
#include "PlayerInteractionComponent.generated.h"

class UInteractableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBestInteractableChanged, UInteractableComponent*, NewBestInteractable);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class INTERACTIONSYSTEM_API UPlayerInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

	UPROPERTY()
	UInteractableComponent* CurrentBestInteractable = nullptr;

	UPROPERTY()
	class UCameraComponent* CameraComponent;

	UPROPERTY()
	class APawn* OwnerPawn;

public:	
	UPlayerInteractionComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnBestInteractableChanged OnBestInteractableChanged;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryInteract();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	UInteractableComponent* GetCurrentBestInteractable() const { return CurrentBestInteractable; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	FVector GetCameraLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	FVector GetCameraForward() const;

	// Called by subsystem
	void SetBestInteractable(UInteractableComponent* NewBest);
		
};

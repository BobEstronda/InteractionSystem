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
#include "InteractableComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionStateChanged, bool, bIsRelevant);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteraction, AActor*, InteractingActor);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class INTERACTIONSYSTEM_API UInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UInteractableComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    FText InteractionText /*= TEXT("Interact") */;

    /*UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    float InteractionRange = 200.0f;*/

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    bool bCanInteract = true;

    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnInteractionStateChanged OnInteractionStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnInteraction OnInteraction;

    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnInteraction OnOverlap;

    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnInteraction OnEndOverlap;

    /*UFUNCTION(BlueprintCallable, Category = "Interaction")
    void SetHighlighted(bool bHighlight);

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    bool IsHighlighted() const { return bIsHighlighted; }*/

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void SetRelevance(const bool bRelevant);

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    bool IsRelevant() const { return bIsRelevant; }

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    FVector GetInteractionLocation() const;

    UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
    void OnInteract(AActor* InteractingActor);
    virtual void OnInteract_Implementation(AActor* InteractingActor);

    UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
    void OnReceivedOverlap(AActor* InteractingActor);
    virtual void OnReceivedOverlap_Implementation(AActor* InteractingActor);

    UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
    void OnReceivedEndOverlap(AActor* InteractingActor);
    virtual void OnReceivedEndOverlap_Implementation(AActor* InteractingActor);

private:
    bool bIsRelevant = false;
    bool bIsOverlapping = false;
		
};

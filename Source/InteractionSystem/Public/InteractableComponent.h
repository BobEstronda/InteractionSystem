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
//#include "ConditionTypes.h"
#include "InteractableComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionStateChanged, bool, bIsRelevant);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteraction, AActor*, InteractingActor);

// IO Types

// Called event from any actor in level
//USTRUCT(BlueprintType)
//struct INTERACTIONSYSTEM_API FInteractionOutputAction
//{
//    GENERATED_BODY()
//
//    UPROPERTY(EditAnywhere, Category = "InteractionIO")
//    FConditionStep test;
//
//    UPROPERTY(EditAnywhere, Category = "InteractionIO")
//    TSoftObjectPtr<AActor> TargetActor;
//
//    UPROPERTY(EditAnywhere, Category = "InteractionIO")
//    FName FunctionName;
//
//    UPROPERTY(EditAnywhere, Category = "InteractionIO", meta = (ClampMin = "0.0"))
//    float Delay = 0.0f;
//
//    UPROPERTY(EditAnywhere, Category = "InteractionIO")
//    bool bOnce = false;
//
//    UPROPERTY(Transient)
//    bool bHasFired = false;
//};
//
//// Listen event from any actor in level and execute any event from actor in level
//USTRUCT(BlueprintType)
//struct FInteractionInputAction
//{
//    GENERATED_BODY()
//
//    UPROPERTY(EditAnywhere, Category = "InteractionIO")
//    TSoftObjectPtr<AActor> TargetActor;
//
//    UPROPERTY(EditAnywhere, Category = "InteractionIO")
//    FName FunctionName;
//
//    UPROPERTY(EditAnywhere, Category = "InteractionIO")
//    TArray<FInteractionOutputAction> Output;
//};
//
//class UInteractionIOCascadeProxy;

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
    FText InteractionText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
    bool bCanInteract = true;

    //UPROPERTY(EditAnywhere, Category = "InteractionIO")
    //TArray<FInteractionOutputAction> Outputs;
    //
    //UPROPERTY(EditAnywhere, Category = "InteractionIO")
    //TArray<FInteractionInputAction> Inputs;

    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnInteractionStateChanged OnInteractionStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnInteraction OnInteraction;

    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnInteraction OnOverlap;

    UPROPERTY(BlueprintAssignable, Category = "Interaction")
    FOnInteraction OnEndOverlap;

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

    //UFUNCTION(BlueprintCallable, Category = "InteractionIO")
    //void FireOutputEvents();
    //
    //void ExecuteAction(FInteractionOutputAction& Action);
private:

    //static void CallFunction(AActor * Target, FName FunctionName);
    //
    //void BindCascadeListener(const FInteractionInputAction& Action);

private:
    bool bIsRelevant = false;
    bool bIsOverlapping = false;

    //UPROPERTY(Transient)
    //TArray<TObjectPtr<UInteractionIOCascadeProxy>> CascadeProxies;
		
};

//UCLASS()
//class UInteractionIOCascadeProxy : public UObject
//{
//    GENERATED_BODY()
//
//public:
//    UPROPERTY()
//    TObjectPtr<UInteractableComponent> OwnerComponent;
//
//    UPROPERTY()
//    TArray<FInteractionOutputAction> OutputEvents;
//
//    UFUNCTION()
//    void OnCascadeEventReceived();
//};
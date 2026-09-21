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
#include "TimerManager.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogInteractionIO, Log, All);

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

    for (FInteractionOutputAction& Action : Outputs)
    {
        Action.bHasFired = false;
    }

    for (FInteractionInputAction& Action : Inputs) 
    {
        BindCascadeListener(Action);
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

    CascadeProxies.Empty();

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
    FireOutputEvents();

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

void UInteractableComponent::FireOutputEvents()
{
    for (FInteractionOutputAction& Action : Outputs)
    {
        if (Action.bOnce && Action.bHasFired)
        {
            continue;
        }

        ExecuteAction(Action);
    }
}

void UInteractableComponent::ExecuteAction(FInteractionOutputAction& Action)
{
    Action.bHasFired = true;

    TWeakObjectPtr<UInteractableComponent> WeakThis(this);
    TSoftObjectPtr<AActor> TargetSoft = Action.TargetActor;
    FName FunctionName = Action.FunctionName;

    auto DoExecute = [WeakThis, TargetSoft, FunctionName]() mutable
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            AActor* Target = TargetSoft.LoadSynchronous();
            if (!IsValid(Target))
            {
                UE_LOG(LogInteractionIO, Warning, TEXT("InteractionIO: TargetActor inválido ao chamar '%s'"), *FunctionName.ToString());
                return;
            }

            CallFunction(Target, FunctionName);
        };

    if (Action.Delay > 0.0f)
    {
        FTimerHandle TimerHandle;
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateLambda(DoExecute), Action.Delay, false);
        }
    }
    else
    {
        DoExecute();
    }
}

void UInteractableComponent::CallFunction(AActor* Target, FName FunctionName)
{
    if (!IsValid(Target) || FunctionName.IsNone())
    {
        return;
    }

    UFunction* Function = Target->FindFunction(FunctionName);
    if (!Function)
    {
        UE_LOG(LogInteractionIO, Warning, TEXT("InteractionIO: função '%s' não encontrada em '%s'"),
            *FunctionName.ToString(), *Target->GetName());
        return;
    }

    Target->ProcessEvent(Function, nullptr);
}

void UInteractableComponent::BindCascadeListener(const FInteractionInputAction& Action)
{
    AActor* Target = Action.TargetActor.LoadSynchronous();
    if (!IsValid(Target))
    {
        return;
    }

    FMulticastDelegateProperty* DelegateProp = FindFProperty<FMulticastDelegateProperty>(Target->GetClass(), Action.FunctionName);
    if (!DelegateProp)
    {
        UE_LOG(LogInteractionIO, Warning, TEXT("InteractionIO: delegate '%s' não encontrado em '%s'"),
            *Action.FunctionName.ToString(), *Target->GetName());
        return;
    }

    UInteractionIOCascadeProxy* Proxy = NewObject<UInteractionIOCascadeProxy>(this);
    Proxy->OwnerComponent = this;
    Proxy->OutputEvents = Action.Output;
    CascadeProxies.Add(Proxy);

    FScriptDelegate ScriptDelegate;
    ScriptDelegate.BindUFunction(Proxy, GET_FUNCTION_NAME_CHECKED(UInteractionIOCascadeProxy, OnCascadeEventReceived));

    void* DelegateAddr = DelegateProp->ContainerPtrToValuePtr<void>(Target);
    DelegateProp->AddDelegate(MoveTemp(ScriptDelegate), Target, DelegateAddr);
}

// Proxy

void UInteractionIOCascadeProxy::OnCascadeEventReceived()
{
    if (IsValid(OwnerComponent) && !OutputEvents.IsEmpty())
    {
        for (FInteractionOutputAction& Action : OutputEvents)
        {
            OwnerComponent->ExecuteAction(Action);
        }
    }
}

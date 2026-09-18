// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InteractionSettings.generated.h"

/**
 * 
 */
UCLASS(Config = Game, defaultconfig, meta = (DisplayName = "Interaction Settings"))
class INTERACTIONSYSTEM_API UInteractionSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Interaction")
	float MaxRelevanceDistance = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Interaction")
	float MaxInteractionDistance = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Interaction")
	float OverlapBoundsPadding = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Interaction")
	float CenterScreenBias = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Interaction")
	float ViewAngleThreshold = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Interaction")
	float MinScreenAngleThreshold = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Interaction")
	float MaxScreenAngleThreshold = 30.f;
};

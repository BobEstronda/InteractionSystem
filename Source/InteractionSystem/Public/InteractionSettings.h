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
#include "Engine/DeveloperSettings.h"
#include "InteractionSettings.generated.h"


UCLASS(Config = Game, defaultconfig, meta = (DisplayName = "Interaction Settings"))
class INTERACTIONSYSTEM_API UInteractionSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Config, EditAnywhere, Category = "Interaction")
	float MaxRelevanceDistance = 800.f;

	UPROPERTY(Config, EditAnywhere, Category = "Interaction")
	float MaxInteractionDistance = 300.f;

	UPROPERTY(Config, EditAnywhere, Category = "Interaction")
	float OverlapBoundsPadding = 0.f;

	UPROPERTY(Config, EditAnywhere, Category = "Interaction")
	float CenterScreenBias = 2.f;

	UPROPERTY(Config, EditAnywhere, Category = "Interaction")
	float ViewAngleThreshold = 40.f;

	UPROPERTY(Config, EditAnywhere, Category = "Interaction")
	float MinScreenAngleThreshold = 10.f;

	UPROPERTY(Config, EditAnywhere, Category = "Interaction")
	float MaxScreenAngleThreshold = 30.f;
};

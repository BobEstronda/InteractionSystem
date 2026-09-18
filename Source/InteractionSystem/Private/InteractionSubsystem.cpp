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


#include "InteractionSubsystem.h"

#include "PlayerInteractionComponent.h"
#include "InteractableComponent.h"
#include "InteractionBlockerComponent.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "InteractionSettings.h"

DEFINE_LOG_CATEGORY(LogInteractionSystem);

DECLARE_STATS_GROUP(TEXT("Interaction System"), STATGROUP_Interaction, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Tick (Total)"), STAT_Interaction_Tick, STATGROUP_Interaction);
DECLARE_CYCLE_STAT(TEXT("UpdateInteractions"), STAT_Interaction_UpdateInteractions, STATGROUP_Interaction);
DECLARE_CYCLE_STAT(TEXT("GetBestInteractableForPlayer"), STAT_Interaction_GetBest, STATGROUP_Interaction);
DECLARE_CYCLE_STAT(TEXT("Octree Query (Interactables)"), STAT_Interaction_OctreeQuery, STATGROUP_Interaction);
DECLARE_CYCLE_STAT(TEXT("CalculateInteractionScore"), STAT_Interaction_Score, STATGROUP_Interaction);
DECLARE_CYCLE_STAT(TEXT("IsOverlappingBounds"), STAT_Interaction_Overlap, STATGROUP_Interaction);
DECLARE_CYCLE_STAT(TEXT("IsInViewAndVisible"), STAT_Interaction_ViewVisible, STATGROUP_Interaction);
DECLARE_CYCLE_STAT(TEXT("IsOcclusionBlocked"), STAT_Interaction_Occlusion, STATGROUP_Interaction);
DECLARE_CYCLE_STAT(TEXT("RegisterInteractable"), STAT_Interaction_Register, STATGROUP_Interaction);
DECLARE_CYCLE_STAT(TEXT("UnregisterInteractable"), STAT_Interaction_Unregister, STATGROUP_Interaction);
DECLARE_CYCLE_STAT(TEXT("RegisterBlocker"), STAT_Interaction_RegisterBlocker, STATGROUP_Interaction);

static TAutoConsoleVariable<int32> CVarInteractionDebugDraw(
	TEXT("interaction.debug.draw"),
	0,
	TEXT("0 = off, 1 = on. Draws live query boxes, occlusion segments, overlap bounds and per-candidate scores for the interaction system."),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarInteractionDebugRegistered(
	TEXT("interaction.debug.registered"),
	0,
	TEXT("0 = off, 1 = on. Draws bounds for every registered interactable (cyan) and blocker (orange), regardless of player proximity."),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarInteractionDebugStats(
	TEXT("interaction.debug.stats"),
	0,
	TEXT("0 = off, 1 = on. Shows an on-screen performance breakdown (µs) of the interaction system: total tick cost and cost of individual phases/methods."),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarInteractionDebugLog(
	TEXT("interaction.debug.log"),
	0,
	TEXT("0 = off, 1 = on. Logs the winning interactable and its score breakdown to LogInteractionSystem whenever the best interactable changes."),
	ECVF_Cheat);

namespace
{
	struct FScopedMicrosecondTimer
	{
		double& Accumulator;
		int32* Counter;
		double StartSeconds;

		explicit FScopedMicrosecondTimer(double& InAccumulator, int32* InCounter = nullptr)
			: Accumulator(InAccumulator)
			, Counter(InCounter)
			, StartSeconds(FPlatformTime::Seconds())
		{
			if (Counter) { ++(*Counter); }
		}

		~FScopedMicrosecondTimer()
		{
			Accumulator += (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
		}
	};
}

TMap<FInteractableOctreeElement, FOctreeElementId2> FInteractableOctreeSemantics::OctreeIds;
TMap<FBlockerOctreeElement, FOctreeElementId2> FBlockerOctreeSemantics::OctreeIds;

void UInteractionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Warning, TEXT("InteractionSubsystem Initialized"));

	const UInteractionSettings* InteractionSettings = GetDefault<UInteractionSettings>();
	MaxRelevanceDistance = InteractionSettings->MaxRelevanceDistance;
	MaxInteractionDistance = InteractionSettings->MaxInteractionDistance;
	OverlapBoundsPadding = InteractionSettings->OverlapBoundsPadding;
	CenterScreenBias = InteractionSettings->CenterScreenBias;
	ViewAngleThreshold = InteractionSettings->ViewAngleThreshold;
	MinScreenAngleThreshold = InteractionSettings->MinScreenAngleThreshold;
	MaxScreenAngleThreshold = InteractionSettings->MaxScreenAngleThreshold;

	FBox WorldBounds(FVector(-100000), FVector(100000));
	InteractableOctree = MakeUnique<FInteractableOctree>(WorldBounds.GetCenter(), WorldBounds.GetExtent().X);
	BlockerOctree = MakeUnique<FBlockerOctree>(WorldBounds.GetCenter(), WorldBounds.GetExtent().X);
}

void UInteractionSubsystem::Deinitialize()
{
	TArray<UInteractableComponent*> InteractablesToRemove;
	RegisteredElements.GetKeys(InteractablesToRemove);
	for (UInteractableComponent* Interactable : InteractablesToRemove)
	{
		UnregisterInteractable(Interactable);
	}

	TArray<UInteractionBlockerComponent*> BlockersToRemove;
	RegisteredBlockerElements.GetKeys(BlockersToRemove);
	for (UInteractionBlockerComponent* Blocker : BlockersToRemove)
	{
		UnregisterBlocker(Blocker);
	}

	RegisteredPlayers.Empty();
	RegisteredElements.Empty();
	RegisteredBlockerElements.Empty();
	PreviousOverlapStates.Empty();

	InteractableOctree.Reset();
	BlockerOctree.Reset();

	UE_LOG(LogInteractionSystem, Log, TEXT("InteractionSubsystem Deinitialize"));

	Super::Deinitialize();
}

void UInteractionSubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_Interaction_Tick);

	CurrentFrameStats = FInteractionFrameStats();
	CurrentFrameStats.NumRegisteredInteractables = RegisteredElements.Num();
	CurrentFrameStats.NumRegisteredBlockers = RegisteredBlockerElements.Num();
	CurrentFrameStats.NumRegisteredPlayers = RegisteredPlayers.Num();

	{
		FScopedMicrosecondTimer TotalTimer(CurrentFrameStats.TickTotalMicroseconds);
		UpdateInteractions();
	}

	LastFrameStats = CurrentFrameStats;

	if (CVarInteractionDebugRegistered.GetValueOnGameThread() != 0)
	{
		DrawDebugRegisteredBounds();
	}

	if (CVarInteractionDebugStats.GetValueOnGameThread() != 0)
	{
		DrawDebugStatsOnScreen();
	}
}

bool UInteractionSubsystem::IsTickable() const
{
	return !IsTemplate() && GetWorld() && !GetWorld()->IsNetMode(NM_DedicatedServer);
}

TStatId UInteractionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UInteractionSubsystem, STATGROUP_Tickables);
}

void UInteractionSubsystem::RegisterInteractable(UInteractableComponent* InteractableComponent)
{
	SCOPE_CYCLE_COUNTER(STAT_Interaction_Register);

	if (InteractableComponent && !RegisteredElements.Contains(InteractableComponent))
	{
		FInteractableOctreeElement InteractableElement = FInteractableOctreeElement(InteractableComponent);
		InteractableOctree->AddElement(InteractableElement);
		RegisteredElements.Add(InteractableComponent, InteractableElement);

		UE_CLOG(CVarInteractionDebugLog.GetValueOnGameThread() != 0, LogInteractionSystem, Verbose,
			TEXT("Registered interactable '%s'. Total registered: %d"),
			*GetNameSafe(InteractableComponent->GetOwner()), RegisteredElements.Num());
	}
}

void UInteractionSubsystem::UnregisterInteractable(UInteractableComponent* InteractableComponent)
{
	SCOPE_CYCLE_COUNTER(STAT_Interaction_Unregister);

	if (!InteractableComponent)
	{
		return;
	}

	if (FInteractableOctreeElement* InteractableElement = RegisteredElements.Find(InteractableComponent))
	{
		if (FOctreeElementId2* ElementId2 = FInteractableOctreeSemantics::OctreeIds.Find(*InteractableElement))
		{
			InteractableOctree->RemoveElement(*ElementId2);
			FInteractableOctreeSemantics::OctreeIds.Remove(*InteractableElement);
		}

		RegisteredElements.Remove(InteractableComponent);
	}

	PreviousOverlapStates.Remove(InteractableComponent);

	for (UPlayerInteractionComponent* Player : RegisteredPlayers)
	{
		if (Player && Player->GetCurrentBestInteractable() == InteractableComponent)
		{
			Player->SetBestInteractable(nullptr);
		}
	}
}

void UInteractionSubsystem::RegisterBlocker(UInteractionBlockerComponent* BlockerComponent)
{
	SCOPE_CYCLE_COUNTER(STAT_Interaction_RegisterBlocker);

	if (BlockerComponent && !RegisteredBlockerElements.Contains(BlockerComponent))
	{
		FBlockerOctreeElement BlockerElement = FBlockerOctreeElement(BlockerComponent);
		BlockerOctree->AddElement(BlockerElement);
		RegisteredBlockerElements.Add(BlockerComponent, BlockerElement);

		UE_CLOG(CVarInteractionDebugLog.GetValueOnGameThread() != 0, LogInteractionSystem, Verbose,
			TEXT("Registered blocker '%s'. Total registered: %d"),
			*GetNameSafe(BlockerComponent->GetOwner()), RegisteredBlockerElements.Num());
	}
}

void UInteractionSubsystem::UnregisterBlocker(UInteractionBlockerComponent* BlockerComponent)
{
	if (!BlockerComponent)
	{
		return;
	}

	if (FBlockerOctreeElement* BlockerElement = RegisteredBlockerElements.Find(BlockerComponent))
	{
		if (FOctreeElementId2* ElementId2 = FBlockerOctreeSemantics::OctreeIds.Find(*BlockerElement))
		{
			BlockerOctree->RemoveElement(*ElementId2);
			FBlockerOctreeSemantics::OctreeIds.Remove(*BlockerElement);
		}

		RegisteredBlockerElements.Remove(BlockerComponent);
	}
}

void UInteractionSubsystem::UpdateBlockerBounds(UInteractionBlockerComponent* BlockerComponent)
{
	if (!BlockerComponent || !RegisteredBlockerElements.Contains(BlockerComponent))
	{
		return;
	}

	UnregisterBlocker(BlockerComponent);
	RegisterBlocker(BlockerComponent);
}

void UInteractionSubsystem::RegisterPlayer(UPlayerInteractionComponent* PlayerComponent)
{
	if (PlayerComponent && !RegisteredPlayers.Contains(PlayerComponent))
	{
		RegisteredPlayers.Add(PlayerComponent);
	}
}

void UInteractionSubsystem::UnregisterPlayer(UPlayerInteractionComponent* PlayerComponent)
{
	if (PlayerComponent)
	{
		RegisteredPlayers.Remove(PlayerComponent);
	}
}

UInteractableComponent* UInteractionSubsystem::GetBestInteractableForPlayer(UPlayerInteractionComponent* PlayerComponent)
{
	SCOPE_CYCLE_COUNTER(STAT_Interaction_GetBest);

	if (!IsValid(PlayerComponent)) return nullptr;

	UInteractableComponent* BestInteractable = nullptr;
	float BestScore = 0.f;

	const float QueryExtent = MaxRelevanceDistance;
	const FVector PlayerLocation = PlayerComponent->GetOwner()->GetActorLocation();
	FBox QueryBox = FBox::BuildAABB(PlayerLocation, FVector(QueryExtent));

	//FBox QueryBox = FBox::BuildAABB(PlayerComponent->GetOwner()->GetActorLocation(), FVector(1000.f));

	TArray<FInteractableOctreeElement> NearbyElements;
	/*InteractableOctree->FindElementsWithBoundsTest(QueryBox,
		[&NearbyElements](const FInteractableOctreeElement& Element)
		{
			NearbyElements.Add(Element);
		});
	*/
	{
		SCOPE_CYCLE_COUNTER(STAT_Interaction_OctreeQuery);
		FScopedMicrosecondTimer OctreeTimer(CurrentFrameStats.OctreeQueryMicroseconds, &CurrentFrameStats.NumOctreeQueries);

		InteractableOctree->FindElementsWithBoundsTest(QueryBox,
			[&NearbyElements](const FInteractableOctreeElement& Element)
			{
				NearbyElements.Add(Element);
			});
	}

	if (CVarInteractionDebugDraw.GetValueOnGameThread() != 0)
	{
		DrawDebugBox(GetWorld(), QueryBox.GetCenter(), QueryBox.GetExtent(), FColor::Yellow, false, -1.f, 0, 1.5f);
	}

	CurrentFrameStats.NumCandidatesEvaluated += NearbyElements.Num();
	
	for (FInteractableOctreeElement OctreeElement : NearbyElements)
	{
		if (!IsValid(OctreeElement.Interactable) || !OctreeElement.Interactable->bCanInteract)
		{
			continue;
		}

		const float Score = CalculateInteractionScore(PlayerComponent, OctreeElement.Interactable);

		if (CVarInteractionDebugDraw.GetValueOnGameThread() != 0)
		{
			const FVector Loc = OctreeElement.Interactable->GetInteractionLocation();
			const bool bIsCurrentBest = Score > BestScore;
			DrawDebugString(GetWorld(), Loc + FVector(0, 0, 30.f),
				FString::Printf(TEXT("score: %.2f"), Score),
				nullptr, bIsCurrentBest ? FColor::Green : FColor::Silver, 0.f, false, 1.1f);
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestInteractable = OctreeElement.Interactable;
		}
	}

	if (CVarInteractionDebugLog.GetValueOnGameThread() != 0 && BestInteractable != PlayerComponent->GetCurrentBestInteractable())
	{
		UE_LOG(LogInteractionSystem, Verbose, TEXT("[%s] Best interactable -> '%s' (score %.3f, %d candidates evaluated)"),
			*GetNameSafe(PlayerComponent->GetOwner()),
			BestInteractable ? *GetNameSafe(BestInteractable->GetOwner()) : TEXT("None"),
			BestScore, NearbyElements.Num());
	}

	return BestInteractable;
}

void UInteractionSubsystem::UpdateInteractableBounds(UInteractableComponent* InteractableComponent)
{
	if (!InteractableComponent || !RegisteredElements.Contains(InteractableComponent))
	{
		return;
	}

	UnregisterInteractable(InteractableComponent);
	RegisterInteractable(InteractableComponent);
}

void UInteractionSubsystem::UpdateInteractions()
{
	SCOPE_CYCLE_COUNTER(STAT_Interaction_UpdateInteractions);
	FScopedMicrosecondTimer UpdateTimer(CurrentFrameStats.UpdateInteractionsMicroseconds);

	RegisteredPlayers.RemoveAll([](UPlayerInteractionComponent* Comp) { return !IsValid(Comp); });

	for (UPlayerInteractionComponent* Player : RegisteredPlayers)
	{
		if (!IsValid(Player))
		{
			continue;
		}

		UInteractableComponent* NewBest = GetBestInteractableForPlayer(Player);

		if (Player->GetCurrentBestInteractable() != NewBest)
		{
			Player->SetBestInteractable(NewBest);
		}
	}
}

float UInteractionSubsystem::CalculateInteractionScore(UPlayerInteractionComponent* Player, UInteractableComponent* Interactable)
{
	SCOPE_CYCLE_COUNTER(STAT_Interaction_Score);
	FScopedMicrosecondTimer Timer(CurrentFrameStats.ScoreCalculationMicroseconds, &CurrentFrameStats.NumScoreCalculations);

	if (!Player || !Interactable) return 0.f;

	FVector AimOrigin;
	FVector AimDirection;
	GetInteractionAimRay(Player, AimOrigin, AimDirection);

	const FVector InteractableLocation = Interactable->GetInteractionLocation();

	const float DistSq = FVector::DistSquared(AimOrigin, InteractableLocation);

	const float RelevanceDistanceSq = FMath::Square(MaxRelevanceDistance);

	if (DistSq > RelevanceDistanceSq)
	{
		Interactable->SetRelevance(false);
		return 0.f;
	}

	const bool bShouldBeOverlapping = IsOverlappingBounds(Player, Interactable);
	bool& bPreviouslyOverlapping = PreviousOverlapStates.FindOrAdd(Interactable);

	if (bPreviouslyOverlapping != bShouldBeOverlapping)
	{
		if (bShouldBeOverlapping)
		{
			Interactable->OnReceivedOverlap(Player->GetOwner());
		}
		else
		{
			Interactable->OnReceivedEndOverlap(Player->GetOwner());
		}
		bPreviouslyOverlapping = bShouldBeOverlapping;
	}

	const FVector ToInteractable = InteractableLocation - AimOrigin;
	const float DistFromOriginSq = ToInteractable.SizeSquared();
	if (FMath::IsNearlyZero(DistFromOriginSq))
	{
		return 0.f;
	}
	const float InvLen = FMath::InvSqrt(DistFromOriginSq);
	const FVector DirectionToInteractable = ToInteractable * InvLen;

	const float DotProduct = FVector::DotProduct(AimDirection, DirectionToInteractable);

	const bool bIsInView = IsInViewAndVisible(Interactable, DotProduct);
	Interactable->SetRelevance(bIsInView);

	if (!bIsInView) return 0.f;

	if (IsOcclusionBlocked(AimOrigin, InteractableLocation, Interactable))
	{
		Interactable->SetRelevance(false);
		return 0.f;
	}

	const float InteractionDistanceSq = FMath::Square(MaxInteractionDistance);

	if (DistSq > InteractionDistanceSq) return 0.f;

	// Base score inversely proportional to distance
	const float BaseScore = 1.f - (DistSq / InteractionDistanceSq);

	const float MinScreenDotThreshold = FMath::Cos(FMath::DegreesToRadians(MinScreenAngleThreshold));
	const float MaxScreenDotThreshold = FMath::Cos(FMath::DegreesToRadians(MaxScreenAngleThreshold));

	// Apply center screen / center aim bias
	float CenterBias = 1.f;
	if (DotProduct > MinScreenDotThreshold)
	{
		CenterBias = CenterScreenBias;
	}
	else if (DotProduct > MaxScreenDotThreshold)
	{
		CenterBias = 1.5f;
	}

	return BaseScore * CenterBias;
}

bool UInteractionSubsystem::IsInViewAndVisible(const UInteractableComponent * Interactable, const float Dot) const
{
	SCOPE_CYCLE_COUNTER(STAT_Interaction_ViewVisible);
	FScopedMicrosecondTimer Timer(CurrentFrameStats.ViewVisibilityMicroseconds, &CurrentFrameStats.NumViewVisibilityTests);

	if (!Interactable) return false;

	static const float ViewDotThreshold = FMath::Cos(FMath::DegreesToRadians(ViewAngleThreshold));

	if (Dot < ViewDotThreshold) return false;

	const AActor* InteractableActor = Interactable->GetOwner();
	if (InteractableActor)
	{
		UPrimitiveComponent* PrimitiveComponent = InteractableActor->FindComponentByClass<UPrimitiveComponent>();
		if (PrimitiveComponent)
		{
			//FPrimitiveSceneInfoData& SceneInfoData = PrimitiveComponent->GetSceneData();
			const bool bIsVisible = (GetWorld()->GetTimeSeconds() - /*SceneInfoData.LastRenderTimeOnScreen*/ PrimitiveComponent->GetLastRenderTimeOnScreen()) <= .05f;
			if (bIsVisible)
			{
				return true;
			}
		}
	}

	return false; 
}

bool UInteractionSubsystem::IsOcclusionBlocked(const FVector& From, const FVector& To, const UInteractableComponent* Ignore) const
{
	SCOPE_CYCLE_COUNTER(STAT_Interaction_Occlusion);
	FScopedMicrosecondTimer Timer(CurrentFrameStats.OcclusionTestMicroseconds, &CurrentFrameStats.NumOcclusionTests);

	if (!bUseGeometryOcclusion || !BlockerOctree.IsValid())
	{
		return false;
	}

	const FBox QuerySegmentBox(FVector::Min(From, To), FVector::Max(From, To));

	bool bBlocked = false;
	FBox BlockingBox;

	BlockerOctree->FindElementsWithBoundsTest(QuerySegmentBox,
		[&](const FBlockerOctreeElement& BlockerElement)
		{
			if (bBlocked || !BlockerElement.Blocker.IsValid())
			{
				return;
			}

			if (SegmentIntersectsAABB(From, To, BlockerElement.Bounds.GetBox()))
			{
				bBlocked = true;
				BlockingBox = BlockerElement.Bounds.GetBox();
			}
		});

	if (bBlocked)
	{
		++CurrentFrameStats.NumOcclusionBlocked;
	}

	if (CVarInteractionDebugDraw.GetValueOnGameThread() != 0)
	{
		DrawDebugLine(GetWorld(), From, To, bBlocked ? FColor::Red : FColor::Green, false, -1.f, 0, 2.f);
		if (bBlocked)
		{
			DrawDebugBox(GetWorld(), BlockingBox.GetCenter(), BlockingBox.GetExtent(), FColor::Red, false, -1.f, 0, 2.f);
		}
	}

	return bBlocked;
}

bool UInteractionSubsystem::IsOverlappingBounds(const UPlayerInteractionComponent* Player, const UInteractableComponent* Interactable) const
{
	SCOPE_CYCLE_COUNTER(STAT_Interaction_Overlap);
	FScopedMicrosecondTimer Timer(CurrentFrameStats.OverlapTestMicroseconds, &CurrentFrameStats.NumOverlapTests);

	if (!Player || !Player->GetOwner() || !Interactable || !Interactable->GetOwner())
	{
		return false;
	}

	FBoxSphereBounds PlayerBounds = Player->GetOwner()->GetComponentsBoundingBox();
	FBoxSphereBounds InteractableBounds = Interactable->GetOwner()->GetRootComponent()
		? Interactable->GetOwner()->GetRootComponent()->Bounds
		: FBoxSphereBounds(Interactable->GetOwner()->GetActorLocation(), FVector(1.f), 1.f);

	if (OverlapBoundsPadding != 0.f)
	{
		PlayerBounds = FBoxSphereBounds(PlayerBounds.Origin, PlayerBounds.BoxExtent + FVector(OverlapBoundsPadding), PlayerBounds.SphereRadius + OverlapBoundsPadding);
	}

	bool bOverlapping = false;
	if (FBoxSphereBounds::SpheresIntersect(PlayerBounds, InteractableBounds))
	{
		bOverlapping = PlayerBounds.GetBox().Intersect(InteractableBounds.GetBox());
	}

	if (CVarInteractionDebugDraw.GetValueOnGameThread() != 0)
	{
		const FColor PlayerColor = bOverlapping ? FColor::Blue : FColor(120, 120, 120);
		DrawDebugBox(GetWorld(), PlayerBounds.Origin, PlayerBounds.BoxExtent, PlayerColor, false, -1.f, 0, 1.f);
		DrawDebugBox(GetWorld(), InteractableBounds.Origin, InteractableBounds.BoxExtent, PlayerColor, false, -1.f, 0, 1.f);
	}

	return bOverlapping;
}

void UInteractionSubsystem::GetInteractionAimRay(const UPlayerInteractionComponent* Player, FVector& OutOrigin, FVector& OutDirection) const
{
	OutOrigin = Player->GetCameraLocation();

	switch (Player->GetAimMode())
	{
	case EInteractionAimMode::PawnForward:
		OutOrigin = Player->GetOwner()->GetActorLocation();
		OutDirection = Player->GetOwner()->GetActorForwardVector();
		break;

	case EInteractionAimMode::ExternalDirection:
		OutOrigin = Player->GetOwner()->GetActorLocation();
		FVector ExternalDir = Player->GetExternalAimDirection();
		if (!ExternalDir.IsNearlyZero())
		{
			OutDirection = ExternalDir.GetSafeNormal();
		}
		else
		{
			OutDirection = Player->GetOwner()->GetActorForwardVector();
		}
		break;

	case EInteractionAimMode::CameraForward:
	default:
		OutOrigin = Player->GetCameraLocation();
		OutDirection = Player->GetCameraForward();
		break;
	}
}

bool UInteractionSubsystem::SegmentIntersectsAABB(const FVector& Start, const FVector& End, const FBox& Box)
{
	const FVector Dir = End - Start;
	float TMin = 0.f;
	float TMax = 1.f;

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (FMath::IsNearlyZero(Dir[Axis]))
		{
			if (Start[Axis] < Box.Min[Axis] || Start[Axis] > Box.Max[Axis])
			{
				return false;
			}
			continue;
		}

		const float InvD = 1.f / Dir[Axis];
		float T0 = (Box.Min[Axis] - Start[Axis]) * InvD;
		float T1 = (Box.Max[Axis] - Start[Axis]) * InvD;

		if (T0 > T1)
		{
			Swap(T0, T1);
		}

		TMin = FMath::Max(TMin, T0);
		TMax = FMath::Min(TMax, T1);

		if (TMin > TMax)
		{
			return false;
		}
	}

	return true;
}

void UInteractionSubsystem::DrawDebugRegisteredBounds() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const auto& Pair : RegisteredElements)
	{
		const FBox Box = Pair.Value.Bounds.GetBox();
		DrawDebugBox(World, Box.GetCenter(), Box.GetExtent(), FColor::Cyan, false, -1.f, 0, 1.f);
	}

	for (const auto& Pair : RegisteredBlockerElements)
	{
		const FBox Box = Pair.Value.Bounds.GetBox();
		DrawDebugBox(World, Box.GetCenter(), Box.GetExtent(), FColor::Orange, false, -1.f, 0, 1.f);
	}
}

void UInteractionSubsystem::DrawDebugStatsOnScreen() const
{
	if (!GEngine)
	{
		return;
	}

	const FInteractionFrameStats& S = LastFrameStats;

	int32 Key = 9000;
	const float T = 1.5f;

	GEngine->AddOnScreenDebugMessage(Key++, T, FColor::Yellow,
		FString::Printf(TEXT("[Interaction] TOTAL TICK: %.2f us  |  players: %d  interactables: %d  blockers: %d"),
			S.TickTotalMicroseconds, S.NumRegisteredPlayers, S.NumRegisteredInteractables, S.NumRegisteredBlockers));

	GEngine->AddOnScreenDebugMessage(Key++, T, FColor::White,
		FString::Printf(TEXT("  UpdateInteractions: %.2f us   (avg/player: %.2f us)"),
			S.UpdateInteractionsMicroseconds, S.AvgMicrosecondsPerPlayer()));

	GEngine->AddOnScreenDebugMessage(Key++, T, FColor::White,
		FString::Printf(TEXT("  Octree query:   %.2f us total / %d queries  (avg: %.2f us)"),
			S.OctreeQueryMicroseconds, S.NumOctreeQueries, S.AvgMicrosecondsPerOctreeQuery()));

	GEngine->AddOnScreenDebugMessage(Key++, T, FColor::White,
		FString::Printf(TEXT("  Score calc:     %.2f us total / %d calls    (avg: %.2f us)  candidates evaluated: %d"),
			S.ScoreCalculationMicroseconds, S.NumScoreCalculations, S.AvgMicrosecondsPerScore(), S.NumCandidatesEvaluated));

	GEngine->AddOnScreenDebugMessage(Key++, T, FColor::White,
		FString::Printf(TEXT("  Occlusion test: %.2f us total / %d tests    (avg: %.2f us)  blocked: %d"),
			S.OcclusionTestMicroseconds, S.NumOcclusionTests, S.AvgMicrosecondsPerOcclusionTest(), S.NumOcclusionBlocked));

	GEngine->AddOnScreenDebugMessage(Key++, T, FColor::White,
		FString::Printf(TEXT("  Overlap test:   %.2f us total / %d tests    (avg: %.2f us)"),
			S.OverlapTestMicroseconds, S.NumOverlapTests, S.AvgMicrosecondsPerOverlapTest()));

	GEngine->AddOnScreenDebugMessage(Key++, T, FColor::White,
		FString::Printf(TEXT("  View/visibility: %.2f us total / %d tests"),
			S.ViewVisibilityMicroseconds, S.NumViewVisibilityTests));

	GEngine->AddOnScreenDebugMessage(Key++, T, FColor::Emerald,
		TEXT("  (also see 'stat Interaction' for engine-level cycle counters / Unreal Insights)"));
}

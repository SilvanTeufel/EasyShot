// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "EasyShotCineCameraActor.h"
#include "CineCameraComponent.h"

DEFINE_LOG_CATEGORY(LogEasyShot);
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "Components/SceneComponent.h"

AEasyShotCineCameraActor::AEasyShotCineCameraActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	if (UCineCameraComponent* CameraComp = GetCineCameraComponent())
	{
		CameraComp->FocusSettings.FocusMethod = ECameraFocusMethod::Tracking;
	}
}

void AEasyShotCineCameraActor::BeginPlay()
{
	Super::BeginPlay();

	CurrentTargetIndex = 0;
	CurrentSideIndex = 0;

	// Ensure this camera is the view target for screenshot consistency
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		PC->SetViewTarget(this);
	}

	// Start the process with a small delay to ensure the world is ready
	GetWorldTimerManager().SetTimer(StepTimerHandle, this, &AEasyShotCineCameraActor::ProcessNext, 1.0f, false);
}

void AEasyShotCineCameraActor::ProcessNext()
{
	if (CurrentTargetIndex >= Targets.Num())
	{
		UE_LOG(LogEasyShot, Log, TEXT("EasyShot: All targets processed."));
		return;
	}

	if (CurrentSideIndex == 0)
	{
		SpawnCurrentTarget();
	}

	if (CurrentSpawnedActor)
	{
		// Ensure this camera is the view target
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			PC->SetViewTarget(this);
		}

		const FEasyShotTargetEntry& Entry = Targets[CurrentTargetIndex];
		
		// Initial rotation to identify "Front" face (+X)
		// We'll calculate bounds after applying user offsets but BEFORE side rotations if we want consistent size?
		// Actually, user wants it to fill the screen for EACH side, so we rotate first.
		
		// Rotation: Face the camera base direction
		FVector CameraLocation = GetActorLocation();
		FRotator CameraRotation = GetActorRotation();
		FVector CameraForward = CameraRotation.Vector();
		
		FRotator FaceCameraRot = (-CameraForward).ToOrientationRotator();
		
		// Determine side rotation (6 sides when enabled):
		// 0: Front, 1: Right, 2: Back, 3: Left, 4: Top, 5: Bottom
		FRotator SideRotation = FRotator::ZeroRotator;
		if (bTakeScreenshotsFromAllSides)
		{
			switch (CurrentSideIndex)
			{
			case 0: SideRotation = FRotator(0.f,   0.f,   0.f); break;   // Front
			case 1: SideRotation = FRotator(0.f,  90.f,   0.f); break;   // Right
			case 2: SideRotation = FRotator(0.f, 180.f,   0.f); break;   // Back
			case 3: SideRotation = FRotator(0.f, 270.f,   0.f); break;   // Left
			case 4: SideRotation = FRotator(-90.f, 0.f,   0.f); break;   // Top (pitch up so top faces camera)
			case 5: SideRotation = FRotator( 90.f, 0.f,   0.f); break;   // Bottom (pitch down so bottom faces camera)
			default: SideRotation = FRotator::ZeroRotator; break;
			}
		}
		
		FRotator CurrentRotation = FaceCameraRot + Entry.RotationOffset + GlobalRotationOffset + SideRotation;
		CurrentSpawnedActor->SetActorRotation(CurrentRotation);
		CurrentSpawnedActor->UpdateComponentTransforms();

		// Calculate bounds in Camera Space for reliable framing
		FTransform WorldToCamera = GetActorTransform().Inverse();
		FBox CameraSpaceBox(ForceInit);
		bool bFoundAny = false;

		TArray<UPrimitiveComponent*> AllComps;
		CurrentSpawnedActor->GetComponents<UPrimitiveComponent>(AllComps, true);

		for (UPrimitiveComponent* Comp : AllComps)
		{
			if (Comp && Comp->IsVisible() && Comp->Bounds.SphereRadius > 1.0f)
			{
				FBox LocalBox(ForceInit);
				if (USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Comp))
				{
					if (Skel->GetSkeletalMeshAsset())
					{
						LocalBox = Skel->GetSkeletalMeshAsset()->GetImportedBounds().GetBox();
					}
				}
				else if (UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Comp))
				{
					if (Static->GetStaticMesh())
					{
						LocalBox = Static->GetStaticMesh()->GetBounds().GetBox();
					}
				}

				if (!LocalBox.IsValid)
				{
					LocalBox = Comp->CalcBounds(FTransform::Identity).GetBox();
				}

				if (LocalBox.IsValid)
				{
					FTransform CompToCamera = Comp->GetComponentTransform() * WorldToCamera;
					FVector Corners[8];
					LocalBox.GetVertices(Corners);
					for (int32 i = 0; i < 8; ++i)
					{
						CameraSpaceBox += CompToCamera.TransformPosition(Corners[i]);
					}
					bFoundAny = true;
				}
			}
		}

		FVector BoxCenter;
		FVector BoxSize;

		if (bFoundAny)
		{
			BoxCenter = CameraSpaceBox.GetCenter();
			BoxSize = CameraSpaceBox.GetSize();
		}
		else
		{
			// Absolute Fallback
			FVector Origin, Extent;
			CurrentSpawnedActor->GetActorBounds(false, Origin, Extent);
			BoxCenter = WorldToCamera.TransformPosition(Origin);
			BoxSize = Extent * 2.0f;
		}

		// Robustness check: Ensure BoxSize is not zero
		if (BoxSize.IsNearlyZero())
		{
			BoxSize = FVector(200.0f, 200.0f, 200.0f);
		}

		UCineCameraComponent* CameraComp = GetCineCameraComponent();
		CameraComp->AspectRatio = (float)ScreenshotWidth / (float)ScreenshotHeight;
		CameraComp->bConstrainAspectRatio = true;

		float HFOV = CameraComp->GetHorizontalFieldOfView();
		float VFOV = CameraComp->GetVerticalFieldOfView();
		if (HFOV <= 0.1f) HFOV = 90.0f;
		if (VFOV <= 0.1f) VFOV = 60.0f;

		float HFOVRad = FMath::DegreesToRadians(HFOV);
		float VFOVRad = FMath::DegreesToRadians(VFOV);

		// Distance needed to fit the box size in camera space
		float DistY = (BoxSize.Y * 0.5f) / (FillPercentage * FMath::Tan(HFOVRad * 0.5f));
		float DistZ = (BoxSize.Z * 0.5f) / (FillPercentage * FMath::Tan(VFOVRad * 0.5f));
		
		float DistanceToCenter = FMath::Max(DistY, DistZ) * CameraDistanceMultiplier;
		
		// If taking top/bottom shots, push the actor further away if multiplier is set
		if (bTakeScreenshotsFromAllSides && (CurrentSideIndex == 4 || CurrentSideIndex == 5))
		{
			DistanceToCenter += BoxSize.X * Entry.TopBottomExtraMultiplier;
		}

		// Requirement: Clamp distance between editable min and max
		DistanceToCenter = FMath::Clamp(DistanceToCenter, MinCameraDistance, MaxCameraDistance);
		
		// Position actor: move it so its camera-space bounds center is at the target distance
		FVector RelativeOffset = Entry.LocationOffset + GlobalLocationOffset + CameraWorldOffset;

		// Ensure the actor center stays within the field of view (correction)
		float MaxY = DistanceToCenter * FMath::Tan(HFOVRad * 0.5f) * 0.85f;
		float MaxZ = DistanceToCenter * FMath::Tan(VFOVRad * 0.5f) * 0.85f;
		RelativeOffset.Y = FMath::Clamp(RelativeOffset.Y, -MaxY, MaxY);
		RelativeOffset.Z = FMath::Clamp(RelativeOffset.Z, -MaxZ, MaxZ);

		FVector TargetBoxCenterInCameraSpace = FVector(DistanceToCenter, 0.f, 0.f) + RelativeOffset;
		
		FVector BoxCenterWorld = GetActorTransform().TransformPosition(BoxCenter);
		FVector TargetBoxCenterWorld = GetActorTransform().TransformPosition(TargetBoxCenterInCameraSpace);
		
		FVector Translation = TargetBoxCenterWorld - BoxCenterWorld;
		FVector NewActorLocation = CurrentSpawnedActor->GetActorLocation() + Translation;

		CurrentSpawnedActor->SetActorLocation(NewActorLocation);

		// Sharpness / Focus
		CameraComp->FocusSettings.FocusMethod = ECameraFocusMethod::Tracking;
		CameraComp->FocusSettings.TrackingFocusSettings.ActorToTrack = CurrentSpawnedActor;
		// Focus on the calculated bounds center
		CameraComp->FocusSettings.TrackingFocusSettings.RelativeOffset = CurrentSpawnedActor->GetActorTransform().InverseTransformPosition(BoxCenterWorld);

		UE_LOG(LogEasyShot, Log, TEXT("EasyShot: Target %d Side %d. BoxSize: %s, Distance: %f, Offset: %s, ActorLoc: %s"), 
			CurrentTargetIndex, CurrentSideIndex, *BoxSize.ToString(), DistanceToCenter, *RelativeOffset.ToString(), *NewActorLocation.ToString());

		// Small delay to let rendering catch up
		GetWorldTimerManager().SetTimer(StepTimerHandle, this, &AEasyShotCineCameraActor::CaptureScreenshot, 1.0f, false);
	}
	else
	{
		CurrentTargetIndex++;
		CurrentSideIndex = 0;
		ProcessNext();
	}
}

void AEasyShotCineCameraActor::SpawnCurrentTarget()
{
	CleanupCurrentTarget();

	if (!Targets.IsValidIndex(CurrentTargetIndex)) return;

	const FEasyShotTargetEntry& Entry = Targets[CurrentTargetIndex];
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Spawn with offsets initially
	FVector SpawnLocation = FVector::ZeroVector + Entry.LocationOffset + GlobalLocationOffset;
	FRotator SpawnRotation = Entry.RotationOffset + GlobalRotationOffset;

	AActor* TempActor = nullptr;

	// 1. If an ActorClass is selected
	if (Entry.ActorClass)
	{
		TempActor = GetWorld()->SpawnActor<AActor>(Entry.ActorClass, SpawnLocation, SpawnRotation, SpawnParams);
		
		if (TempActor)
		{
			UClass* ActorClass = TempActor->GetClass();
			bool bUseSkeletal = true;

			// Reflection: Check if bUseSkeletalMovement (RTSUnitTemplate) is false
			if (FProperty* SkeletalProp = ActorClass->FindPropertyByName(TEXT("bUseSkeletalMovement")))
			{
				if (FBoolProperty* BoolSkeletalProp = CastField<FBoolProperty>(SkeletalProp))
				{
					bUseSkeletal = BoolSkeletalProp->GetPropertyValue_InContainer(TempActor);
				}
			}

			// CASE: ISM mode (Option B: extract mesh and spawn in new actor)
			if (!bUseSkeletal)
			{
				UStaticMesh* ExtractedMesh = nullptr;
				if (FProperty* ISMProp = ActorClass->FindPropertyByName(TEXT("ISMComponent")))
				{
					if (FObjectProperty* ObjProp = CastField<FObjectProperty>(ISMProp))
					{
						if (UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(ObjProp->GetObjectPropertyValue_InContainer(TempActor)))
						{
							ExtractedMesh = ISM->GetStaticMesh();
						}
					}
				}

				if (ExtractedMesh)
				{
					TempActor->Destroy(); // Remove complex actor
					
					// Create a clean StaticMeshActor for the screenshot
					AStaticMeshActor* MeshActor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
					if (MeshActor)
					{
						MeshActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
						MeshActor->GetStaticMeshComponent()->SetStaticMesh(ExtractedMesh);
						CurrentSpawnedActor = MeshActor;
					}
				}
				else
				{
					CurrentSpawnedActor = TempActor; // Fallback
				}
			}
			else
			{
				CurrentSpawnedActor = TempActor; // Normal Skeletal Mesh mode
			}
		}
	}
	// 2. If a mesh is selected directly
	else if (Entry.Mesh)
	{
		AStaticMeshActor* MeshActor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
		if (MeshActor)
		{
			MeshActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			MeshActor->GetStaticMeshComponent()->SetStaticMesh(Entry.Mesh);
			CurrentSpawnedActor = MeshActor;
		}
	}

	// 3. Visibility and RTS options for the final screenshot actor
	if (CurrentSpawnedActor)
	{
		CurrentSpawnedActor->SetActorHiddenInGame(false);
		
		// Manage visibility for the screenshot actor
		TArray<USceneComponent*> Components;
		CurrentSpawnedActor->GetComponents<USceneComponent>(Components, true);
		for (USceneComponent* Comp : Components)
		{
			if (Comp)
			{
				// Identify helper components (capsules, arrows, etc.) to hide
				bool bIsHelper = Comp->IsA<UCapsuleComponent>() || 
								 Comp->IsA<UArrowComponent>() ||
								 Comp->GetName().Contains(TEXT("directionForwardVector"));

				if (bIsHelper)
				{
					Comp->SetHiddenInGame(true);
					Comp->SetVisibility(false, false);
				}
				else
				{
					Comp->SetHiddenInGame(false);
					Comp->SetVisibility(true, true);
				}
			}
		}

		// RTS-specific reflection adjustments
		UClass* FinalClass = CurrentSpawnedActor->GetClass();
		
		if (FProperty* FogProp = FinalClass->FindPropertyByName(TEXT("EnableFog")))
		{
			if (FBoolProperty* BoolFogProp = CastField<FBoolProperty>(FogProp))
				BoolFogProp->SetPropertyValue_InContainer(CurrentSpawnedActor, false);
		}

		if (FProperty* TeamProp = FinalClass->FindPropertyByName(TEXT("IsMyTeam")))
		{
			if (FBoolProperty* BoolTeamProp = CastField<FBoolProperty>(TeamProp))
				BoolTeamProp->SetPropertyValue_InContainer(CurrentSpawnedActor, true);
		}

		UFunction* VisFunc = CurrentSpawnedActor->FindFunction(TEXT("SetCharacterVisibility"));
		if (VisFunc)
		{
			struct FVisParams { bool bDesiredVisibility; };
			FVisParams Params;
			Params.bDesiredVisibility = true;
			CurrentSpawnedActor->ProcessEvent(VisFunc, &Params);
		}
	}
}

void AEasyShotCineCameraActor::CaptureScreenshot()
{
	FString Command = FString::Printf(TEXT("HighResShot %dx%d"), ScreenshotWidth, ScreenshotHeight);
	UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), Command, nullptr);

	// Wait 1 second after command to ensure it's captured before we move/cleanup
	GetWorldTimerManager().SetTimer(StepTimerHandle, this, &AEasyShotCineCameraActor::HandlePostScreenshot, 1.0f, false);
}

void AEasyShotCineCameraActor::HandlePostScreenshot()
{
	if (bTakeScreenshotsFromAllSides && (CurrentSideIndex < 5))
	{
		CurrentSideIndex++;
		ProcessNext();
	}
	else
	{
		CleanupCurrentTarget();
		CurrentTargetIndex++;
		CurrentSideIndex = 0;
		// Small delay before spawning next target
		GetWorldTimerManager().SetTimer(StepTimerHandle, this, &AEasyShotCineCameraActor::ProcessNext, 0.5f, false);
	}
}

void AEasyShotCineCameraActor::CleanupCurrentTarget()
{
	if (CurrentSpawnedActor)
	{
		CurrentSpawnedActor->Destroy();
		CurrentSpawnedActor = nullptr;
	}
}

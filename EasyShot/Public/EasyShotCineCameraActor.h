// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CineCameraActor.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"
#include "EasyShotCineCameraActor.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogEasyShot, Log, All);

USTRUCT(BlueprintType)
struct FEasyShotTargetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "The class of the actor to spawn for the screenshot."))
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "The static mesh to spawn if no ActorClass is specified."))
	UStaticMesh* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "Relative location offset for this specific target."))
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "Relative rotation offset for this specific target."))
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "Extra distance multiplier for top/bottom shots of this target."))
	float TopBottomExtraMultiplier = 0.0f;
};

UCLASS()
class EASYSHOT_API AEasyShotCineCameraActor : public ACineCameraActor
{
	GENERATED_BODY()

public:
	AEasyShotCineCameraActor(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "List of targets to take screenshots of."))
	TArray<FEasyShotTargetEntry> Targets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "Global location offset applied to all spawned targets."))
	FVector GlobalLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "Global rotation offset applied to all spawned targets."))
	FRotator GlobalRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "The width of the screenshot in pixels."))
	int32 ScreenshotWidth = 3840;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "The height of the screenshot in pixels."))
	int32 ScreenshotHeight = 2160;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "If true, takes screenshots from all 6 sides (Front, Right, Back, Left, Top, Bottom)."))
	bool bTakeScreenshotsFromAllSides = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "The percentage of the screen the target should fill (0.1 to 1.0).", ClampMin = "0.1", ClampMax = "1.0"))
	float FillPercentage = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "Multiplier for the calculated camera distance."))
	float CameraDistanceMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "Additional world-space offset for the camera position."))
	FVector CameraWorldOffset = FVector(0.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "Minimum distance the camera can be from the target."))
	float MinCameraDistance = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EasyShot", meta = (ToolTip = "Maximum distance the camera can be from the target."))
	float MaxCameraDistance = 5000.0f;

private:
	void ProcessNext();
	void SpawnCurrentTarget();
	void CaptureScreenshot();
	void HandlePostScreenshot();
	void CleanupCurrentTarget();

	int32 CurrentTargetIndex = 0;
	int32 CurrentSideIndex = 0;
	
	UPROPERTY()
	AActor* CurrentSpawnedActor = nullptr;

	FTimerHandle StepTimerHandle;
};

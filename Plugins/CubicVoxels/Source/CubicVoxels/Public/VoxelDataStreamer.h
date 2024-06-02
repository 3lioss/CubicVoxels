
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelDataStreamer.generated.h"


class AVoxelWorld;

UCLASS()
class CUBICVOXELS_API AVoxelDataStreamer : public AActor
{
	GENERATED_BODY()
	
public:
	// Sets default values for this actor's properties
	AVoxelDataStreamer();

	bool IsActive;
	TArray<uint8> DataToStream;
	int32 CurrentIndex;
	int32 MaxBytesPerStreamChunk;

	UPROPERTY()
	AVoxelWorld* ParentVoxelWorld;
	
	UPROPERTY()
	APlayerController* OwningPlayerController;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};

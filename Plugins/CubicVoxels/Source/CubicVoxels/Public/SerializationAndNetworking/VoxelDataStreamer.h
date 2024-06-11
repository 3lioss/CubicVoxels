
#pragma once

#include "CoreMinimal.h"
#include "ReplicationStructs.h"
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

	void AddDataToStream(const FVoxelStreamData* StreamDataPtr);
	
	UPROPERTY()
	APlayerController* OwningPlayerController;

	

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	TArray<const FVoxelStreamData*> StreamsToProcess;

	UFUNCTION(Client, Reliable)
	void SendVoxelStreamChunk(FVoxelStreamChunk Data);

	TArray<uint8> SerializedDataAccumulator;

	int32 CurrentIndex;

	int32 StreamMaxChunkSize;

	UFUNCTION(Client, Reliable)
	void CallEndFunctionOnClient(int32 StreamOwner, FName StreamType);

private:
	const FVoxelStreamData* CurrentStreamPtr;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};

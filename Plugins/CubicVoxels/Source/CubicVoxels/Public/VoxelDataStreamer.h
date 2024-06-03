
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

	void ActivateStreamer(const TArray<uint8>& DataToSend, const TFunction<void(TArray<uint8>)>& FunctionToCallOnDataPtr, int32 MaxBytesPerStreamChunk);
	
	UPROPERTY()
	APlayerController* OwningPlayerController;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	bool IsActive;
	TArray<uint8> DataToStream;
	int32 CurrentIndex;
	int32 ChunkSize;
	TFunction<void(TArray<uint8>)> FunctionToCallOnTransferFinishedPtr;

	UFUNCTION(Client, Reliable)
	void SendVoxelStreamChunk(FVoxelStreamChunk Data);

	TArray<uint8> SerializedDataAccumulator;


public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};


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

	//void ActivateStreamer(const TArray<uint8>& DataToSend, const TFunction<void(TArray<uint8>)>& FunctionToCallOnDataPtr, int32 MaxBytesPerStreamChunk);
	void AddDataToStream(FVoxelStreamData StreamData);
	
	UPROPERTY()
	APlayerController* OwningPlayerController;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	TQueue<FVoxelStreamData> StreamsQueue;

	UFUNCTION(Client, Reliable)
	void SendVoxelStreamChunk(FVoxelStreamChunk Data);

	TArray<uint8> SerializedDataAccumulator;

	FVoxelStreamData* CurrentStream;

	UFUNCTION(Client, Reliable)
	void CallEndFunctionOnClient(int32 StreamOwner, FName StreamType);

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};

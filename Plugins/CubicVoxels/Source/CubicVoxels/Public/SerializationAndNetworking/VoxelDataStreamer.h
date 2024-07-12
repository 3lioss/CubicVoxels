
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
	// WARNING!!!!! This actor will only work if it is not owned by a player controller AND OwningPlayerController is set to a valid object
	AVoxelDataStreamer();

	void AddDataToStream(const FVoxelStreamData* , AActor* StreamOriginActor);
	
	UPROPERTY()
	APlayerController* OwningPlayerController;

	

private:
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

	UFUNCTION(Client, Reliable)
	void AssignIDOnClient(int32 ID, AActor* StreamOriginActor);

	UFUNCTION(Server, Reliable)
	void NotifyServerThatIdHasBeenMapped(int32 ID);
	
	const FVoxelStreamData* CurrentStreamPtr;

	int32 LastAssignedStreamID;

	UPROPERTY()
	TMap<int32, AActor*> IDToActorMap; //Used only on the client, maps a stream ID to the actor that sent the stream

	TSet<int32> ValidIDs; //Used only on the server,set of the streams chose ID has already been correctly set on the client

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};

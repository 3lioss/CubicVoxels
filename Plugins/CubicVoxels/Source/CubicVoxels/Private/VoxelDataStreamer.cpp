// Fill out your copyright notice in the Description page of Project Settings.
#include "VoxelDataStreamer.h"
#include "GameFramework/PlayerController.h"
#include "CoreMinimal.h"
#include "Engine/ActorChannel.h"
#include "VoxelWorld.h"

// Sets default values
AVoxelDataStreamer::AVoxelDataStreamer()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	IsActive = false;
	CurrentIndex = 0;

	MaxBytesPerStreamChunk = 32000;
}

// Called when the game starts or when spawned
void AVoxelDataStreamer::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AVoxelDataStreamer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsValid(OwningPlayerController))
	{
		auto* Channel =  OwningPlayerController->NetConnection->FindActorChannelRef(this);
		while ( Channel->NumOutRec < (RELIABLE_BUFFER/2) && (CurrentIndex + MaxBytesPerStreamChunk < DataToStream.Num()))
		{
			FVoxelStreamChunk ChunkToSend;
			ChunkToSend.StartIndex = CurrentIndex;
			ChunkToSend.EndOfStreamIndex = DataToStream.Num();
			
			const auto N = FMath::Max(CurrentIndex + MaxBytesPerStreamChunk, DataToStream.Num() - 1);
			ChunkToSend.DataSlice.SetNum(N+1);
			for (int32 i = 0; i < N; i++)
			{
				ChunkToSend.DataSlice[i] = DataToStream[CurrentIndex + i];
			}
			
			CurrentIndex += N;
			
			ParentVoxelWorld->SendVoxelStreamChunk(ChunkToSend);
		}
	}
	
	// (CurrentIndex + MaxBytesPerStreamChunk < DataToStream.Num() &&
	
	
		// while (ChunksSent < ChunksToSend && Channel->NumOutRec < (RELIABLE_BUFFER / 2))
		// {
		// 	ChunkBuffer.Reset();
		// 	const int32 StartIndex = ChunksSent * MAXCHUNKSIZE;
		// 	const int32 NumElements = FMath::Min(MAXCHUNKSIZE, Data.Num() - StartIndex);
		//
		// 	check(NumElements > 0 && (StartIndex + NumElements - 1) < DataToStream.Num());
		// 	ChunkBuffer.Append(DataToStream.GetData() + StartIndex, NumElements);
		//
		// 	// Send a reliable Client RPC with the subarray data here
		// 	ClientReceiveData(ChunkBuffer);
		// 	ChunksSent++;
		// }
	
}


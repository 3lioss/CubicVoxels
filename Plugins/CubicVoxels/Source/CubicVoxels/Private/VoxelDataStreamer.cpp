// Fill out your copyright notice in the Description page of Project Settings.
#include "VoxelDataStreamer.h"
#include "GameFramework/PlayerController.h"
#include "CoreMinimal.h"
#include "Engine/ActorChannel.h"

// Sets default values
AVoxelDataStreamer::AVoxelDataStreamer()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	IsActive = false;
	CurrentIndex = 0;
	OwningPlayerController = nullptr;
	FunctionToCallOnTransferFinishedPtr = nullptr;

	ChunkSize = 32000;
}

void AVoxelDataStreamer::ActivateStreamer(const TArray<uint8>& DataToSend, const TFunction<void(TArray<uint8>)>& FunctionToCallOnDataPtr, int32 MaxBytesPerStreamChunk)
{
	DataToStream = DataToSend;
	ChunkSize = MaxBytesPerStreamChunk;

	CurrentIndex = 0;
	
	FunctionToCallOnTransferFinishedPtr = FunctionToCallOnDataPtr;

	IsActive = true;
}

// Called when the game starts or when spawned
void AVoxelDataStreamer::BeginPlay()
{
	Super::BeginPlay();
	
}

void AVoxelDataStreamer::SendVoxelStreamChunk_Implementation(FVoxelStreamChunk Data)
{
	if (Data.StartIndex + Data.DataSlice.Num() >= Data.EndOfStreamIndex)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid data chunk received while streaming world to client"))	
	}
	
	if (SerializedDataAccumulator.Num() == 0)
	{
		SerializedDataAccumulator.SetNum(Data.EndOfStreamIndex + 1);
	}

	for (int32 i = 0; i < Data.DataSlice.Num(); i++)
	{
		SerializedDataAccumulator[i + Data.StartIndex] = Data.DataSlice[i];
	}

	if (Data.StartIndex + Data.DataSlice.Num() == Data.EndOfStreamIndex - 1)
	{
		FunctionToCallOnTransferFinishedPtr(SerializedDataAccumulator);
		SerializedDataAccumulator.Empty();
	}
}

// Called every frame
void AVoxelDataStreamer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsValid(OwningPlayerController))
	{
		auto* Channel =  OwningPlayerController->NetConnection->FindActorChannelRef(this);
		while ( Channel->NumOutRec < (RELIABLE_BUFFER/2) && (CurrentIndex + ChunkSize < DataToStream.Num()))
		{
			FVoxelStreamChunk ChunkToSend;
			ChunkToSend.StartIndex = CurrentIndex;
			ChunkToSend.EndOfStreamIndex = DataToStream.Num();
			
			const auto N = FMath::Max(CurrentIndex + ChunkSize, DataToStream.Num() - 1);
			ChunkToSend.DataSlice.SetNum(N+1);
			for (int32 i = 0; i < N; i++)
			{
				ChunkToSend.DataSlice[i] = DataToStream[CurrentIndex + i];
			}
			
			CurrentIndex += N;
			
			SendVoxelStreamChunk(ChunkToSend);
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


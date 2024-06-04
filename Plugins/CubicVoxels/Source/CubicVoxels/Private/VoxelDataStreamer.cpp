// Fill out your copyright notice in the Description page of Project Settings.
#include "VoxelDataStreamer.h"
#include "GameFramework/PlayerController.h"
#include "CoreMinimal.h"
#include "VoxelStreamInterpretationInterface.h"
#include "Engine/ActorChannel.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AVoxelDataStreamer::AVoxelDataStreamer()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	CurrentStream = nullptr;

	OwningPlayerController = nullptr;
}

void AVoxelDataStreamer::AddDataToStream(FVoxelStreamData StreamData)
{
	StreamsQueue.Enqueue(StreamData);
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
	
}
//UObject* Object = GetWorld()->GetNetDriver()->GuidCache->GetObjectFromNetGUID(StreamOwner);

void AVoxelDataStreamer::CallEndFunctionOnClient_Implementation(int32 StreamOwner, FName StreamType)
{
		
	TArray<AActor*> OutActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), UVoxelStreamInterpretationInterface::StaticClass() , OutActors);
	 
	// OutActors contains all BP and C++ actors that are or inherit from AMyInterfaceActor
	for (AActor* CurrentActor : OutActors)
	{
		if (CurrentActor->GetUniqueID() == StreamOwner)
		Cast<IVoxelStreamInterpretationInterface>(CurrentActor)->InterpretVoxelStream(StreamOwner, StreamType);
	}
}

// Called every frame
void AVoxelDataStreamer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (CurrentStream)
	{
		if (IsValid(OwningPlayerController))
		{
			auto* Channel =  OwningPlayerController->NetConnection->FindActorChannelRef(this);
			while ( Channel->NumOutRec < (RELIABLE_BUFFER/2) && (CurrentStream->CurrentIndex < CurrentStream->DataToStream.Num()))
			{
				FVoxelStreamChunk ChunkToSend;
				ChunkToSend.StartIndex = CurrentStream->CurrentIndex;
				ChunkToSend.EndOfStreamIndex = CurrentStream->DataToStream.Num();
			
				const auto N = FMath::Max(CurrentStream->CurrentIndex + ChunkSize, CurrentStream->DataToStream.Num() - 1);
				ChunkToSend.DataSlice.SetNum(N+1);
				for (int32 i = 0; i < N; i++)
				{
					ChunkToSend.DataSlice[i] = CurrentStream->DataToStream[CurrentStream->CurrentIndex + i];
				}
			
				CurrentStream->CurrentIndex += N;
			
				SendVoxelStreamChunk(ChunkToSend);
			}

			if (CurrentStream->CurrentIndex >= CurrentStream->DataToStream.Num())
			{
				
			}
		}
	}
	else
	{
		auto NewStream = FVoxelStreamData();
		if (StreamsQueue.Dequeue(NewStream))
		{
			CurrentStream = &NewStream;
		}
		else
		{
			CurrentStream = nullptr;
		}
	}
	
}


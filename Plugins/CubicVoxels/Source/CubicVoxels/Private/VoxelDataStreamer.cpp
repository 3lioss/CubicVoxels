// Fill out your copyright notice in the Description page of Project Settings.
#include "SerializationAndNetworking/VoxelDataStreamer.h"
#include "GameFramework/PlayerController.h"
#include "CoreMinimal.h"
#include "VoxelWorld.h"
#include "SerializationAndNetworking/VoxelStreamInterpretationInterface.h"
#include "Engine/ActorChannel.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AVoxelDataStreamer::AVoxelDataStreamer()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	CurrentStreamPtr = nullptr;

	OwningPlayerController = nullptr;
	
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	
	CurrentIndex = 0;

	StreamMaxChunkSize = 10;

	LastAssignedStreamID = -1;
}

void AVoxelDataStreamer::AddDataToStream(const FVoxelStreamData* StreamDataPtr, AActor* StreamOriginActor)
{
	StreamDataPtr->StreamID = LastAssignedStreamID + 1;
	LastAssignedStreamID += 1;
	StreamsToProcess.Add(StreamDataPtr);
	AssignIDOnClient(StreamDataPtr->StreamID, StreamOriginActor);
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
		UE_LOG(LogTemp, Error, TEXT("Invalid data chunk received while streaming world to client at index %d"), Data.StartIndex)	
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

void AVoxelDataStreamer::CallEndFunctionOnClient_Implementation(int32 StreamID, FName StreamType)
{
	for (int32 i = 0; i < SerializedDataAccumulator.Num(); i++)
	{
		UE_LOG(LogTemp, Display, TEXT("The bit received at %d by the streamer is %d on the client"), i, SerializedDataAccumulator[i])
	}
	
	auto StreamOwnerPtr = IDToActorMap.FindAndRemoveChecked(StreamID);
	if (StreamOwnerPtr->Implements<UVoxelStreamInterpretationInterface>())
	{
		auto ActorToUseData = Cast<IVoxelStreamInterpretationInterface>(StreamOwnerPtr);
		ActorToUseData->InterpretVoxelStream(StreamType, SerializedDataAccumulator);
		SerializedDataAccumulator.Empty();
	}
}

void AVoxelDataStreamer::AssignIDOnClient_Implementation(int32 ID, AActor* StreamOriginActor)
{
	IDToActorMap.Add(ID, StreamOriginActor);
}

// Called every frame
void AVoxelDataStreamer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority())
	{
		if (CurrentStreamPtr)
		{
			if (IsValid(OwningPlayerController))
			{
				if (IsValid(OwningPlayerController->NetConnection))
				{
					auto Channel =  OwningPlayerController->NetConnection->FindActorChannelRef(this);

					if (IsValid(Channel))
					{
						while ( Channel->NumOutRec < (RELIABLE_BUFFER/2) &&  (CurrentIndex < CurrentStreamPtr->GetStreamData().Num()))
						{
					
						
							FVoxelStreamChunk ChunkToSend;
							ChunkToSend.StartIndex = CurrentIndex;
							ChunkToSend.EndOfStreamIndex = CurrentStreamPtr->GetStreamData().Num();
							const auto N = FMath::Min( StreamMaxChunkSize, CurrentStreamPtr->GetStreamData().Num() - CurrentIndex);
							ChunkToSend.DataSlice.SetNum(N+1);
							for (int32 i = 0; i < N; i++)
							{
								ChunkToSend.DataSlice[i] = CurrentStreamPtr->GetStreamData()[CurrentIndex + i];
							}
					
							CurrentIndex += N;
							SendVoxelStreamChunk(ChunkToSend);
					
						}
					
						if (CurrentIndex >= CurrentStreamPtr->GetStreamData().Num())
						{
							UE_LOG(LogTemp, Display, TEXT("Calling end function"));
							const auto a =  CurrentStreamPtr->StreamType;
							CallEndFunctionOnClient(CurrentStreamPtr->StreamID, CurrentStreamPtr->StreamType);
							delete CurrentStreamPtr;
							CurrentStreamPtr = nullptr;
							CurrentIndex = 0;
						}
					}
					
				}
				else
				{
					UE_LOG(LogTemp, Display, TEXT("Net channel not valid"));
				}
			
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("The owning player controller ptr is not valid"));
			}
		}
		else
		{
			if (!StreamsToProcess.IsEmpty())
			{
				CurrentStreamPtr = StreamsToProcess[0];
				StreamsToProcess.RemoveAt(0);
			}
		}
	}
}

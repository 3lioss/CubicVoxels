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
}

void AVoxelDataStreamer::AddDataToStream(const FVoxelStreamData* StreamDataPtr)
{
	StreamsToProcess.Add(StreamDataPtr);
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

void AVoxelDataStreamer::CallEndFunctionOnClient_Implementation(int32 StreamOwner, FName StreamType)
{
		
	TArray<AActor*> OutActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVoxelWorld::StaticClass() , OutActors);
	 UE_LOG(LogTemp, Warning, TEXT("Getting all actors implementing the interface"))
	 //OutActors contains all BP and C++ actors that are or inherit from AVoxelStreamInterpretationInterface
	 for (AActor* CurrentActor : OutActors)
	 {
	 	UE_LOG(LogTemp, Warning, TEXT("Actor net tag: %d, target net tag: %d"), CurrentActor->NetTag, StreamOwner); 
	 	if (CurrentActor->GetUniqueID() == StreamOwner)
	 	{
	 		UE_LOG(LogTemp, Warning, TEXT("Found target actor, launching interface method"))
	 		Cast<IVoxelStreamInterpretationInterface>(CurrentActor)->InterpretVoxelStream(StreamOwner, StreamType, SerializedDataAccumulator);
	 		SerializedDataAccumulator.Empty();
	 		break;
	 	}
	 }
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
						UE_LOG(LogTemp, Display, TEXT("Max index is the min of %d and %d"), StreamMaxChunkSize, CurrentStreamPtr->GetStreamData().Num() - CurrentIndex);
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
						for (int32 i = 0; i < CurrentStreamPtr->GetStreamData().Num(); i++)
						{
							UE_LOG(LogTemp, Display, TEXT("The bit received at %d by the streamer is %d on the client"), i, CurrentStreamPtr->GetStreamData()[i])
						}
						UE_LOG(LogTemp, Warning, TEXT("The stream owner is %d"), CurrentStreamPtr->StreamOwner);
						const auto a =  CurrentStreamPtr->StreamType;
						CallEndFunctionOnClient(CurrentStreamPtr->StreamOwner, CurrentStreamPtr->StreamType);
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


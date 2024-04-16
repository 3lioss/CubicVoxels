// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelWorld.h"
#include "Enums.h"
#include "FVoxelThread.h"
#include "VoxelWorldDataSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Chunk.h"

// Sets default values
AVoxelWorld::AVoxelWorld()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	
	ViewDistance = 32;
	VerticalViewDistance = 5;
	
	ViewLayers = TArray<TSet<FIntVector>>();
	ViewLayers.SetNum(ViewDistance + 1);

	//Initialize the list that gives the order in which chunks should be iterated
	auto FirstViewLayer = TSet<FIntVector>();
	FirstViewLayer.Add(FIntVector(0,0,0));
	ViewLayers[0] = FirstViewLayer;
	
	for (int32 i = 0; i < ViewDistance; i++)
	{
		for (auto& LayerElement : ViewLayers[i])
		{
			for (auto& Offset : {FIntVector(1,0,0), FIntVector(-1,0,0), FIntVector(0,1,0), FIntVector(0,-1,0), FIntVector(0,0,1), FIntVector(0,0,-1)})
			{
				if ( abs((LayerElement + Offset).Z) < VerticalViewDistance &&  OneNorm(LayerElement + Offset) > OneNorm(LayerElement) ) //To ponder...
				{
					ViewLayers[i+1].Add(LayerElement + Offset);
				}
			}
		}
	}
	
	RootComponent = CreateDefaultSubobject<USceneComponent>("Voxel world root");
	SetRootComponent(RootComponent);
	
	ChunkStates = TMap<FIntVector, EChunkState>();
	ChunkActorsMap = TMap<FIntVector, TObjectPtr<AChunk>>();
	ChunksToUnloadOnGivenTick = TArray<FIntVector>();

	WorldGenerationFunction = &DefaultGenerateBlockAt;
}

void AVoxelWorld::IterateChunkLoading(FVector PlayerPosition)
{
	
	const auto LoadingOrigin = FIntVector(this->GetActorRotation().GetInverse().RotateVector((PlayerPosition - this->GetActorLocation())/(ChunkSize*DefaultVoxelSize*this->GetActorScale().X)));
	PlayerPositionUpdatesPtr->Enqueue(TTuple<FIntVector, float>(LoadingOrigin, GetGameTimeSinceCreation()));
	//Logic for checking for every chunk in the vicinity of the player whether that chunk has been loaded, and if not load it
	
	for (int32 i = 0; i < ViewDistance; i++)
	{
		for (auto& Chunk : ViewLayers[i])
		{
			if (!ChunkStates.Contains( Chunk + LoadingOrigin ))
			{
				
					ChunkStates.Add(Chunk + LoadingOrigin, EChunkState::Loading);
					//(new FAutoDeleteAsyncTask<FVoxelChunkInsidesCookingAsyncTask>(Chunk + LoadingOrigin, &PreCookedChunksToLoadBlockData, &ChunkQuadsToLoad, ChunkSize, DefaultVoxelSize, WorldGenerationFunction ))->StartBackgroundTask();
					const auto RegionSavedData = GetRegionSavedData(GetRegionOfChunk(Chunk + LoadingOrigin) );
					if (RegionSavedData)
					{
						/*for (int k = 0; k <ChunkSavedData->ChunkData.Num(); k++ )
						{
							UE_LOG(LogTemp, Warning, TEXT("%d"), ChunkSavedData->ChunkData[k].StackSize );
						}*/
						
						const auto ChunkSavedData = RegionSavedData->Find(Chunk + LoadingOrigin);
						if (ChunkSavedData)
						{
							const TSharedPtr<FChunkData> CompressedChunkBlocksPtr(new FChunkData);
							(*CompressedChunkBlocksPtr) = *ChunkSavedData;
							auto ChunkGenerationOrder = FChunkThreadedWorkOrderBase();
							ChunkGenerationOrder.BlockSize = DefaultVoxelSize;
							ChunkGenerationOrder.ChunkSize = ChunkSize;
							ChunkGenerationOrder.CompressedChunkBlocksPtr = CompressedChunkBlocksPtr;
							ChunkGenerationOrder.OutputChunkDataQueuePtr = &GeneratedChunksToLoadInGame;
							ChunkGenerationOrder.OutputChunkFacesQueuePtr = &ChunkQuadsToLoad;
							ChunkGenerationOrder.ChunkLocation = Chunk + LoadingOrigin;
							ChunkGenerationOrder.OrderType = EChunkThreadedWorkOrderType::MeshingFromData;
							ChunkThreadedWorkOrdersQueuePtr->Enqueue(ChunkGenerationOrder);
							//ComputeInsideFacesOfLoadedChunk( Chunk + LoadingOrigin, &PreCookedChunksToLoadBlockData, &ChunkQuadsToLoad, ChunkSize, DefaultVoxelSize, CompressedChunkBlocksPtr);
						}
						else
						{
							auto ChunkGenerationOrder = FChunkThreadedWorkOrderBase();
							ChunkGenerationOrder.BlockSize = DefaultVoxelSize;
							ChunkGenerationOrder.ChunkSize = ChunkSize;
							ChunkGenerationOrder.GenerationFunction = WorldGenerationFunction;
							ChunkGenerationOrder.OutputChunkDataQueuePtr = &GeneratedChunksToLoadInGame;
							ChunkGenerationOrder.OutputChunkFacesQueuePtr = &ChunkQuadsToLoad;
							ChunkGenerationOrder.ChunkLocation = Chunk + LoadingOrigin;
							ChunkGenerationOrder.OrderType = EChunkThreadedWorkOrderType::GenerationAndMeshing;
							ChunkThreadedWorkOrdersQueuePtr->Enqueue(ChunkGenerationOrder);
						}
					}
					else
					{
						auto ChunkGenerationOrder = FChunkThreadedWorkOrderBase();
						ChunkGenerationOrder.BlockSize = DefaultVoxelSize;
						ChunkGenerationOrder.ChunkSize = ChunkSize;
						ChunkGenerationOrder.GenerationFunction = WorldGenerationFunction;
						ChunkGenerationOrder.OutputChunkDataQueuePtr = &GeneratedChunksToLoadInGame;
						ChunkGenerationOrder.OutputChunkFacesQueuePtr = &ChunkQuadsToLoad;
						ChunkGenerationOrder.ChunkLocation = Chunk + LoadingOrigin;
						ChunkGenerationOrder.OrderType = EChunkThreadedWorkOrderType::GenerationAndMeshing;
						ChunkThreadedWorkOrdersQueuePtr->Enqueue(ChunkGenerationOrder);
					}
			}
		}
	}
	
	while(!GeneratedChunksToLoadInGame.IsEmpty())
	{
		TTuple<FIntVector, TSharedPtr<FChunkData>> DataToLoad;
		GeneratedChunksToLoadInGame.Dequeue(DataToLoad);
		OrderedGeneratedChunksToLoadInGame.Add(DataToLoad);
	}
	
	OrderedGeneratedChunksToLoadInGame.Sort([this](TTuple<FIntVector, TSharedPtr<FChunkData>> A, TTuple<FIntVector, TSharedPtr<FChunkData>> B) {
		return OneNorm(A.Get<0>()) < OneNorm(B.Get<0>()); // sort the pre-cooked chunks by distance to player
	});
	
	//Load the chunks which have been pre-cooked asynchronously, in the order of distance to the player
	
	
	for (int32 Index = 0; Index < OrderedGeneratedChunksToLoadInGame.Num(); Index++)
	{
		const TTuple<FIntVector, TSharedPtr<FChunkData>> DataToLoad = OrderedGeneratedChunksToLoadInGame[Index];
		if ( !( DataToLoad.Get<1>()->CompressedChunkData.Num() == 1 && DataToLoad.Get<1>()->CompressedChunkData[0].Voxel.VoxelType == "Air") )
		{
			//Spawn the chunk actor
			const TObjectPtr<AChunk> NewChunk = GetWorld()->SpawnActor<AChunk>();

			NewChunk->OwningWorld = this;
			NewChunk->VoxelCharacteristicsData = VoxelPhysicalCharacteristicsTable;
			NewChunk->Location = DataToLoad.Get<0>();
			//NewChunk->SetActorLocationAndRotation(/*this->GetActorRotation().RotateVector( this->GetActorLocation() + this->GetActorScale().X**/ChunkSize*DefaultVoxelSize*FVector(DataToLoad.Get<0>()) /*)*/, /*this->GetActorRotation()*/ FRotator());
			NewChunk->SetActorLocationAndRotation(this->GetActorRotation().RotateVector( this->GetActorLocation() + this->GetActorScale().X*ChunkSize*DefaultVoxelSize*FVector(DataToLoad.Get<0>()) ), this->GetActorRotation() );
			NewChunk->SetActorScale3D(this->GetActorScale());
			NewChunk->LoadBlocks(DataToLoad.Get<1>());
			NewChunk->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			
			ChunkStates.Add(DataToLoad.Get<0>(), EChunkState::Loaded);
			ChunkActorsMap.Add(MakeTuple(DataToLoad.Get<0>(), NewChunk));          

			//If any neighbouring chunk is loaded, compute asynchronously the chunk's corresponding side mesh data
			const FIntVector Directions[6] = {
				FIntVector(1,0,0),
				FIntVector(0,1,0),
				FIntVector(-1,0,0),
				FIntVector(0,-1,0),
				FIntVector(0,0,1),
				FIntVector(0,0,-1)							
			};

			const int32 OppositeDirections[6] = {
				2,
				3,
				0,
				1,
				5,
				4							
			};
			
			//logic to start the generation of chunk sides if possible
			for (int32 i = 0; i<6; i++)
			{
				const auto StatePtr = ChunkStates.Find(DataToLoad.Get<0>() + Directions[i]);
				if (StatePtr && *StatePtr == EChunkState::Loaded)
				{
					const auto ActorPtr = ChunkActorsMap.Find(DataToLoad.Get<0>() + Directions[i]);
					if ( ActorPtr && IsValid(*ActorPtr))
					{
						(new FAutoDeleteAsyncTask<FVoxelChunkSideCookingAsyncTask>((NewChunk->BlocksData).Get(), ((*ActorPtr)->BlocksData).Get(), i,  &ChunkQuadsToLoad, DataToLoad.Get<0>()))->StartBackgroundTask();
						(new FAutoDeleteAsyncTask<FVoxelChunkSideCookingAsyncTask>(((*ActorPtr)->BlocksData).Get(), (NewChunk->BlocksData).Get(),  OppositeDirections[i], &ChunkQuadsToLoad, DataToLoad.Get<0>() + Directions[i]))->StartBackgroundTask();
						
					}
				}
				
			}
		}
	}

	OrderedGeneratedChunksToLoadInGame.Empty();
}

void AVoxelWorld::IterateChunkMeshing()
{
	//Add sides to chunks whose sides' mesh data have been computed
	
	while(!ChunkQuadsToLoad.IsEmpty())
	{
		
		TTuple<FIntVector, TMap<FIntVector4, FVoxel>> DataToLoad;
		ChunkQuadsToLoad.Dequeue(DataToLoad);
		const auto LoadingState = ChunkStates.Find(DataToLoad.Get<0>());
		if (LoadingState)
		{
			if (*LoadingState == EChunkState::Loaded)
			{
				const auto ChunkActor = ChunkActorsMap.Find(DataToLoad.Get<0>());
				if (ChunkActor && IsValid(*ChunkActor))
				{
					(*ChunkActor)->AddQuads(DataToLoad.Get<1>());
					(*ChunkActor)->RenderChunk(DefaultVoxelSize);
				}
				else
				{
					ChunkQuadsToBeLoadedLater.Enqueue(DataToLoad);
				}
				//If later chunks are to be forbidden from loading based on culling methods, there ought to be added logic for creating a ChunkActor just to add its sides
			}
			if (*LoadingState == EChunkState::Loading)
			{
				ChunkQuadsToBeLoadedLater.Enqueue(DataToLoad);
			}
		}
	}

	while(!ChunkQuadsToBeLoadedLater.IsEmpty())
	{
		TTuple<FIntVector, TMap<FIntVector4, FVoxel>> DataToLoad;
		ChunkQuadsToBeLoadedLater.Dequeue(DataToLoad);
		ChunkQuadsToLoad.Enqueue(DataToLoad);
	}
}

void AVoxelWorld::IterateChunkUnloading(FVector PlayerPosition)
{

	const auto LoadingOrigin =	FIntVector(this->GetActorRotation().GetInverse().RotateVector((PlayerPosition - this->GetActorLocation())/(ChunkSize*DefaultVoxelSize*this->GetActorScale().X)));
	

	for (auto& RegisteredChunkState : ChunkStates)
	{
		
		if (RegisteredChunkState.Value == EChunkState::Loaded && !ChunksToSave.Contains(RegisteredChunkState.Key) && OneNorm(RegisteredChunkState.Key - LoadingOrigin) > ViewDistance + 2 )
		{
			ChunkStates[RegisteredChunkState.Key] = EChunkState::Unloading;
			ChunksToUnloadOnGivenTick.Add(RegisteredChunkState.Key);
			
			if (ChunkActorsMap.Contains(RegisteredChunkState.Key))
			{
				ChunkActorsMap[RegisteredChunkState.Key]->Destroy();
			}
			
		}
	}
	for (auto& ChunkToUnload : ChunksToUnloadOnGivenTick)
	{
		ChunkStates.Remove(ChunkToUnload);
	}
	ChunksToUnloadOnGivenTick.Empty();

}

bool AVoxelWorld::IsChunkLoaded(FIntVector ChunkLocation) //tests if the chunk is marked as loaded in the chunkstates map
{
	if (ChunkStates.Contains(ChunkLocation))
	{
		return (ChunkStates[ChunkLocation] == EChunkState::Loaded);
	}
	else
	{
		return false;
	}
}

TMap<FIntVector, FChunkData>* AVoxelWorld::GetRegionSavedData(FIntVector ChunkRegionLocation)
{
	const auto LoadedRegionData = LoadedRegions.Find(ChunkRegionLocation);

	if (LoadedRegionData)
	{
		return LoadedRegionData;
	}

	else
	{
		FString RegionCoordinates = "";
		RegionCoordinates.AppendInt(ChunkRegionLocation.X);
		RegionCoordinates.Append(",");
		RegionCoordinates.AppendInt(ChunkRegionLocation.Y);
		RegionCoordinates.Append(",");
		RegionCoordinates.AppendInt(ChunkRegionLocation.Z);
		const FString& SaveSlotName = RegionCoordinates;

		if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
		{
			auto Temp = Cast<UVoxelWorldDataSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
			if (Temp)
			{
				LoadedRegions.Add(ChunkRegionLocation, (Temp->RegionData));
				return &(Temp->RegionData);
			}
			else
			{
				return nullptr;
			}
		}
		else
		{
			return nullptr;
		}
	}
	
}

TObjectPtr<AChunk> AVoxelWorld::GetChunkAt(FIntVector ChunkLocation)
{
	if(ChunkActorsMap.Contains(ChunkLocation))
	{
		return ChunkActorsMap[ChunkLocation];
	}
	else
	{
		return nullptr;
	}
}

void AVoxelWorld::SetChunkSavedData(FIntVector ChunkLocation, FChunkData NewData)
{
	const auto LoadedRegionData = LoadedRegions.Find(GetRegionOfChunk(ChunkLocation));

	if (LoadedRegionData)
	{
		const auto CurrentChunkData = LoadedRegionData->Find(ChunkLocation);
		if (CurrentChunkData)
		{
			CurrentChunkData->CompressedChunkData = NewData.CompressedChunkData;
		}
	}

	else
	{
		FString RegionCoordinates = "";
		RegionCoordinates.AppendInt(GetRegionOfChunk(ChunkLocation).X);
		RegionCoordinates.Append(",");
		RegionCoordinates.AppendInt(GetRegionOfChunk(ChunkLocation).Y);
		RegionCoordinates.Append(",");
		RegionCoordinates.AppendInt(GetRegionOfChunk(ChunkLocation).Z);
		const FString& SaveSlotName = RegionCoordinates;

		if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
		{
			auto Temp = Cast<UVoxelWorldDataSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
			if (Temp)
			{
				LoadedRegions.Add(GetRegionOfChunk(ChunkLocation), (Temp->RegionData));
				const auto CurrentChunkData = LoadedRegionData->Find(ChunkLocation);
				if (CurrentChunkData)
				{
					CurrentChunkData->CompressedChunkData = NewData.CompressedChunkData;
				}
			}
			else
			{
				UE_LOG(LogTemp,Error, TEXT("Encountered invalid save data while loading a region"))
			}
		}
		else
		{
			auto NewRegion =  TMap<FIntVector, FChunkData>();
			NewRegion.Add(ChunkLocation, NewData);
			LoadedRegions.Add(GetRegionOfChunk(ChunkLocation), NewRegion);
		}
	}

	RegisterChunkForSaving(ChunkLocation);
}

void AVoxelWorld::RegisterChunkForSaving(FIntVector3 ChunkLocation)
{
	ChunksToSave.Add(ChunkLocation);
	RegionsToSave.Add(GetRegionOfChunk(ChunkLocation));
}

void AVoxelWorld::SaveVoxelWorld() 
{
	
	for (auto& CurrentRegionToSave : RegionsToSave) //iterate over the regions to save 
	{
		TMap<FIntVector, FChunkData> CurrentRegionData = TMap<FIntVector, FChunkData>();
		
		auto RegionSaveData = GetRegionSavedData(CurrentRegionToSave);
		if (RegionSaveData)
		{
			CurrentRegionData = *RegionSaveData;
		}

		for(int32 x = 0; x < RegionSize; x++)
		{
			for(int32 y = 0; y < RegionSize; y++)
			{
				for(int32 z = 0; z < RegionSize; z++)
				{
					auto const CurrentChunk = FIntVector(RegionSize*CurrentRegionToSave.X + x, RegionSize*CurrentRegionToSave.Y + y, RegionSize*CurrentRegionToSave.Z + z);
					
					if (ChunksToSave.Contains(CurrentChunk)) //for every chunk in the region, check if the chunk must be saved
					{
						
						auto Temp = ChunkActorsMap.Find(CurrentChunk);
						if (Temp)
						{
							if (IsValid(*Temp))
							{
								CurrentRegionData.Add(CurrentChunk, *((*Temp)->BlocksData) );
							}
						}
					}
					
				}
			}
		}

		//save the region at hand
		auto SaveObject = Cast<UVoxelWorldDataSaveGame>(UGameplayStatics::CreateSaveGameObject(UVoxelWorldDataSaveGame::StaticClass()));

		SaveObject->RegionData = CurrentRegionData;
	
		FString ChunkCoordinates = "";
		ChunkCoordinates.AppendInt(CurrentRegionToSave.X);
		ChunkCoordinates.Append(",");
		ChunkCoordinates.AppendInt(CurrentRegionToSave.Y);
		ChunkCoordinates.Append(",");
		ChunkCoordinates.AppendInt(CurrentRegionToSave.Z);
		const FString& SaveSlotName = ChunkCoordinates;
	
		UGameplayStatics::SaveGameToSlot(SaveObject, SaveSlotName, 0 );
		
	}

	ChunksToSave.Empty();
}

void AVoxelWorld::DestroyBlockAt(FVector BlockWorldLocation)
{
	//Check if the chunk affected by the edit is loaded
	const auto temp = (BlockWorldLocation - this->GetActorLocation())/(DefaultVoxelSize*ChunkSize*this->GetActorScale().X);
	const auto AffectedChunkLocation = FIntVector(FMath::Floor(temp.X), FMath::Floor(temp.Y), FMath::Floor(temp.Z));

	if (IsChunkLoaded(AffectedChunkLocation))
	{
		
		const auto ChunkPtr = GetChunkAt(AffectedChunkLocation);
		if (ChunkPtr)
		{
			ChunkPtr->DestroyBlockAt(BlockWorldLocation); //TODO: correct the problem with this function (appears to target the wrong chunk)
		}
		//If the chunk is marked as loaded but no actor is registered for it, it means the chunk is actually empty and there is nothing to remove
	}
	else
	{
		const auto RegionDataPtr = GetRegionSavedData(GetRegionOfChunk(AffectedChunkLocation));
		
		if (RegionDataPtr)
		{
			const auto BlockLocationInChunk = FIntVector(BlockWorldLocation/DefaultVoxelSize) - AffectedChunkLocation*ChunkSize;
			LoadedRegions[GetRegionOfChunk(AffectedChunkLocation)][AffectedChunkLocation].RemoveVoxel(BlockLocationInChunk);
		}
		else
		{
			//TODO: find a way to handle this case
		}
	}
	//Otherwise, just edit the disk region data
	
}

void AVoxelWorld::SetBlockAt(FVector BlockWorldLocation, FVoxel Block)
{
	//Check if the chunk affected by the edit is loaded
	const auto temp = (BlockWorldLocation - this->GetActorLocation())/(DefaultVoxelSize*ChunkSize*this->GetActorScale().X);
	const auto AffectedChunkLocation = FIntVector(FMath::Floor(temp.X), FMath::Floor(temp.Y), FMath::Floor(temp.Z));

	if (IsChunkLoaded(AffectedChunkLocation))
	{
		
		const auto ChunkPtr = GetChunkAt(AffectedChunkLocation);
		if (ChunkPtr)
		{
			ChunkPtr->SetBlockAt(BlockWorldLocation, Block); //TODO: correct the problem with this function (appears to target the wrong chunk)
		}
		//TODO: handle this case by creating a new chunk actor just to place block in
	}
	else
	{
		const auto RegionDataPtr = GetRegionSavedData(GetRegionOfChunk(AffectedChunkLocation));
		
		if (RegionDataPtr)
		{
			const auto BlockLocationInChunk = FIntVector(BlockWorldLocation/DefaultVoxelSize) - AffectedChunkLocation*ChunkSize;
			LoadedRegions[GetRegionOfChunk(AffectedChunkLocation)][AffectedChunkLocation].SetVoxel(BlockLocationInChunk, Block);
		}
		else
		{
			//TODO: find a way to handle this case
		}
	}
	//Otherwise, just edit the disk region data
	
}

FVoxel AVoxelWorld::DefaultGenerateBlockAt(FVector Position)
{
	if ((Position.Z < 10000*FMath::PerlinNoise2D(FVector2d(Position.X, Position.Y)/10000 ) ) )
	{
		FVoxel Temp;
		Temp.VoxelType = "Stone";
		Temp.IsTransparent = false;
		Temp.IsSolid = true;
		return Temp;

	}
	else
	{
		if (Position.Z < -100)
		{
			FVoxel Temp;
			Temp.VoxelType = "Water";
			Temp.IsTransparent = true;
			return Temp;
		}
		else
		{
			return DefaultVoxel;
		}
		
	}
}



FIntVector AVoxelWorld::GetRegionOfChunk(FIntVector ChunkCoordinates)
{
	const int32 RegionX = floor( static_cast<float>(ChunkCoordinates.X) / RegionSize);
	const int32 RegionY = floor( static_cast<float>(ChunkCoordinates.Y) / RegionSize);
	const int32 RegionZ = floor( static_cast<float>(ChunkCoordinates.Z) / RegionSize);

	return FIntVector(RegionX, RegionY, RegionZ);
}

// Called when the game starts or when spawned
void AVoxelWorld::BeginPlay()
{
	Super::BeginPlay();
	
	WorldGenerationThread = new FVoxelThread();
	ChunkThreadedWorkOrdersQueuePtr = WorldGenerationThread->GetGenerationOrdersQueue();
	PlayerPositionUpdatesPtr = WorldGenerationThread->GetPlayerUpdatesQueue();
}

void AVoxelWorld::BeginDestroy()
{
	Super::BeginDestroy();
	if (WorldGenerationThread)
	{
		WorldGenerationThread->StartShutdown();
	}
}


// Called every frame
void AVoxelWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	IterateChunkLoading(GetWorld()->GetFirstPlayerController()->GetPawnOrSpectator()->GetActorLocation() );
	
	IterateChunkMeshing();

	if (!ChunksToSave.IsEmpty())
	{
		SaveVoxelWorld();

	}
	
	IterateChunkUnloading(GetWorld()->GetFirstPlayerController()->GetPawnOrSpectator()->GetActorLocation());

}

int32 AVoxelWorld::OneNorm(FIntVector Vector) const
{
	return abs(Vector.X) + abs(Vector.Y) + abs(Vector.Z);
}

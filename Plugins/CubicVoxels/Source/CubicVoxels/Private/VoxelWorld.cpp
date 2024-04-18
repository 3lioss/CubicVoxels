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
	
	ViewDistance = 5;
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
	NumbersOfPlayerOutsideRangeOfChunkMap = TMap<FIntVector, int32>();

	WorldGenerationFunction = &DefaultGenerateBlockAt;
}

void AVoxelWorld::IterateChunkLoading( )
{
	/*Register all chunks in loading distance for loading for each managed player*/

	for (const auto CurrentPlayerThreadPair : ManagedPlayerDataMap)
	{
		if (IsValid(CurrentPlayerThreadPair.Key))
		{
			auto PlayerWorkOrdersQueuePtr = CurrentPlayerThreadPair.Value.ChunkThreadedWorkOrdersQueuePtr;
			auto PlayerPositionUpdatesQueuePtr = CurrentPlayerThreadPair.Value.PositionUpdatesQueuePtr;

			FVector PlayerPosition = CurrentPlayerThreadPair.Key->GetPawn()->GetActorLocation();
			
				const auto LoadingOrigin = FIntVector(this->GetActorRotation().GetInverse().RotateVector((PlayerPosition - this->GetActorLocation())/(ChunkSize*DefaultVoxelSize*this->GetActorScale().X)));
				PlayerPositionUpdatesQueuePtr->Enqueue(TTuple<FIntVector, float>(LoadingOrigin, GetGameTimeSinceCreation()));
				//Logic for checking for every chunk in the vicinity of the player whether that chunk has been loaded, and if not load it
	
				for (int32 i = 0; i < ViewDistance; i++)
				{
					for (auto& Chunk : ViewLayers[i])
					{
						if (!ChunkStates.Contains( Chunk + LoadingOrigin ))
						{
				
							ChunkStates.Add(Chunk + LoadingOrigin, EChunkState::Loading);
							const auto RegionSavedData = GetRegionSavedData(GetRegionOfChunk(Chunk + LoadingOrigin) );
							if (RegionSavedData)
							{
						
						
								const auto ChunkSavedData = RegionSavedData->Find(Chunk + LoadingOrigin);
								if (ChunkSavedData)
								{
									const TSharedPtr<FChunkData> CompressedChunkBlocksPtr(new FChunkData);
									(*CompressedChunkBlocksPtr) = *ChunkSavedData;
									auto ChunkGenerationOrder = FChunkThreadedWorkOrderBase();
							
									ChunkGenerationOrder.CompressedChunkBlocksPtr = CompressedChunkBlocksPtr;
									ChunkGenerationOrder.OutputChunkDataQueuePtr = &GeneratedChunksToLoadInGame;
									ChunkGenerationOrder.OutputChunkFacesQueuePtr = &ChunkQuadsToLoad;
									ChunkGenerationOrder.ChunkLocation = Chunk + LoadingOrigin;

									if (CompressedChunkBlocksPtr->IsAdditive)
									{
										ChunkGenerationOrder.OrderType = EChunkThreadedWorkOrderType::GeneratingAndMeshingWithAdditiveData;
										ChunkGenerationOrder.GenerationFunction = WorldGenerationFunction;
									}
									else
									{
										ChunkGenerationOrder.OrderType = EChunkThreadedWorkOrderType::MeshingFromData;
									}
							
									PlayerWorkOrdersQueuePtr->Enqueue(ChunkGenerationOrder);

								}
								else
								{
									auto ChunkGenerationOrder = FChunkThreadedWorkOrderBase();
		
									ChunkGenerationOrder.GenerationFunction = WorldGenerationFunction;
									ChunkGenerationOrder.OutputChunkDataQueuePtr = &GeneratedChunksToLoadInGame;
									ChunkGenerationOrder.OutputChunkFacesQueuePtr = &ChunkQuadsToLoad;
									ChunkGenerationOrder.ChunkLocation = Chunk + LoadingOrigin;
									ChunkGenerationOrder.OrderType = EChunkThreadedWorkOrderType::GenerationAndMeshing;
									PlayerWorkOrdersQueuePtr->Enqueue(ChunkGenerationOrder);
								}
							}
							else
							{
								auto ChunkGenerationOrder = FChunkThreadedWorkOrderBase();
					
								ChunkGenerationOrder.GenerationFunction = WorldGenerationFunction;
								ChunkGenerationOrder.OutputChunkDataQueuePtr = &GeneratedChunksToLoadInGame;
								ChunkGenerationOrder.OutputChunkFacesQueuePtr = &ChunkQuadsToLoad;
								ChunkGenerationOrder.ChunkLocation = Chunk + LoadingOrigin;
								ChunkGenerationOrder.OrderType = EChunkThreadedWorkOrderType::GenerationAndMeshing;
								PlayerWorkOrdersQueuePtr->Enqueue(ChunkGenerationOrder);
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
	}
}

void AVoxelWorld::IterateChunkMeshing()
{
	/*Transmit to the chunk actors their data when it has finished being generated asynchronously */
	
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

void AVoxelWorld::IterateChunkUnloading()
{
	/*Unload all chunks beyond loading distance*/
	for (const auto CurrentPair : ManagedPlayerDataMap)
	{
		if (IsValid(CurrentPair.Key))
		{
			const auto LoadingOrigin =	FIntVector(this->GetActorRotation().GetInverse().RotateVector((CurrentPair.Key->GetPawn()->GetActorLocation() - this->GetActorLocation())/(ChunkSize*DefaultVoxelSize*this->GetActorScale().X)));
	
			for (auto& RegisteredChunkState : ChunkStates)
			{
				if (RegisteredChunkState.Value == EChunkState::Loaded && !ChunksToSave.Contains(RegisteredChunkState.Key) && OneNorm(RegisteredChunkState.Key - LoadingOrigin) > ViewDistance + 2 )
				{
					ChunkStates[RegisteredChunkState.Key] = EChunkState::Unloading;
					if (NumbersOfPlayerOutsideRangeOfChunkMap.Contains(RegisteredChunkState.Key))
					{
						NumbersOfPlayerOutsideRangeOfChunkMap[RegisteredChunkState.Key] += 1;
					}
					else
					{
						NumbersOfPlayerOutsideRangeOfChunkMap.Add(RegisteredChunkState.Key, 1);
					}
				}
			}
		}
	}
	for (auto& ChunkUnloadingScore : NumbersOfPlayerOutsideRangeOfChunkMap)
	{
		if (ChunkUnloadingScore.Value == ManagedPlayerDataMap.Num())
		{
			ChunkStates.Remove(ChunkUnloadingScore.Key);
			ChunkActorsMap[ChunkUnloadingScore.Key]->Destroy();
		}
	}
	NumbersOfPlayerOutsideRangeOfChunkMap.Empty();
	
}

bool AVoxelWorld::IsChunkLoaded(FIntVector ChunkLocation) 
{
	/*Tests if a chunk is marked as loaded. A true return value doesn't necessarily mean its chunk data is finished loading*/
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
	/*Get the saved data of a given region */
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
	/*Get the actor of the chunk at a given location if it exists*/
	
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
	/*Sets the data of a chunk on the save file directly*/
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
	/*Mark a chunk as needing to be updated in the save file on the next save*/
	ChunksToSave.Add(ChunkLocation);
	RegionsToSave.Add(GetRegionOfChunk(ChunkLocation));
}

void AVoxelWorld::SaveVoxelWorld() 
{
	/*Overwrite the save data of every chunk that has been marked as needing to be saved with their current data in the game*/

	//iterate over the regions to save 
	for (auto& CurrentRegionToSave : RegionsToSave) 
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
	/*Destroy the block at a given location regardless of whether the chunk is loaded or has even been generated in the first place
	 * In multiplayer, this function should be multicasted.
	 */
	
	//Check if the chunk affected by the edit is loaded
	const auto Temp = (BlockWorldLocation - this->GetActorLocation())/(DefaultVoxelSize*ChunkSize*this->GetActorScale().X);
	const auto AffectedChunkLocation = FIntVector(FMath::Floor(Temp.X), FMath::Floor(Temp.Y), FMath::Floor(Temp.Z));

	if (IsChunkLoaded(AffectedChunkLocation))
	{
		const auto ChunkPtr = GetChunkAt(AffectedChunkLocation);
		if (ChunkPtr)
		{
			ChunkPtr->DestroyBlockAt(BlockWorldLocation); 
		}
		//If the chunk is marked as loaded but no actor is registered for it, it means the chunk is actually empty and there is nothing to remove
		
	}
	else
	{
		const auto RegionDataPtr = GetRegionSavedData(GetRegionOfChunk(AffectedChunkLocation));
		
		if (RegionDataPtr)
		{
			const auto BlockLocationInChunk = FIntVector(BlockWorldLocation/DefaultVoxelSize) - AffectedChunkLocation*ChunkSize;
			if (LoadedRegions[GetRegionOfChunk(AffectedChunkLocation)].Contains(AffectedChunkLocation))
			{
				LoadedRegions[GetRegionOfChunk(AffectedChunkLocation)][AffectedChunkLocation].RemoveVoxel(BlockLocationInChunk);
			}
			else
			{
				//Compute the block's location in the chunk
				auto RelativeLocation =  (BlockWorldLocation - this->GetActorLocation()) - FVector(AffectedChunkLocation)*(DefaultVoxelSize*ChunkSize*this->GetActorScale().X);
				const auto BlockLocation = FIntVector(FMath::Floor(RelativeLocation.X), FMath::Floor(RelativeLocation.Y), FMath::Floor(RelativeLocation.Z));

				//Create additive data to account for the voxel removal
			
				FChunkData AdditiveChunkData = FChunkData::EmptyChunkData();
				AdditiveChunkData.IsAdditive = true;
				AdditiveChunkData.SetVoxel(BlockLocation, FVoxel());

				//Add the data to the region, which is guaranteed to be currently loaded since we called GetRegionSavedData earlier in the function
				RegionDataPtr->Add(AffectedChunkLocation, AdditiveChunkData);
			}
		}
		else
		{
			//Compute the block's location in the chunk
			auto RelativeLocation =  (BlockWorldLocation - this->GetActorLocation()) - FVector(AffectedChunkLocation)*(DefaultVoxelSize*ChunkSize*this->GetActorScale().X);
			const auto BlockLocation = FIntVector(FMath::Floor(RelativeLocation.X), FMath::Floor(RelativeLocation.Y), FMath::Floor(RelativeLocation.Z));

			//Create additive data to account for the voxel removal
			FChunkData AdditiveChunkData = FChunkData::EmptyChunkData();
			AdditiveChunkData.IsAdditive = true;
			AdditiveChunkData.SetVoxel(BlockLocation, FVoxel());

			//Define the new region and add it to loaded regions so that it may be saved on world saving
			auto NewRegion =  TMap<FIntVector, FChunkData>();
			NewRegion.Add(AffectedChunkLocation, AdditiveChunkData);
			LoadedRegions.Add(GetRegionOfChunk(AffectedChunkLocation), NewRegion);
			
		}
	}

	RegisterChunkForSaving(AffectedChunkLocation);
}

void AVoxelWorld::SetBlockAt(FVector BlockWorldLocation, FVoxel Block)
{
	/*Set the value of the block at a given location regardless of wether or not the chunk is loaded or has even been generated in the first place
	 * In multiplayer, this function should be multicasted.
	 */
	
	//Check if the chunk affected by the edit is loaded
	const auto Temp = (BlockWorldLocation - this->GetActorLocation())/(DefaultVoxelSize*ChunkSize*this->GetActorScale().X);
	const auto AffectedChunkLocation = FIntVector(FMath::Floor(Temp.X), FMath::Floor(Temp.Y), FMath::Floor(Temp.Z));

	if (IsChunkLoaded(AffectedChunkLocation))
	{
		const auto ChunkPtr = GetChunkAt(AffectedChunkLocation);
		if (ChunkPtr)
		{
			ChunkPtr->SetBlockAt(BlockWorldLocation, Block); 
		}
		else
		{
			//TODO: handle this case
		}
		
	}
	else
	{
		const auto RegionDataPtr = GetRegionSavedData(GetRegionOfChunk(AffectedChunkLocation));
		
		if (RegionDataPtr)
		{
			const auto BlockLocationInChunk = FIntVector(BlockWorldLocation/DefaultVoxelSize) - AffectedChunkLocation*ChunkSize;
			if (LoadedRegions[GetRegionOfChunk(AffectedChunkLocation)].Contains(AffectedChunkLocation))
			{
				LoadedRegions[GetRegionOfChunk(AffectedChunkLocation)][AffectedChunkLocation].SetVoxel(BlockLocationInChunk, Block);
			}
			else
			{
				//Compute the block's location in the chunk
				auto RelativeLocation =  (BlockWorldLocation - this->GetActorLocation()) - FVector(AffectedChunkLocation)*(DefaultVoxelSize*ChunkSize*this->GetActorScale().X);
				const auto BlockLocation = FIntVector(FMath::Floor(RelativeLocation.X), FMath::Floor(RelativeLocation.Y), FMath::Floor(RelativeLocation.Z));

				//Create additive data to account for the voxel removal
			
				FChunkData AdditiveChunkData = FChunkData::EmptyChunkData();
				AdditiveChunkData.IsAdditive = true;
				AdditiveChunkData.SetVoxel(BlockLocation, Block);

				//Add the data to the region, which is guaranteed to be currently loaded since we called GetRegionSavedData earlier in the function
				RegionDataPtr->Add(AffectedChunkLocation, AdditiveChunkData);
			}
		}
		else
		{
			//Compute the block's location in the chunk
			auto RelativeLocation =  (BlockWorldLocation - this->GetActorLocation()) - FVector(AffectedChunkLocation)*(DefaultVoxelSize*ChunkSize*this->GetActorScale().X);
			const auto BlockLocation = FIntVector(FMath::Floor(RelativeLocation.X), FMath::Floor(RelativeLocation.Y), FMath::Floor(RelativeLocation.Z));

			//Create additive data to account for the voxel removal
			FChunkData AdditiveChunkData = FChunkData::EmptyChunkData();
			AdditiveChunkData.IsAdditive = true;
			AdditiveChunkData.SetVoxel(BlockLocation, Block);

			//Define the new region and add it to loaded regions so that it may be saved on world saving
			auto NewRegion =  TMap<FIntVector, FChunkData>();
			NewRegion.Add(AffectedChunkLocation, AdditiveChunkData);
			LoadedRegions.Add(GetRegionOfChunk(AffectedChunkLocation), NewRegion);
			
		}
	}

	RegisterChunkForSaving(AffectedChunkLocation);
}

void AVoxelWorld::AddManagedPlayer(APlayerController* PlayerToAdd)
{
	const auto CurrentPlayerThread = new FVoxelThread();

	// if(GEngine)
	// {GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Created thread"));}
			
			
	UE_LOG(LogTemp, Warning, TEXT("Created a new voxel thread for player " ))
	FVoxelWorldManagedPlayerData CurrentPlayerData;
	CurrentPlayerData.ManagingThread = CurrentPlayerThread;
	CurrentPlayerData.ChunkThreadedWorkOrdersQueuePtr = CurrentPlayerThread->GetGenerationOrdersQueue();
	CurrentPlayerData.PositionUpdatesQueuePtr = CurrentPlayerThread->GetPlayerUpdatesQueue();

	ManagedPlayerDataMap.Add(PlayerToAdd, CurrentPlayerData);
}

FVoxel AVoxelWorld::DefaultGenerateBlockAt(FVector Position)
{
	//Default world generation function
	
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
	/*Get the region of a given chunk*/
	const int32 RegionX = floor( static_cast<float>(ChunkCoordinates.X) / RegionSize);
	const int32 RegionY = floor( static_cast<float>(ChunkCoordinates.Y) / RegionSize);
	const int32 RegionZ = floor( static_cast<float>(ChunkCoordinates.Z) / RegionSize);

	return FIntVector(RegionX, RegionY, RegionZ);
}

// Called when the game starts or when spawned
void AVoxelWorld::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		for (TPlayerControllerIterator<APlayerController>::ServerAll It(GetWorld()); It; ++It)
		{
			TObjectPtr<APlayerController> CurrentPlayerController = *It;	
			AddManagedPlayer(CurrentPlayerController);
		}
	}
	else
	{
		AddManagedPlayer(GetWorld()->GetFirstPlayerController());
	}

}

void AVoxelWorld::BeginDestroy()
{
	Super::BeginDestroy();

	for (const auto CurrentPair : ManagedPlayerDataMap )
	if (CurrentPair.Value.ManagingThread)
	{
		CurrentPair.Value.ManagingThread->StartShutdown();
	}
}


// Called every frame
void AVoxelWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	IterateChunkLoading();
	
	IterateChunkMeshing();
	
	IterateChunkUnloading();

}

int32 AVoxelWorld::OneNorm(FIntVector Vector) const
{
	return abs(Vector.X) + abs(Vector.Y) + abs(Vector.Z);
}


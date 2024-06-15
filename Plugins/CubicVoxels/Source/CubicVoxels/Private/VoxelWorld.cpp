// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelWorld.h"
#include "Enums.h"
#include "ThreadedWorldGeneration/FVoxelWorldGenerationRunnable.h"
#include "Kismet/GameplayStatics.h"
#include "Chunk.h"
#include "SerializationAndNetworking/VoxelDataStreamer.h"
#include "SerializationAndNetworking/VoxelWorldNetworkSerializationSaveGame.h"
#include "SerializationAndNetworking/VoxelWorldGlobalDataSaveGame.h"
#include "SerializationAndNetworking/RegionDataSaveGame.h"


void AVoxelWorld::TestingFunction(APlayerController* PlayerController)
{
	if (HasAuthority())
	{
		//Test code
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetInstigator();

		FVector SpawnLocation = FVector(0.0f, 0.0f, 0.0f);
		FRotator SpawnRotation = FRotator(0.0f, 0.0f, 0.0f);

		// auto TestStreamer = GetWorld()->SpawnActor<AVoxelDataStreamer>(AVoxelDataStreamer::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
		// TestStreamer->SetOwner(PlayerController);
		// TestStreamer->OwningPlayerController = PlayerController;
		//
		// UE_LOG(LogTemp, Display, TEXT("In VoxelWorld: %hs"), IsValid(TestStreamer->OwningPlayerController) ? "Player controller valid" : "Player controller not valid")
		//
		// const FVoxelStreamData* TestStreamPtr = new FVoxelStreamData( "test" ,TArray<uint8>({2,56,1,8,0,37,2,2,5,7,9,1,2,56,1,8,0,37,2,2,5,7,9,1,2,56,1,8,0,37,2,2,5,7,9,1,2,5,7,9,1,2,56,1,8,0,37,2,2,5,7,9,1,2,56,1,8,0,37}));
		//
		// TestStreamer->AddDataToStream(TestStreamPtr, this);
		
	}
}

// Sets default values
AVoxelWorld::AVoxelWorld()
{
	
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	
	ViewDistance = 12;
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
	NumbersOfPlayerOutsideRangeOfChunkMap = TMap<FIntVector, uint32>();

	WorldGenerationFunction = &DefaultGenerateBlockAt;

	SetActorScale3D(FVector(1,1,1));
	
	WorldName = "MyWorld";

	NetworkMode = EVoxelWorldNetworkMode::ServerSendsFullGeometry;
	
}

void AVoxelWorld::IterateChunkCreationNearPlayers( )
{
	/*Register all chunks in loading distance for loading for each managed player*/
	for (int32 i = 0; i < ViewDistance; i++)
	{
		for (const auto CurrentPlayerThreadPair : ManagedPlayerDataMap)
		{
			if (IsValid(CurrentPlayerThreadPair.Key))
			{
				//Updating the generating thread's value for the position of the player
				FVector PlayerPosition = CurrentPlayerThreadPair.Key->GetPawn()->GetActorLocation();
				const auto LoadingOrigin = FloorVector(this->GetActorRotation().GetInverse().RotateVector((PlayerPosition - this->GetActorLocation())/(ChunkSize*DefaultVoxelSize*this->GetActorScale().X)));

				//Logic for generating the chunks in proximity to the player
				for (auto& Chunk : ViewLayers[i])
				{
					CreateChunkAt(Chunk + LoadingOrigin, CurrentPlayerThreadPair.Value.ChunkGenerationOrdersQueuePtr);
				}
			}
		}
	}
	
}

void AVoxelWorld::IterateGeneratedChunkLoadingAndSidesGeneration()
{

	while(!GeneratedChunksToLoadInGame.IsEmpty())
	{
		TTuple<FIntVector, TSharedPtr<FChunkData>> DataToLoad;
		GeneratedChunksToLoadInGame.Dequeue(DataToLoad);
		GeneratedChunksToLoadByDistanceToNearestPlayer.Add(DataToLoad);
	}
	
	GeneratedChunksToLoadByDistanceToNearestPlayer.Sort([this](const TTuple<FIntVector, TSharedPtr<FChunkData>>& A,const TTuple<FIntVector, TSharedPtr<FChunkData>>& B) {
		return DistanceToNearestPlayer(A.Key) < DistanceToNearestPlayer(B.Key); // sort the pre-cooked chunks by distance to player
	});
	
	//Load the chunks which have been pre-cooked asynchronously, in the order of distance to the player
	for (int32 Index = 0; Index < GeneratedChunksToLoadByDistanceToNearestPlayer.Num(); Index++)
	{
		const TTuple<FIntVector, TSharedPtr<FChunkData>> DataToLoad = GeneratedChunksToLoadByDistanceToNearestPlayer[Index];
		
		//Spawn the chunk actor
		const TObjectPtr<AChunk> RecentlyGeneratedChunk = GetWorld()->SpawnActor<AChunk>();

		RecentlyGeneratedChunk->OwningWorld = this;
		RecentlyGeneratedChunk->VoxelCharacteristicsData = VoxelPhysicalCharacteristicsTable;
		RecentlyGeneratedChunk->Location = DataToLoad.Key;
		RecentlyGeneratedChunk->SetActorLocationAndRotation(this->GetActorRotation().RotateVector( this->GetActorLocation() + this->GetActorScale().X*ChunkSize*DefaultVoxelSize*FVector(DataToLoad.Key) ), this->GetActorRotation() );
		RecentlyGeneratedChunk->SetActorScale3D(this->GetActorScale());
		RecentlyGeneratedChunk->LoadBlocks(DataToLoad.Value);
		RecentlyGeneratedChunk->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		
		ChunkStates.Add(DataToLoad.Key, EChunkState::Loaded);
		ChunkActorsMap.Add(MakeTuple(DataToLoad.Key, RecentlyGeneratedChunk));

		//Replicate the chunk
		//TODO: Write code that serializes the chunk's geometry data and finds all players close enough to this chunk to stream it to them

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
			const auto StatePtr = ChunkStates.Find(DataToLoad.Key + Directions[i]);
			if (StatePtr && *StatePtr == EChunkState::Loaded)
			{
				const auto ActorPtr = ChunkActorsMap.Find(DataToLoad.Key + Directions[i]);
				TSharedPtr<FChunkData> NeighborChunkDataPtr;
				
				if ( ActorPtr && IsValid(*ActorPtr))
				{
					NeighborChunkDataPtr = (*ActorPtr)->BlocksDataPtr;
				}
				else
				{
					NeighborChunkDataPtr = nullptr;
					UE_LOG(LogTemp, Error, TEXT("Some chunk was marked as loaded but had no corresponding actor"))
				}
					
				const auto NearestPlayer = NearestPlayerToChunk(DataToLoad.Key);
				if (const auto NearestPlayerData = ManagedPlayerDataMap.Find(NearestPlayer))
				{
	
					const auto PlayerWorkOrdersQueuePtr = NearestPlayerData->ChunkSidesMeshingOrdersQueuePtr;
							
					auto ChunkSidesGenerationOrder_1 = FChunkThreadedWorkOrderBase();
					ChunkSidesGenerationOrder_1.ChunkLocation = DataToLoad.Key;
					ChunkSidesGenerationOrder_1.DirectionIndex = i;
					ChunkSidesGenerationOrder_1.GeneratedChunkGeometryToLoadQueuePtr = &ChunkQuadsToLoad;
					ChunkSidesGenerationOrder_1.TargetChunkDataPtr = (RecentlyGeneratedChunk->BlocksDataPtr);
					ChunkSidesGenerationOrder_1.NeighboringChunkDataPtr = NeighborChunkDataPtr;
					ChunkSidesGenerationOrder_1.OrderType = EChunkThreadedWorkOrderType::GeneratingExistingChunksSides;
					PlayerWorkOrdersQueuePtr->Enqueue(ChunkSidesGenerationOrder_1);

					auto ChunkSidesGenerationOrder_2 = FChunkThreadedWorkOrderBase();
					ChunkSidesGenerationOrder_2.ChunkLocation = DataToLoad.Key + Directions[i];
					ChunkSidesGenerationOrder_2.DirectionIndex = OppositeDirections[i];
					ChunkSidesGenerationOrder_2.GeneratedChunkGeometryToLoadQueuePtr = &ChunkQuadsToLoad;
					ChunkSidesGenerationOrder_2.TargetChunkDataPtr = NeighborChunkDataPtr;
					ChunkSidesGenerationOrder_2.NeighboringChunkDataPtr = (RecentlyGeneratedChunk->BlocksDataPtr);
					ChunkSidesGenerationOrder_2.OrderType = EChunkThreadedWorkOrderType::GeneratingExistingChunksSides;
					PlayerWorkOrdersQueuePtr->Enqueue(ChunkSidesGenerationOrder_2);
				}
						
					
			}
				
			//	}
		}
	}
	GeneratedChunksToLoadByDistanceToNearestPlayer.Empty();
}

void AVoxelWorld::IterateChunkMeshing()
{
	/*Transmit to the chunk actors their data when it has finished being generated asynchronously */
	
	while(!ChunkQuadsToLoad.IsEmpty())
	{
		
		FChunkGeometry DataToLoad;
		ChunkQuadsToLoad.Dequeue(DataToLoad);
		if (const auto LoadingState = ChunkStates.Find(DataToLoad.ChunkLocation))
		{
			if (*LoadingState == EChunkState::Loaded)
			{
				
				const auto ChunkActor = ChunkActorsMap.Find(DataToLoad.ChunkLocation);
				if (ChunkActor && IsValid(*ChunkActor))
				{
					(*ChunkActor)->AddQuads(DataToLoad.Geometry);
					(*ChunkActor)->RenderChunk(DefaultVoxelSize);
					if (DataToLoad.DirectionIndex >= 0 && DataToLoad.DirectionIndex < 6)
					{
						(*ChunkActor)->IsSideGeometryLoaded[DataToLoad.DirectionIndex] = true;
					}
					else
					{
						(*ChunkActor)->IsInsideGeometryLoaded = true;
					}
					
				}
				else
				{
					ChunkGeometryToBeLoadedLater.Enqueue(DataToLoad);
				}
				//If later chunks are to be forbidden from loading based on culling methods, there ought to be added logic for creating a ChunkActor just to add its sides
			}
			if (*LoadingState == EChunkState::Loading)
			{
				ChunkGeometryToBeLoadedLater.Enqueue(DataToLoad);
			}
		}
	}

	while(!ChunkGeometryToBeLoadedLater.IsEmpty())
	{
		FChunkGeometry DataToLoad;
		ChunkGeometryToBeLoadedLater.Dequeue(DataToLoad);
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


TArray<uint8> AVoxelWorld::GetSerializedWorldData()
{
	auto Serializer = Cast<UVoxelWorldNetworkSerializationSaveGame>(UGameplayStatics::CreateSaveGameObject(UVoxelWorldNetworkSerializationSaveGame::StaticClass()));
	Serializer->WorldName = WorldName;

	auto WorldVoxelData = TMap<FIntVector, FRegionDataSerializationWrapper>();
	auto SavedRegions = WorldSavedInfo->SavedRegions;

	for (const auto& CurrentRegion : SavedRegions)
	{
		if (const auto RegionDataPtr = GetRegionSavedData(CurrentRegion))
		{
			auto WrappedRegionData = FRegionDataSerializationWrapper( *RegionDataPtr);
			WorldVoxelData.Add(CurrentRegion, WrappedRegionData);
		}
	}

	Serializer->WorldVoxelData = WorldVoxelData;
	
	TArray<uint8> SerializedData = TArray<uint8>();
	if (UGameplayStatics::SaveGameToMemory(Serializer, SerializedData))
	{
		return SerializedData;
	}
	else
	{
		return TArray<uint8>();	
	}
	
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

	if (const auto LoadedRegionData = LoadedRegions.Find(ChunkRegionLocation))
	{
		return LoadedRegionData;
	}
	else
	{
		const FString& RegionSaveSlot = GetRegionName(ChunkRegionLocation);
		if (UGameplayStatics::DoesSaveGameExist(RegionSaveSlot, 0))
		{
			if (auto RegionDataSaveObjectPtr = Cast<URegionDataSaveGame>(UGameplayStatics::LoadGameFromSlot(RegionSaveSlot, 0)))
			{
				LoadedRegions.Add(ChunkRegionLocation, (RegionDataSaveObjectPtr->RegionData));
				return &(RegionDataSaveObjectPtr->RegionData);
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

	if (const auto LoadedRegionData = LoadedRegions.Find(GetRegionOfChunk(ChunkLocation)))
	{
		if (const auto CurrentChunkData = LoadedRegionData->Find(ChunkLocation))
		{
			CurrentChunkData->CompressedChunkData = NewData.CompressedChunkData;
		}
	}

	else
	{
		const FString& RegionSaveSlot = GetRegionName(GetRegionOfChunk(ChunkLocation));
		if (UGameplayStatics::DoesSaveGameExist(RegionSaveSlot, 0))
		{
			if (auto RegionDataSaveObject = Cast<URegionDataSaveGame>(UGameplayStatics::LoadGameFromSlot( RegionSaveSlot, 0)))
			{
				LoadedRegions.Add(GetRegionOfChunk(ChunkLocation), (RegionDataSaveObject->RegionData));
				if (const auto CurrentChunkData = LoadedRegionData->Find(ChunkLocation))
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
	WorldSavedInfo->SavedRegions.Add(GetRegionOfChunk(ChunkLocation));
}

void AVoxelWorld::SaveVoxelWorld() 
{
	/*Overwrite the save data of every chunk that has been marked as needing to be saved with their current data in the game*/

	UGameplayStatics::SaveGameToSlot(WorldSavedInfo, WorldName + "\\WorldSaveData", 0 );

	//iterate over the regions to save 
	for (auto& CurrentRegionToSave : RegionsToSave) 
	{
		TMap<FIntVector, FChunkData> CurrentRegionData = TMap<FIntVector, FChunkData>();

		if (auto RegionSaveData = GetRegionSavedData(CurrentRegionToSave))
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
						if (auto ChunkObjectPtr = ChunkActorsMap.Find(CurrentChunk))
						{
							if (IsValid(*ChunkObjectPtr))
							{
								CurrentRegionData.Add(CurrentChunk, *((*ChunkObjectPtr)->BlocksDataPtr) );
							}
						}
					}
					
				}
			}
		}

		//save the region at hand
		auto SaveObject = Cast<URegionDataSaveGame>(UGameplayStatics::CreateSaveGameObject(URegionDataSaveGame::StaticClass()));

		SaveObject->RegionData = CurrentRegionData;

		const FString& RegionSaveSlot = GetRegionName(CurrentRegionToSave);
		UGameplayStatics::SaveGameToSlot(SaveObject,RegionSaveSlot , 0 );
		
	}

	ChunksToSave.Empty();
}

void AVoxelWorld::DestroyBlockAt(FVector BlockWorldLocation)
{
	/*Destroy the block at a given location regardless of whether the chunk is loaded or has even been generated in the first place
	 * In multiplayer, this function should be multicasted.
	 */
	
	//Check if the chunk affected by the edit is loaded
	const auto AffectedChunkLocation = FloorVector((BlockWorldLocation - this->GetActorLocation())/(DefaultVoxelSize*ChunkSize*this->GetActorScale().X) );
	
	if (IsChunkLoaded(AffectedChunkLocation))
	{
		const auto ChunkPtr = GetChunkAt(AffectedChunkLocation);
		if (ChunkPtr)
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Destroying block by calling chunk actor"));	
			ChunkPtr->DestroyBlockAt(BlockWorldLocation); 
		}
		//If the chunk is marked as loaded but no actor is registered for it, it means the chunk is actually empty and there is nothing to remove
		
	}
	else
	{
		const auto RegionDataPtr = GetRegionSavedData(GetRegionOfChunk(AffectedChunkLocation));
		
		//compute the block's location in the chunk
		const auto BlockLocationInChunk = FloorVector((BlockWorldLocation-this->GetActorLocation())/DefaultVoxelSize) - AffectedChunkLocation*ChunkSize;
		UE_LOG(LogTemp, Display, TEXT("Destroying block in chunk: %d,%d, %d"), AffectedChunkLocation.X, AffectedChunkLocation.Y, AffectedChunkLocation.Z );
		
		if (RegionDataPtr)
		{
			
			if (LoadedRegions[GetRegionOfChunk(AffectedChunkLocation)].Contains(AffectedChunkLocation))
			{
				//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Destroying block by editing saved data on an existing chunk"));	
				LoadedRegions[GetRegionOfChunk(AffectedChunkLocation)][AffectedChunkLocation].RemoveVoxel(BlockLocationInChunk);
			}
			else
			{
				
				//Create additive data to account for the voxel removal
				FChunkData AdditiveChunkData = FChunkData::EmptyChunkData(false);
				AdditiveChunkData.SetVoxel(BlockLocationInChunk, FVoxel());
				AdditiveChunkData.IsAdditive = true;
				UE_LOG(LogTemp, Display, TEXT("IsAdditive variable is set to %hs on chunk data"), AdditiveChunkData.IsAdditive ? "True" : "False")
				//Add the data to the region, which is guaranteed to be currently loaded since we called GetRegionSavedData earlier in the function
				RegionDataPtr->Add(AffectedChunkLocation, AdditiveChunkData);

				UE_LOG(LogTemp, Display, TEXT("IsAdditive variable is set to %hs in region data"), (*RegionDataPtr)[AffectedChunkLocation].IsAdditive ? "True" : "False")
			}
		}
		else
		{

			//Create additive data to account for the voxel removal
			FChunkData AdditiveChunkData = FChunkData::EmptyChunkData(false);
			AdditiveChunkData.SetVoxel(BlockLocationInChunk, FVoxel());
			AdditiveChunkData.IsAdditive = true;
			UE_LOG(LogTemp, Display, TEXT("IsAdditive variable is set to %hs on chunk data"), AdditiveChunkData.IsAdditive ? "True" : "False")

			//Define the new region and add it to loaded regions so that it may be saved on world saving
			auto NewRegion =  TMap<FIntVector, FChunkData>();
			NewRegion.Add(AffectedChunkLocation, AdditiveChunkData);
			LoadedRegions.Add(GetRegionOfChunk(AffectedChunkLocation), NewRegion);
			UE_LOG(LogTemp, Display, TEXT("IsAdditive variable is set to %hs in region data"), NewRegion[AffectedChunkLocation].IsAdditive ? "True" : "False")
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
	const auto AffectedChunkLocation = FloorVector((BlockWorldLocation - this->GetActorLocation())/(DefaultVoxelSize*ChunkSize*this->GetActorScale().X));
	
	if (IsChunkLoaded(AffectedChunkLocation))
	{
		if (const auto ChunkPtr = GetChunkAt(AffectedChunkLocation))
		{
			
			ChunkPtr->SetBlockAt(BlockWorldLocation, Block); 
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("A chunk was marked as loaded but the world has no valid pointer to it"))
		}
		
	}
	else
	{
		const auto RegionDataPtr = GetRegionSavedData(GetRegionOfChunk(AffectedChunkLocation));

		const auto BlockLocationInChunk = FloorVector((BlockWorldLocation-this->GetActorLocation())/DefaultVoxelSize) - AffectedChunkLocation*ChunkSize;
		UE_LOG(LogTemp, Display, TEXT("Setting block at: %d,%d, %d"), BlockLocationInChunk.X, BlockLocationInChunk.Y, BlockLocationInChunk.Z );
		
		if (RegionDataPtr)
		{
			
			if (LoadedRegions[GetRegionOfChunk(AffectedChunkLocation)].Contains(AffectedChunkLocation))
			{
				LoadedRegions[GetRegionOfChunk(AffectedChunkLocation)][AffectedChunkLocation].SetVoxel(BlockLocationInChunk, Block);
			}
			else
			{

				//Create additive data to account for the voxel removal
				FChunkData AdditiveChunkData = FChunkData::EmptyChunkData(false);
				AdditiveChunkData.IsAdditive = true;
				AdditiveChunkData.SetVoxel(BlockLocationInChunk, Block);
				
				//Add the data to the region, which is guaranteed to be currently loaded since we called GetRegionSavedData earlier in the function
				RegionDataPtr->Add(AffectedChunkLocation, AdditiveChunkData);
			}
		}
		else
		{
			
			//Create additive data to account for the voxel removal
			FChunkData AdditiveChunkData = FChunkData::EmptyChunkData(false);
			
			AdditiveChunkData.IsAdditive = true;
			AdditiveChunkData.SetVoxel(BlockLocationInChunk, Block);
			
			//Define the new region and add it to loaded regions so that it may be saved on world saving
			auto NewRegion =  TMap<FIntVector, FChunkData>();
			NewRegion.Add(AffectedChunkLocation, AdditiveChunkData);
			LoadedRegions.Add(GetRegionOfChunk(AffectedChunkLocation), NewRegion);
			
		}
	}

	RegisterChunkForSaving(AffectedChunkLocation);
}

FVoxel AVoxelWorld::GetBlockAt(FVector BlockWorldLocation)
{
	const auto AffectedChunkLocation = FloorVector((BlockWorldLocation - this->GetActorLocation())/(DefaultVoxelSize*ChunkSize*this->GetActorScale().X));
	
	if (IsChunkLoaded(AffectedChunkLocation))
	{
		if (const auto ChunkPtr = GetChunkAt(AffectedChunkLocation))
		{
			
			return ChunkPtr->GetBlockAt(BlockWorldLocation); 
		}
		else
		{
			auto Result = FVoxel();
			Result.VoxelType = "Null";
			return Result;
		}
		
	}
	else
	{
		const auto RegionDataPtr = GetRegionSavedData(GetRegionOfChunk(AffectedChunkLocation));

		const auto BlockLocationInChunk = FloorVector((BlockWorldLocation-this->GetActorLocation())/DefaultVoxelSize) - AffectedChunkLocation*ChunkSize;
		
		if (RegionDataPtr)
		{
			
			if (LoadedRegions[GetRegionOfChunk(AffectedChunkLocation)].Contains(AffectedChunkLocation))
			{
				return LoadedRegions[GetRegionOfChunk(AffectedChunkLocation)][AffectedChunkLocation].GetVoxelAt(BlockLocationInChunk);
			}
			else
			{
				auto Result = FVoxel();
				Result.VoxelType = "Null";
				return Result;
			}
		}
		else
		{
			auto Result = FVoxel();
			Result.VoxelType = "Null";
			return Result;
		}
	}

}

void AVoxelWorld::AddManagedPlayer(APlayerController* PlayerToAdd)
{

	if (NetworkMode !=  EVoxelWorldNetworkMode::ClientOnly && HasAuthority())
	{
		FVoxelWorldManagedPlayerData CurrentPlayerData;
		if (WorldGenerationThreads.Num() == 0)
		{
			ServerThreadsSetup(1);
		}
		CurrentPlayerData.PlayerWorldGenerationThread = WorldGenerationThreads[CurrentGenerationThreadIndex];
		CurrentPlayerData.ChunkGenerationOrdersQueuePtr = CurrentPlayerData.PlayerWorldGenerationThread->GetGenerationOrdersQueue();

		//Separating chunk insides and sides generation into different threads might get rid of some stutters
		CurrentPlayerData.PlayerChunkSidesGenerationThread = CurrentPlayerData.PlayerWorldGenerationThread;
		CurrentPlayerData.ChunkSidesMeshingOrdersQueuePtr = CurrentPlayerData.PlayerChunkSidesGenerationThread->GetGenerationOrdersQueue();
		

		/*if (NetworkMode == EVoxelWorldNetworkMode::ServerSendsFullGeometry)
		{
			TObjectPtr<AVoxelDataStreamer> PlayerDataStreamer = CreateDefaultSubobject<AVoxelDataStreamer>("Player dedicated data streamer");
			PlayerDataStreamer->SetOwner(PlayerToAdd);
			PlayerDataStreamer->OwningPlayerController = PlayerToAdd;
			CurrentPlayerData.PlayerDataStreamer = PlayerDataStreamer;
		}*/

		ManagedPlayerDataMap.Add(PlayerToAdd, CurrentPlayerData);
		CurrentGenerationThreadIndex = CurrentGenerationThreadIndex + 1 % NumberOfWorldGenerationThreads;
		UE_LOG(LogTemp, Display, TEXT("Finished adding managed player"));
	}
	else
	{
		
		const auto CurrentPlayerWorldGenerationRunnable = new FVoxelWorldGenerationRunnable;
	
		FVoxelWorldManagedPlayerData CurrentPlayerData;
		CurrentPlayerData.PlayerWorldGenerationThread = CurrentPlayerWorldGenerationRunnable;
		CurrentPlayerData.ChunkGenerationOrdersQueuePtr = CurrentPlayerData.PlayerWorldGenerationThread->GetGenerationOrdersQueue();

		//Separating chunk insides and sides generation into different threads might get rid of some stutters
		const auto CurrentPlayerChunkSidesGenerationRunnable = new FVoxelWorldGenerationRunnable;
		CurrentPlayerData.PlayerChunkSidesGenerationThread = CurrentPlayerChunkSidesGenerationRunnable;
		CurrentPlayerData.ChunkSidesMeshingOrdersQueuePtr = CurrentPlayerData.PlayerChunkSidesGenerationThread->GetGenerationOrdersQueue();

		ManagedPlayerDataMap.Add(PlayerToAdd, CurrentPlayerData);
	}

	int32 PlayerIndex = 0;
	while (PlayerIDs.FindKey(PlayerIndex))
	{
		PlayerIndex = PlayerIndex + 1 % 10000000;
	}
	PlayerIDs.Add(PlayerToAdd, PlayerIndex);
	
}

void AVoxelWorld::InterpretVoxelStream(FName StreamType, const TArray<uint8>& VoxelStream)
{
	if (StreamType == "WorldSave")
	{
		OverwriteSaveWithSerializedData(VoxelStream);
	}

	if (StreamType == "test")
	{
		for (int32 i = 0; i < VoxelStream.Num(); i++)
		{
			UE_LOG(LogTemp, Display, TEXT("END/ The bit at %d is at %d"), i, VoxelStream[i])
		}
	}

	if (StreamType == "Chunk replicated data")
	{
		//TODO: write code that deserializes this as a FChunkReplicatedData and feeds the data into the client chunk
	}
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
		if (Position.Z < -2500)
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




void AVoxelWorld::CreateChunkAt(FIntVector ChunkLocation,
                                TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* OrdersQueuePtr)
{
	if (!ChunkStates.Contains( ChunkLocation ))
	{
				
		ChunkStates.Add(ChunkLocation, EChunkState::Loading);
		if (const auto RegionSavedData = GetRegionSavedData(GetRegionOfChunk(ChunkLocation) ))
		{
			if (const auto ChunkSavedData = RegionSavedData->Find(ChunkLocation))
			{
				const TSharedPtr<FChunkData> ChunkVoxelDataPtr(ChunkSavedData);
				auto ChunkGenerationOrder = FChunkThreadedWorkOrderBase();
							
				ChunkGenerationOrder.TargetChunkDataPtr = ChunkVoxelDataPtr;
				ChunkGenerationOrder.OutputChunkDataQueuePtr = &GeneratedChunksToLoadInGame;
				ChunkGenerationOrder.GeneratedChunkGeometryToLoadQueuePtr = &ChunkQuadsToLoad;
				ChunkGenerationOrder.ChunkLocation = ChunkLocation;
								
				if (ChunkVoxelDataPtr->IsAdditive)
				{
					ChunkGenerationOrder.OrderType = EChunkThreadedWorkOrderType::GeneratingAndMeshingWithAdditiveData;
					ChunkGenerationOrder.GenerationFunction = WorldGenerationFunction;
				}
				else
				{
					ChunkGenerationOrder.OrderType = EChunkThreadedWorkOrderType::MeshingFromData;
				}
							
				OrdersQueuePtr->Enqueue(ChunkGenerationOrder);

			}
			else
			{
				auto ChunkGenerationOrder = FChunkThreadedWorkOrderBase();
		
				ChunkGenerationOrder.GenerationFunction = WorldGenerationFunction;
				ChunkGenerationOrder.OutputChunkDataQueuePtr = &GeneratedChunksToLoadInGame;
				ChunkGenerationOrder.GeneratedChunkGeometryToLoadQueuePtr = &ChunkQuadsToLoad;
				ChunkGenerationOrder.ChunkLocation = ChunkLocation;
				ChunkGenerationOrder.OrderType = EChunkThreadedWorkOrderType::GenerationAndMeshing;
				OrdersQueuePtr->Enqueue(ChunkGenerationOrder);
			}
		}
		else
		{
			auto ChunkGenerationOrder = FChunkThreadedWorkOrderBase();
					
			ChunkGenerationOrder.GenerationFunction = WorldGenerationFunction;
			ChunkGenerationOrder.OutputChunkDataQueuePtr = &GeneratedChunksToLoadInGame;
			ChunkGenerationOrder.GeneratedChunkGeometryToLoadQueuePtr = &ChunkQuadsToLoad;
			ChunkGenerationOrder.ChunkLocation = ChunkLocation;
			ChunkGenerationOrder.OrderType = EChunkThreadedWorkOrderType::GenerationAndMeshing;
			OrdersQueuePtr->Enqueue(ChunkGenerationOrder);
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

void AVoxelWorld::UpdatePlayerPositionsOnThreads()
{
	
	if (!ManagedPlayerDataMap.IsEmpty())
	{
		PlayerPositionsUpdateOnThreadsMutex.Lock();
		
		for (const auto CurrentPlayerThreadPair : ManagedPlayerDataMap)
		{
			//TODO: Find out why player controller is sometimes not valid
			if (IsValid(CurrentPlayerThreadPair.Key))
			{
				if (const auto PlayerIDPtr = PlayerIDs.Find(CurrentPlayerThreadPair.Key))
				{
					FVector PlayerPosition = CurrentPlayerThreadPair.Key->GetPawn()->GetActorLocation();
					const auto LoadingOrigin = FloorVector(this->GetActorRotation().GetInverse().RotateVector((PlayerPosition - this->GetActorLocation())/(ChunkSize*DefaultVoxelSize*this->GetActorScale().X)));
					CurrentPlayerThreadPair.Value.PlayerWorldGenerationThread->ManagedPlayersPositionsMap.Add( *PlayerIDPtr, LoadingOrigin);
				}
				
			}
			
		}
		
		PlayerPositionsUpdateOnThreadsMutex.Unlock();

	}
	

}

int32 AVoxelWorld::DistanceToNearestPlayer(FIntVector ChunkLocation)
{
	auto Result = -1;
	
	for (auto& PlayerDataPair : ManagedPlayerDataMap)
	{
		if (IsValid(PlayerDataPair.Key))
		{
			const auto LoadingOrigin =	FloorVector(((PlayerDataPair.Key->GetPawn()->GetActorLocation() - this->GetActorLocation())/(ChunkSize*DefaultVoxelSize*this->GetActorScale().X)));
			if (Result == -1 || OneNorm(LoadingOrigin - ChunkLocation) < Result)
			{
				Result = OneNorm(LoadingOrigin - ChunkLocation);
			}
		}
	}

	return Result;
}

TObjectPtr<APlayerController> AVoxelWorld::NearestPlayerToChunk(FIntVector ChunkLocation)
{
	auto MinimumDistance = -1;
	TObjectPtr<APlayerController> Result = nullptr;
	
	for (auto& PlayerDataPair : ManagedPlayerDataMap)
	{
		if (IsValid(PlayerDataPair.Key))
		{
			const auto LoadingOrigin =	FloorVector(((PlayerDataPair.Key->GetPawn()->GetActorLocation() - this->GetActorLocation())/(ChunkSize*DefaultVoxelSize*this->GetActorScale().X)));
			if (MinimumDistance == -1 || OneNorm(LoadingOrigin - ChunkLocation) < MinimumDistance)
			{
				MinimumDistance = OneNorm(LoadingOrigin - ChunkLocation);
				Result = PlayerDataPair.Key;
			}
		}
	}

	return Result;
}

FString AVoxelWorld::GetRegionName(FIntVector RegionLocation)
{
	FString RegionCoordinates = WorldName + "\\";
	RegionCoordinates.AppendInt(RegionLocation.X);
	RegionCoordinates.Append(",");
	RegionCoordinates.AppendInt(RegionLocation.Y);
	RegionCoordinates.Append(",");
	RegionCoordinates.AppendInt(RegionLocation.Z);

	return RegionCoordinates;
}

void AVoxelWorld::ServerThreadsSetup(int32 NumberOfThreads)
{
	NumberOfWorldGenerationThreads = NumberOfThreads;
	WorldGenerationThreads.SetNum(NumberOfWorldGenerationThreads);
	for (int32 i = 0; i < NumberOfWorldGenerationThreads; i++)
	{
		WorldGenerationThreads[i] = new FVoxelWorldGenerationRunnable;
	}
	CurrentGenerationThreadIndex = 0;

	ManagedPlayerDataMap = TMap<TObjectPtr<APlayerController>, FVoxelWorldManagedPlayerData>();
}

void AVoxelWorld::DownloadWorldSave_Implementation()
{
	if (APlayerController* SendingPlayerController = Cast<APlayerController>(GetOwner()))
	{
		auto StreamManager = CreateDefaultSubobject<AVoxelDataStreamer>("Streamer"); 

		StreamManager->SetOwner(SendingPlayerController);
		TFunction<void(TArray<uint8>)> SaveFileOverwriteFunction = [this](const TArray<uint8>& Data){ return OverwriteSaveWithSerializedData(Data); };
		
		auto WorldDownloadStream = FVoxelStreamData( FName("WorldSaveStream"), GetSerializedWorldData());
		StreamManager->AddDataToStream(&WorldDownloadStream, this);
	}
}

void AVoxelWorld::OverwriteSaveWithSerializedData(TArray<uint8> Data)
{
	if (UVoxelWorldNetworkSerializationSaveGame* SerializationSaveObject = Cast<UVoxelWorldNetworkSerializationSaveGame>(UGameplayStatics::LoadGameFromMemory(Data)))
	{
		WorldName = SerializationSaveObject->WorldName;

		TSet<FIntVector> SavedRegions;
		
		for (const auto& Region : SerializationSaveObject->WorldVoxelData)
		{
			SavedRegions.Add(Region.Key);

			auto SaveObject = Cast<URegionDataSaveGame>(UGameplayStatics::CreateSaveGameObject(URegionDataSaveGame::StaticClass()));

			SaveObject->RegionData = Region.Value.RegionData;

			const FString& RegionSaveSlot = GetRegionName(Region.Key);
			UGameplayStatics::SaveGameToSlot(SaveObject, RegionSaveSlot, 0 );
			
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Could not deserialize saved data"))
	}

	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Overwrote client save file with server save file"));	
}

// Called when the game starts or when spawned
void AVoxelWorld::BeginPlay()
{
	Super::BeginPlay();
	
	//Creates the main world save file
	 if (UGameplayStatics::DoesSaveGameExist(WorldName + "\\WorldSaveData", 0))
	 {
		 if (auto GlobalDatasaveGameObject = Cast<UVoxelWorldGlobalDataSaveGame>(UGameplayStatics::LoadGameFromSlot(WorldName + "\\WorldSaveData", 0)))
	 	{
	 		WorldSavedInfo = GlobalDatasaveGameObject;
	 	}
	 	else
	 	{
	 		UE_LOG(LogTemp, Error, TEXT("A save file was found but it was not valid"))
	 	}
	 }
	 else
	 {
		WorldSavedInfo = Cast<UVoxelWorldGlobalDataSaveGame>(UGameplayStatics::CreateSaveGameObject(UVoxelWorldGlobalDataSaveGame::StaticClass()));
	}

	//If the game is single-player, the player around which the world is generated is automatically registered
	//On a server, players must be added as they join
	if (NetworkMode == EVoxelWorldNetworkMode::ClientOnly)
	{
		AddManagedPlayer(GetWorld()->GetFirstPlayerController());
		
	}
}

void AVoxelWorld::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	TSet<FVoxelWorldGenerationRunnable*> VoxelWorldGenerationRunnables;

	for (const auto CurrentPair : ManagedPlayerDataMap )
	{
		VoxelWorldGenerationRunnables.Add(CurrentPair.Value.PlayerWorldGenerationThread);
		VoxelWorldGenerationRunnables.Add(CurrentPair.Value.PlayerChunkSidesGenerationThread);
	}
	
	for (auto CurrentThread : VoxelWorldGenerationRunnables )
	{
		
		ThreadShutdownMutex.Lock();
		CurrentThread->StartShutdown();
		bool IsThreadValid = CurrentThread->Thread != nullptr;
		ThreadShutdownMutex.Unlock();
		
		if (IsThreadValid)
		{
			CurrentThread->Thread->WaitForCompletion();
		}
		
	}
}


// Called every frame
void AVoxelWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsEnabled && (NetworkMode == EVoxelWorldNetworkMode::ServerSendsVoxelDiffs ||  NetworkMode == EVoxelWorldNetworkMode::ClientOnly || (NetworkMode == EVoxelWorldNetworkMode::ServerSendsFullGeometry && HasAuthority())))
	{
		UpdatePlayerPositionsOnThreads();
	
		IterateChunkCreationNearPlayers();

		IterateGeneratedChunkLoadingAndSidesGeneration();
	
		IterateChunkMeshing();
	
		IterateChunkUnloading();
	}
	
}

int32 AVoxelWorld::OneNorm(FIntVector Vector)
{
	return abs(Vector.X) + abs(Vector.Y) + abs(Vector.Z);
}

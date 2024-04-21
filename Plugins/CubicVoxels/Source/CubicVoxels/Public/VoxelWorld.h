// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelStructs.h"
#include "ReplicationStructs.h"
#include "VoxelChunkThreadingUtilities.h"
#include "FVoxelThread.h"
#include "VoxelWorld.generated.h"

class AChunk;
class UProceduralMeshComponent; 

enum class EChunkState;

UCLASS()
class AVoxelWorld : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AVoxelWorld();

	//Chunk loading distance parameters
	int32 ViewDistance;
	int32 VerticalViewDistance;

	//Table that links voxel types with their materials
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UDataTable* VoxelPhysicalCharacteristicsTable;

	//Pointer to the function that generates the terrain procedurally
	FVoxel (*WorldGenerationFunction) (FVector);

	//The VoxelWorld may manage multiple players
	//It will generate the world around each managed player
	//On the client there will generally be only one managed player, on the server every player is generally managed
	//Each managed player has its own assigned thread to create the world around him
	//All the data needed to manage a player is contained in a struct linked to the player by the following map
	TMap<TObjectPtr<APlayerController>, FVoxelWorldManagedPlayerData> ManagedPlayerDataMap;
	
	//Functions to access chunk data
	bool IsChunkLoaded(FIntVector ChunkLocation);
	TMap<FIntVector, FChunkData>* GetRegionSavedData(FIntVector RegionLocation);
	TObjectPtr<AChunk> GetChunkAt(FIntVector ChunkLocation);
	void SetChunkSavedData(FIntVector ChunkLocation, FChunkData NewData);

	//Blueprint functions to edit and save the world
	UFUNCTION(BlueprintCallable)
	void SaveVoxelWorld();

	UFUNCTION(BlueprintCallable)
	void DestroyBlockAt(FVector BlockWorldLocation); 

	UFUNCTION(BlueprintCallable)
	void SetBlockAt(FVector BlockWorldLocation, FVoxel Block);

	//Function to add a player to be managed by the VoxelWorld
	UFUNCTION(BlueprintCallable)
	void AddManagedPlayer(APlayerController* PlayerToAdd);
	
private:

	//Main functions called on actor ticking
	void UpdatePlayerPositionsOnThreads();
	void IterateChunkCreationNearPlayers();
	void IterateChunkSidesGenerationNearPlayers();
	void IterateChunkMeshing();
	void IterateChunkUnloading();

	TMap<FIntVector, EChunkState> ChunkStates;
	TMap<FIntVector, TObjectPtr<AChunk>> ChunkActorsMap;
	TSet<FIntVector> ChunksToSave;
	TSet<FIntVector> RegionsToSave;

	void CreateChunkAt(FIntVector ChunkLocation, TQueue<FChunkThreadedWorkOrderBase, EQueueMode::Mpsc>* OrdersQueuePtr);

	//Map of all the regions that are currently loaded in memory, in each region the chunks are located in absolute chunk coordinates
	TMap<FIntVector, TMap<FIntVector, FChunkData>> LoadedRegions; 

	TQueue< TTuple<FIntVector, TSharedPtr<FChunkData>>, EQueueMode::Mpsc> GeneratedChunksToLoadInGame;
	TQueue< TTuple<FIntVector, TMap<FIntVector4, FVoxel>>, EQueueMode::Mpsc> ChunkQuadsToLoad;

	FIntVector GetRegionOfChunk(FIntVector ChunkCoordinates);

	void RegisterChunkForSaving(FIntVector3 ChunkLocation);

	FCriticalSection PlayerPositionsUpdateOnThreadsMutex;



protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	
	TArray<TSet<FIntVector>> ViewLayers;
	TArray<TTuple<FIntVector, TSharedPtr<FChunkData>>> OrderedGeneratedChunksToLoadInGame;
	TMap<FIntVector, uint32> NumbersOfPlayerOutsideRangeOfChunkMap;
	TQueue<TTuple<FIntVector, TMap<FIntVector4, FVoxel>>> ChunkQuadsToBeLoadedLater;
	
	int32 OneNorm(FIntVector Vector) const;
	
	static FVoxel DefaultGenerateBlockAt(FVector Position);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override; 

};


// class FVoxelChunkSideCookingAsyncTask : public FNonAbandonableTask //Asynchronous task that generates a chunk's face's quad data
// {
// 	FChunkData* TaskChunkToContinueLoadingBlocks;
// 	FChunkData* TaskNeighbourChunkBlocks;
// 	int32 TaskDirectionIndex;
// 	TQueue< TTuple<FIntVector, TMap<FIntVector4, FVoxel>>, EQueueMode::Mpsc>* TaskChunkQuadsToLoad;
// 	FIntVector TaskChunkToContinueLoadingCoordinates;
//  
// public:
// 	//Default constructor
// 	FVoxelChunkSideCookingAsyncTask(FChunkData* ChunkToContinueLoadingBlocks, FChunkData* TaskNeighbourChunkBlocks, int32 DirectionIndex, TQueue< TTuple<FIntVector, TMap<FIntVector4, FVoxel>>, EQueueMode::Mpsc>* ChunkQuadsToLoad, FIntVector ChunkToContinueLoadingCoordinates)
// 	{
// 		this->TaskChunkToContinueLoadingBlocks = ChunkToContinueLoadingBlocks;
// 		this->TaskNeighbourChunkBlocks = TaskNeighbourChunkBlocks;
// 		this->TaskDirectionIndex = DirectionIndex;
// 		this->TaskChunkQuadsToLoad = ChunkQuadsToLoad;
// 		this->TaskChunkToContinueLoadingCoordinates = ChunkToContinueLoadingCoordinates;
// 	}
//  
// 	//This function is needed from the API of the engine. 
// 	FORCEINLINE TStatId GetStatId() const
// 	{
// 		RETURN_QUICK_DECLARE_CYCLE_STAT(PrimeCalculationAsyncTask, STATGROUP_ThreadPoolAsyncTasks);
// 	}
//  
// 	//This function is executed when we tell our task to execute
// 	void DoWork()
// 	{
// 		ComputeChunkSideFacesFromData( TaskChunkToContinueLoadingBlocks, TaskNeighbourChunkBlocks, TaskDirectionIndex, TaskChunkQuadsToLoad,  TaskChunkToContinueLoadingCoordinates);
// 	}
// };

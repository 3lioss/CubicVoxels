// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelStructs.h"
#include "ReplicationStructs.h"
#include "ThreadedWorldGeneration/FVoxelWorldGenerationRunnable.h"
#include "Serialization/VoxelWorldGlobalDataSaveGame.h"
#include "VoxelDataStreamer.h"
#include "VoxelWorld.generated.h"

class AChunk;
class UProceduralMeshComponent; 

enum class EChunkState;

UCLASS()
class AVoxelWorld : public AActor
{
	GENERATED_BODY()

	friend AVoxelDataStreamer;
public:
	//TODO: remove this
	UFUNCTION(BlueprintCallable)
	void TestWorldSerialization();
	
	// Sets default values for this actor's properties
	AVoxelWorld();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool IsEnabled; //Should be false by default in multiplayer

	//Chunk loading distance parameters //TODO: Make it mutable at runtime
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

	//Name of the folder where the world saved data is saved
	UPROPERTY(EditAnywhere)
	FString WorldName;

	TArray<uint8> GetSerializedWorldData();
	
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

	UFUNCTION(BlueprintCallable)
	FVoxel GetBlockAt(FVector BlockWorldLocation);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void DownloadWorldSave();

	//Function to add a player to be managed by the VoxelWorld
	UFUNCTION(BlueprintCallable)
	void AddManagedPlayer(APlayerController* PlayerToAdd, bool IsOnServer = true);
	
	
private:
	//Only used on the server to set a fixed number of threads to handle players
	int32 NumberOfWorldGenerationThreads;
	int32 CurrentGenerationThreadIndex;
	TArray<FVoxelWorldGenerationRunnable*> WorldGenerationThreads;

	//Main functions called on actor ticking
	void UpdatePlayerPositionsOnThreads();
	void IterateChunkCreationNearPlayers();
	void IterateGeneratedChunkLoadingAndSidesGeneration();
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
	TQueue< FChunkGeometry, EQueueMode::Mpsc> ChunkQuadsToLoad;

	FIntVector GetRegionOfChunk(FIntVector ChunkCoordinates);

	void RegisterChunkForSaving(FIntVector3 ChunkLocation);

	FCriticalSection PlayerPositionsUpdateOnThreadsMutex;

	int32 DistanceToNearestPlayer(FIntVector ChunkLocation);
	TObjectPtr<APlayerController> NearestPlayerToChunk(FIntVector ChunkLocation);

	FString GetRegionName(FIntVector RegionLocation);

	//Functions and variables used for world download from server
	//TMap<TObjectPtr<APlayerController>, FVoxelStreamManager> PlayerWorldDownloadingStreamMap;

	UFUNCTION(Client, Reliable)
	void SendVoxelStreamChunk(FVoxelStreamChunk Data);

	TArray<uint8> SerializedDataAccumulator;

	void OverwriteSaveWithSerializedData(TArray<uint8> Data);

	int32 MaxBytesPerStreamChunk;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	
	TArray<TSet<FIntVector>> ViewLayers;
	TArray<TTuple<FIntVector, TSharedPtr<FChunkData>>> GeneratedChunksToLoadByDistanceToNearestPlayer;
	TMap<FIntVector, uint32> NumbersOfPlayerOutsideRangeOfChunkMap;
	TQueue<FChunkGeometry> ChunkGeometryToBeLoadedLater;
	
	int32 OneNorm(FIntVector Vector) const;
	
	static FVoxel DefaultGenerateBlockAt(FVector Position);


	//SaveGame that stores all the global data of the VoxelWorld actor
	//That is the data which is not owned by a particular region
	UPROPERTY()
	UVoxelWorldGlobalDataSaveGame* WorldSavedInfo;
	

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override; 

};



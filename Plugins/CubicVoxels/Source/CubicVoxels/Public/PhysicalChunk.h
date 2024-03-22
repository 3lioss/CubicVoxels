// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelWorld.h"
#include "PhysicalChunk.generated.h"

class UProceduralMeshComponent; 
struct FMeshData;

UCLASS()
class CUBICVOXELS_API APhysicalChunk : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APhysicalChunk();

	void LoadBlocks(TSharedPtr<FChunkData> InputVoxelData);
	void AddQuads(TMap<FIntVector4, FVoxel> VoxelQuadsToAdd);
	bool HasQuadAt(FIntVector4 QuadLocation);
	void RemoveQuad(FIntVector4 Quad);
	
	void RenderChunk(float VoxelSize); //Function that sets up the procedural mesh component with the data contained in ChunkQuads

	UFUNCTION(BlueprintCallable)
	void DestroyBlockAt(FVector BlockWorldLocation);

	UFUNCTION(BlueprintCallable)
	void SetBlockAt(FVector BlockWorldLocation, FVoxel BlockType);

	TObjectPtr<AVoxelWorld> OwningWorld;
	FIntVector Location;
	bool IsLoaded;

	class UDataTable* VoxelCharacteristicsData;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	TMap<FIntVector4, FVoxel> VoxelQuads; //A map that associates to a coordinate and a direction the voxel at that location if it should have a face in the given direction

	static bool IsInsideChunk(FIntVector BlockLocation);
	
	void RemoveBlockFromBlockData(FIntVector3 BlockLocation);
	void SetBlockInBlockData(FIntVector3 BlockLocation, FVoxel BlockVoxel);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	TSharedPtr<FChunkData> BlocksData;
	TObjectPtr<UProceduralMeshComponent> Mesh;
	TObjectPtr<UStaticMeshComponent> LoadingCube;

public:
	void SaveChunkDataToDisk(); //Temporary, in final system we should rather save regions of chunks
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelWorld.h"
#include "Chunk.generated.h"

class UProceduralMeshComponent; 
struct FMeshData;

UCLASS()
class CUBICVOXELS_API AChunk : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AChunk();

	//Functions to set up the chunk
	void LoadBlocks(TSharedPtr<FChunkData> InputVoxelData);
	void AddQuads(TMap<FIntVector4, FVoxel> VoxelQuadsToAdd);
	bool HasQuadAt(FIntVector4 QuadLocation);
	void RemoveQuad(FIntVector4 Quad);
	void RenderChunk(float VoxelSize);

	//Functions to modify the chunk
	void DestroyBlockAt(FVector BlockWorldLocation);
	void SetBlockAt(FVector BlockWorldLocation, FVoxel BlockType);
	FVoxel GetBlockAt(FVector BlockWorldLocation);

	//Values set by the VoxelWorld
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AVoxelWorld> OwningWorld;
	
	FIntVector Location;
	bool IsLoaded;

	UPROPERTY()
	class UDataTable* VoxelCharacteristicsData;

	UFUNCTION(BlueprintCallable)
	void ShowFaceGenerationStatus();

protected:
	UStaticMeshComponent* Cube;
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	//A map that associates to a coordinate and a direction the voxel at that location if it should have a face in the given direction
	TMap<FIntVector4, FVoxel> VoxelQuads;

	static bool IsInsideChunk(FIntVector BlockLocation);

	

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	bool IsInsideGeometryLoaded;
	bool IsSideGeometryLoaded[6];
	
	TSharedPtr<FChunkData> BlocksDataPtr;
	
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Mesh;
	
};

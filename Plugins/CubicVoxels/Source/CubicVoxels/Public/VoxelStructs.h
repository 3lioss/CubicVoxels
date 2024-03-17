#pragma once
#include "ProceduralMeshComponent.h" 
#include "Engine/DataTable.h"
#include "VoxelStructs.generated.h"


USTRUCT()
struct FMeshData
{
	GENERATED_USTRUCT_BODY()

	TArray<FVector> VertexData = TArray<FVector>();
	TArray<FVector> NormalsData = TArray<FVector>();
	TArray<int32> TriangleData = TArray<int32>();
	TArray<FVector2d> UVData = TArray<FVector2d>();
	TArray<FColor> VertexColors = TArray<FColor>();
	TArray<FProcMeshTangent> Tangents = TArray<FProcMeshTangent>();
};

USTRUCT(BlueprintType)
struct FVoxelCharacteristics : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* VoxelMaterial;
	
};

USTRUCT()
struct FVoxel
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(SaveGame)
	FName VoxelType = "Air";

	UPROPERTY(SaveGame)
	bool IsTransparent = true;
	
	UPROPERTY(SaveGame)
	bool IsSolid = false;
	
};

inline FVoxel DefaultVoxel; //global variable used to denote an air voxel

USTRUCT()
struct FVoxelStack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(SaveGame)
	FVoxel Voxel;

	UPROPERTY(SaveGame)
	int32 StackSize;
};

inline FVoxelStack MakeStack(FVoxel Voxel, int32 StackSize)
{
	FVoxelStack Result = FVoxelStack();
	Result.Voxel = Voxel;
	Result.StackSize = StackSize;
	
	return Result;
}

USTRUCT()
struct FChunkData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(SaveGame)
	TArray<FVoxelStack> ChunkData;
	
};


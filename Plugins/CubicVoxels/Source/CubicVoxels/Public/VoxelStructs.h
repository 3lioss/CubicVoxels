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

USTRUCT(BlueprintType)
struct FVoxel
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite)
	FName VoxelType = "Air";

	UPROPERTY(SaveGame,BlueprintReadWrite)
	bool IsTransparent = true;
	
	UPROPERTY(SaveGame, BlueprintReadWrite)
	bool IsSolid = false;

	
};

inline bool operator== (const FVoxel& V1, const FVoxel& V2)
{
	return (V1.VoxelType == V2.VoxelType);
}
	

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

	void RemoveVoxel(FIntVector3 VoxelLocation,int32 ChunkSize){
	
		auto BlockIndex = VoxelLocation.X*ChunkSize*ChunkSize + VoxelLocation.Y*ChunkSize + VoxelLocation.Z;

		int32 i = 0;
	
		while (ChunkData[i].StackSize - 1 < BlockIndex)
		{
			BlockIndex -= ChunkData[i].StackSize;
			i += 1;
		}
	
	
		if (ChunkData[i].Voxel == DefaultVoxel)
		{
			if (BlockIndex == 0)
			{
				ChunkData[i].StackSize -= 1;
				ChunkData.Insert(MakeStack(DefaultVoxel, 1), i);
			}
			else
			{
				if (BlockIndex == ChunkData[i].StackSize - 1)
				{
					if (i+1 < ChunkData.Num())
					{
						if (ChunkData[i+1].Voxel == DefaultVoxel)
						{
							ChunkData[i].StackSize -= 1;
							ChunkData[i+1].StackSize += 1;
						}
						else
						{
							ChunkData[i].StackSize -= 1;
							ChunkData.Insert(MakeStack(DefaultVoxel, 1), i+1);
						}
					}
					else
					{
						ChunkData[i].StackSize -= 1;
						ChunkData.Add(MakeStack(DefaultVoxel, 1));
					}
				}
				else
				{
					const auto Temp = ChunkData[i].StackSize;
					ChunkData[i].StackSize = BlockIndex;
					if (i+1 < ChunkData.Num())
					{
						ChunkData.Insert(MakeStack(ChunkData[i].Voxel, Temp - BlockIndex - 1), i+1);
						ChunkData.Insert(MakeStack(DefaultVoxel, 1), i+1);
					}
					else
					{
						ChunkData.Add(MakeStack(DefaultVoxel, 1));
						ChunkData.Add(MakeStack(ChunkData[i].Voxel, Temp - BlockIndex - 1));
					}
				}
			
			}
		}
		}

	void SetVoxel(FIntVector3 VoxelLocation, FVoxel Voxel, int32 ChunkSize)
	{
		auto BlockIndex = VoxelLocation.X*ChunkSize*ChunkSize + VoxelLocation.Y*ChunkSize + VoxelLocation.Z;

		int32 i = 0;
	
		while (ChunkData[i].StackSize - 1 < BlockIndex)
		{
			BlockIndex -= (ChunkData)[i].StackSize;
			i += 1;
		}
	
	
		if ((ChunkData)[i].Voxel != Voxel )
		{
			if (BlockIndex == 0)
			{
				(ChunkData)[i].StackSize -= 1;
				ChunkData.Insert(MakeStack(Voxel, 1), i);
			}
			else
			{
				if (BlockIndex == (ChunkData)[i].StackSize - 1)
				{
					if (i+1 < ChunkData.Num())
					{
						if ((ChunkData)[i+1].Voxel == Voxel )
						{
							(ChunkData)[i].StackSize -= 1;
							(ChunkData)[i+1].StackSize += 1;
						}
						else
						{
							(ChunkData)[i].StackSize -= 1;
							ChunkData.Insert(MakeStack(Voxel, 1), i+1);
						}
					}
					else
					{
						(ChunkData)[i].StackSize -= 1;
						ChunkData.Add(MakeStack(Voxel, 1));
					}
				}
				else
				{
					const auto Temp = (ChunkData)[i].StackSize;
					(ChunkData)[i].StackSize = BlockIndex;
					if (i+1 < ChunkData.Num())
					{
						ChunkData.Insert(MakeStack((ChunkData)[i].Voxel, Temp - BlockIndex - 1), i+1);
						ChunkData.Insert(MakeStack(Voxel, 1), i+1);
					}
					else
					{
						ChunkData.Add(MakeStack(Voxel, 1));
						ChunkData.Add(MakeStack((ChunkData)[i].Voxel, Temp - BlockIndex - 1));
					}
				}
			}
		}
	}
};


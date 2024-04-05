#pragma once
#include "BasePluginValues.h"
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

	FVoxelStack()
	{
		StackSize = 0;
		Voxel = FVoxel();
	}
};

inline FVoxelStack MakeStack(FVoxel Voxel, int32 StackSize)
{
	FVoxelStack Result;
	Result.Voxel = Voxel;
	Result.StackSize = StackSize;
	
	return Result;
}

USTRUCT()
struct FChunkData
{
	GENERATED_USTRUCT_BODY()

	FThreadSafeBool IsCompressed = false; //TODO: use this boolean as a necessary condition for saving on disk

	TArray<FVoxel> UncompressedChunkData;

	UPROPERTY(SaveGame)
	TArray<FVoxelStack> CompressedChunkData;

	FChunkData()
	{
		UncompressedChunkData.SetNum(ChunkSize*ChunkSize*ChunkSize);
	}

	FVoxel GetVoxelAt(int32 x, int32 y, int32 z)
	{
		return GetVoxelAt(FIntVector(x,y,z));
	}

	FVoxel GetVoxelAt(FIntVector IntLocation){ //Function to manipulate compressed chunk data //TODO: move this code into a method of FChunkData and replace all usages of it with the method

		if (IsCompressed)
		{
			const int32 BlockIndex = IntLocation.X*ChunkSize*ChunkSize + IntLocation.Y*ChunkSize + IntLocation.Z ;
	
			int32 Counter = -1;
			int32 i = 0;
			while (Counter + CompressedChunkData[i].StackSize < BlockIndex)
			{
				Counter += CompressedChunkData[i].StackSize;
				i += 1;
				if (i >= CompressedChunkData.Num())
				{
					FVoxel Temp;
					Temp.VoxelType = "Error";
					return Temp;
				}
			}
			return CompressedChunkData[i].Voxel;
		}
		else
		{
			return UncompressedChunkData[IntLocation.X*ChunkSize*ChunkSize + IntLocation.Y*ChunkSize +IntLocation.Z];
		}
		
	}

	void RemoveVoxel(int32 x, int32 y, int32 z)
	{
		RemoveVoxel(FIntVector(x,y,z));
	}

	void RemoveVoxel(FIntVector3 VoxelLocation){

		if (IsCompressed)
		{
			auto BlockIndex = VoxelLocation.X*ChunkSize*ChunkSize + VoxelLocation.Y*ChunkSize + VoxelLocation.Z;

			int32 i = 0;
	
			while (CompressedChunkData[i].StackSize - 1 < BlockIndex)
			{
				BlockIndex -= CompressedChunkData[i].StackSize;
				i += 1;
			}
	
	
			if (CompressedChunkData[i].Voxel == DefaultVoxel)
			{
				if (BlockIndex == 0)
				{
					CompressedChunkData[i].StackSize -= 1;
					CompressedChunkData.Insert(MakeStack(DefaultVoxel, 1), i);
				}
				else
				{
					if (BlockIndex == CompressedChunkData[i].StackSize - 1)
					{
						if (i+1 < CompressedChunkData.Num())
						{
							if (CompressedChunkData[i+1].Voxel == DefaultVoxel)
							{
								CompressedChunkData[i].StackSize -= 1;
								CompressedChunkData[i+1].StackSize += 1;
							}
							else
							{
								CompressedChunkData[i].StackSize -= 1;
								CompressedChunkData.Insert(MakeStack(DefaultVoxel, 1), i+1);
							}
						}
						else
						{
							CompressedChunkData[i].StackSize -= 1;
							CompressedChunkData.Add(MakeStack(DefaultVoxel, 1));
						}
					}
					else
					{
						const auto Temp = CompressedChunkData[i].StackSize;
						CompressedChunkData[i].StackSize = BlockIndex;
						if (i+1 < CompressedChunkData.Num())
						{
							CompressedChunkData.Insert(MakeStack(CompressedChunkData[i].Voxel, Temp - BlockIndex - 1), i+1);
							CompressedChunkData.Insert(MakeStack(DefaultVoxel, 1), i+1);
						}
						else
						{
							CompressedChunkData.Add(MakeStack(DefaultVoxel, 1));
							CompressedChunkData.Add(MakeStack(CompressedChunkData[i].Voxel, Temp - BlockIndex - 1));
						}
					}
			
				}
			}
		}
		else
		{
			UncompressedChunkData[VoxelLocation.X*ChunkSize*ChunkSize + VoxelLocation.Y*ChunkSize + VoxelLocation.Z] = DefaultVoxel;
		}
		
	}

	void SetVoxel(int32 x, int32 y, int32 z, FVoxel Voxel)
	{
		return SetVoxel(FIntVector(x,y,z), Voxel);
	}

	void SetVoxel(FIntVector VoxelLocation, FVoxel Voxel)
	{

		if (IsCompressed)
		{
			auto BlockIndex = VoxelLocation.X*ChunkSize*ChunkSize + VoxelLocation.Y*ChunkSize + VoxelLocation.Z;

			int32 i = 0;
	
			while (CompressedChunkData[i].StackSize - 1 < BlockIndex)
			{
				BlockIndex -= (CompressedChunkData)[i].StackSize;
				i += 1;
			}
	
	
			if ((CompressedChunkData)[i].Voxel != Voxel )
			{
				if (BlockIndex == 0)
				{
					(CompressedChunkData)[i].StackSize -= 1;
					CompressedChunkData.Insert(MakeStack(Voxel, 1), i);
				}
				else
				{
					if (BlockIndex == (CompressedChunkData)[i].StackSize - 1)
					{
						if (i+1 < CompressedChunkData.Num())
						{
							if ((CompressedChunkData)[i+1].Voxel == Voxel )
							{
								(CompressedChunkData)[i].StackSize -= 1;
								(CompressedChunkData)[i+1].StackSize += 1;
							}
							else
							{
								(CompressedChunkData)[i].StackSize -= 1;
								CompressedChunkData.Insert(MakeStack(Voxel, 1), i+1);
							}
						}
						else
						{
							(CompressedChunkData)[i].StackSize -= 1;
							CompressedChunkData.Add(MakeStack(Voxel, 1));
						}
					}
					else
					{
						const auto Temp = (CompressedChunkData)[i].StackSize;
						(CompressedChunkData)[i].StackSize = BlockIndex;
						if (i+1 < CompressedChunkData.Num())
						{
							CompressedChunkData.Insert(MakeStack((CompressedChunkData)[i].Voxel, Temp - BlockIndex - 1), i+1);
							CompressedChunkData.Insert(MakeStack(Voxel, 1), i+1);
						}
						else
						{
							CompressedChunkData.Add(MakeStack(Voxel, 1));
							CompressedChunkData.Add(MakeStack((CompressedChunkData)[i].Voxel, Temp - BlockIndex - 1));
						}
					}
				}
			}
		}
		else
		{
			if (UncompressedChunkData.Num() == 0)
			{
				UncompressedChunkData.SetNum(ChunkSize*ChunkSize*ChunkSize);
			}
			UncompressedChunkData[VoxelLocation.X*ChunkSize*ChunkSize + VoxelLocation.Y*ChunkSize + VoxelLocation.Z] = Voxel;
		}
	}

	FChunkData& operator=(FChunkData A) {
		const bool IsCompressedCopy = A.IsCompressed;
		IsCompressed = IsCompressedCopy;
		if(IsCompressed)
		{
			CompressedChunkData = A.CompressedChunkData;
		}
		else
		{
			UncompressedChunkData = A.UncompressedChunkData;
		}
		return *this;
	}
};


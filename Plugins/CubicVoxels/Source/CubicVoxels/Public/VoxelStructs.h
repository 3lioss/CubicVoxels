#pragma once
#include "GlobalPluginParameters.h"
#include "ProceduralMeshComponent.h" 
#include "Engine/DataTable.h"
#include "VoxelStructs.generated.h"

USTRUCT()
struct FMeshData
{
	/*Struct to hold the procedural mesh data of a chunk*/

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
	/*Type used in data tables to link voxel types with materials*/
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* VoxelMaterial;
	
};

USTRUCT(BlueprintType)
struct FVoxel
{
	/*Base voxel type*/
	GENERATED_USTRUCT_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite)
	FName VoxelType; //The name "Null" is reserved for an empty voxel

	UPROPERTY(SaveGame,BlueprintReadWrite)
	bool IsTransparent;
	
	UPROPERTY(SaveGame, BlueprintReadWrite)
	bool IsSolid;

	//The default voxel is air
	FVoxel()
	{
		VoxelType = "Air";
		IsTransparent = true;
		IsSolid = false;
	}

	operator bool() const
	{
		return VoxelType != "Null";
	}
	
};

inline bool operator== (const FVoxel& V1, const FVoxel& V2)
{
	return (V1.VoxelType == V2.VoxelType);
}

inline FVoxel DefaultVoxel = FVoxel(); //global variable used to denote an air voxel

USTRUCT()
struct FVoxelStack
{
	/*A stack of voxels */
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

	FVoxelStack(FVoxel InputVoxel, int32 InputStackSize)
	{
		Voxel = InputVoxel;
		StackSize = InputStackSize;
	}
};

USTRUCT()
struct FChunkData
{
	GENERATED_USTRUCT_BODY()

	bool IsCompressed = false; //TODO: use this boolean as a necessary condition for saving on disk

	UPROPERTY(SaveGame)
	TArray<FVoxel> UncompressedChunkData;

	UPROPERTY(SaveGame)
	TArray<FVoxelStack> CompressedChunkData;

	UPROPERTY(SaveGame)
	bool IsAdditive = false;

	FChunkData()
	{
		UncompressedChunkData.SetNum(ChunkSize*ChunkSize*ChunkSize);
	}

	//Lookup functions

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

	//Chunk edition functions

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
					CompressedChunkData.Insert(FVoxelStack(DefaultVoxel, 1), i);
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
								CompressedChunkData.Insert(FVoxelStack(DefaultVoxel, 1), i+1);
							}
						}
						else
						{
							CompressedChunkData[i].StackSize -= 1;
							CompressedChunkData.Add(FVoxelStack(DefaultVoxel, 1));
						}
					}
					else
					{
						const auto Temp = CompressedChunkData[i].StackSize;
						CompressedChunkData[i].StackSize = BlockIndex;
						if (i+1 < CompressedChunkData.Num())
						{
							CompressedChunkData.Insert(FVoxelStack(CompressedChunkData[i].Voxel, Temp - BlockIndex - 1), i+1);
							CompressedChunkData.Insert(FVoxelStack(DefaultVoxel, 1), i+1);
						}
						else
						{
							CompressedChunkData.Add(FVoxelStack(DefaultVoxel, 1));
							CompressedChunkData.Add(FVoxelStack(CompressedChunkData[i].Voxel, Temp - BlockIndex - 1));
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
					CompressedChunkData.Insert(FVoxelStack(Voxel, 1), i);
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
								CompressedChunkData.Insert(FVoxelStack(Voxel, 1), i+1);
							}
						}
						else
						{
							(CompressedChunkData)[i].StackSize -= 1;
							CompressedChunkData.Add(FVoxelStack(Voxel, 1));
						}
					}
					else
					{
						const auto Temp = (CompressedChunkData)[i].StackSize;
						(CompressedChunkData)[i].StackSize = BlockIndex;
						if (i+1 < CompressedChunkData.Num())
						{
							CompressedChunkData.Insert(FVoxelStack((CompressedChunkData)[i].Voxel, Temp - BlockIndex - 1), i+1);
							CompressedChunkData.Insert(FVoxelStack(Voxel, 1), i+1);
						}
						else
						{
							CompressedChunkData.Add(FVoxelStack(Voxel, 1));
							CompressedChunkData.Add(FVoxelStack((CompressedChunkData)[i].Voxel, Temp - BlockIndex - 1));
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

	void ApplyAsAdditive(FChunkData AdditiveChunk) //Replace every voxel that is not marked as "None" in the additive chunk by its value in the additive chunk.
	{
		for (int32 x = 0; x < ChunkSize; x++)
		{
			for (int32 y = 0; y < ChunkSize; y++)
			{
				for (int32 z = 0; z < ChunkSize; z++)
				{
					if (AdditiveChunk.GetVoxelAt(x, y, z))
					{
						SetVoxel(x,y,z, AdditiveChunk.GetVoxelAt(x, y, z));
					}
				}
			}
		}
	}

	//Operators and static functions

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

	static bool Compress(FChunkData A) //returns true iff the chunk was successfully compressed
	{
		if(A.IsCompressed)
		{
			return false;
		}
		else //TODO: add mutex lock here to avoid unwanted accesses to the chunk data during asynchronous compression
		{
			//UE_LOG(LogTemp, Warning, TEXT("Size before compression: %llu"), sizeof(A.UncompressedChunkData[0])*A.UncompressedChunkData.Num());
			A.CompressedChunkData.Empty();
			int32 BlockCounter = 0;

			for (int32 x = 0; x < ChunkSize; x++)
			{
				for (int32 y = 0; y < ChunkSize; y++)
				{
					for (int32 z = 0; z < ChunkSize; z++)
					{

						if (x ==0 && y==0 && z==0)
						{
							A.CompressedChunkData.Add(FVoxelStack( A.UncompressedChunkData[0], 1));
						}
						else
						{
							if ( A.CompressedChunkData[BlockCounter].Voxel == A.UncompressedChunkData[x*ChunkSize*ChunkSize + y*ChunkSize + z] )
							{
								A.CompressedChunkData[BlockCounter].StackSize += 1;
							}
							else
							{
								A.CompressedChunkData.Add( FVoxelStack( A.UncompressedChunkData[x*ChunkSize*ChunkSize + y*ChunkSize + z], 1)  );
								BlockCounter +=1;
							}
						}
					}
				}
			}
			A.IsCompressed = true;
			A.UncompressedChunkData.Empty();
			//UE_LOG(LogTemp, Warning, TEXT("Estimated size after compression: %llu"), sizeof(A.CompressedChunkData[0])*A.CompressedChunkData.Num());
			return true;
		}
	}

	static FChunkData EmptyChunkData()
	{
		auto Result = FChunkData();
		Result.IsCompressed = true;
		auto NullVoxel = FVoxel();
		NullVoxel.VoxelType = "Null";
		
		Result.CompressedChunkData = TArray<FVoxelStack>();
		Result.CompressedChunkData.Add(FVoxelStack(NullVoxel, ChunkSize*ChunkSize*ChunkSize));
		return Result;
	}
};


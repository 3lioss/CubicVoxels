#pragma once

//Enum that represents the loading state of a chunk
enum class EChunkState
{
	Loading, Loaded, Unloading
};

//Enum that represents the type of threaded work to be realised to generate a given chunk
enum class EChunkThreadedWorkOrderType
{
	GenerationAndMeshing, MeshingFromData, GeneratingAndMeshingWithAdditiveData, GeneratingExistingChunksSides
};

UENUM()
enum class EVoxelWorldNetworkMode
{
	ClientOnly
};
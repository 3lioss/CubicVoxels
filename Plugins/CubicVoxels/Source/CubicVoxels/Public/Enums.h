#pragma once

enum class EChunkState
{
	Loading, Loaded, Unloading
};

enum class EChunkThreadedWorkOrderType
{
	GenerationAndMeshing, MeshingFromData
};
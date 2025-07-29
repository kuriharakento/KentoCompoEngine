#pragma once
#include <vector>

#include "base/GraphicsTypes.h"

namespace ParticleMath
{
	std::vector<VertexData> MakePlaneVertexData();
	std::vector<VertexData> MakeRingVertexData();
	std::vector<VertexData> MakeCylinderVertexData();
	std::vector<VertexData> MakeSphereVertexData();
	std::vector<VertexData> MakeTorusVertexData();
	std::vector<VertexData> MakeStarVertexData();
	std::vector<VertexData> MakeHeartVertexData();
	std::vector<VertexData> MakeSpiralVertexData();
	std::vector<VertexData> MakeConeVertexData();
}

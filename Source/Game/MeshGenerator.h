#include <vector>
#pragma once

namespace Urho3D
{
	class Model;
}

using namespace Urho3D;


enum MeshDirection : char {
	MESH_DIR_None = 0b00000000, // None

	MESH_DIR_NegativeZ = 0b00000001, // Left
	MESH_DIR_NegativeY = 0b00000010, // Bottom
	MESH_DIR_PositiveX = 0b00000100, // Front
	MESH_DIR_PositiveZ = 0b00001000, // Right	
	MESH_DIR_PositiveY = 0b00010000, // Top
	MESH_DIR_NegativeX = 0b00100000, // Back
};

struct Face
{
public:
	Face(std::vector<float> vertexData, std::vector<uint16_t> indexData, uint16_t count);
	std::vector<float> vertexData;
	std::vector<uint16_t> indexData;
	uint16_t count;


};

class Mesh
{	
public:	
	Mesh(std::vector<Face> faces);
	std::vector<float> GetAllVertexData();
	std::vector<uint16_t> GetAllIndexData();
private:
	std::vector<Face> faces_;
};


class MeshGenerator
{	
public:	
	MeshGenerator(Context* context);
	~MeshGenerator();
	SharedPtr<Model> CreateModel(MeshDirection directions);
private:
	Context* context_;
	Face GenerateVertexesForFace(uint16_t& dir);
};


#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/IndexBuffer.h>
#include <Urho3D/Graphics/VertexBuffer.h>
#include <Urho3D/Graphics/Geometry.h>
#include <vector>

#include "MeshGenerator.h"


const unsigned numSingularIndex = 6;
const Vector2 singluarVertex[] = {
	Vector2(-0.5f, -0.5f),
	Vector2(0.5f, -0.5f),
	Vector2(-0.5f, 0.5f),
	Vector2(0.5f, 0.5f)
};

const unsigned char singularIndex[] = {
	2, 0, 1,
	2, 1, 3
};
const unsigned char singularIndex_inv[] = {
	0, 2, 1,
	1, 2, 3
};



MeshGenerator::MeshGenerator(Context * context)
{
	context_ = context;
}

MeshGenerator::~MeshGenerator()
{
}

Face MeshGenerator::GenerateVertexesForFace(uint16_t& dir) {
	std::vector<float> vertexData = std::vector<float>();
	std::vector<uint16_t> indexData = std::vector<uint16_t>();

	int indexPosCounter = 0;

	for (int indexPos = 0; indexPos < numSingularIndex; indexPos++) {

		Vector2 point = singluarVertex[((dir < 3) ? singularIndex : singularIndex_inv)[indexPos]];

		if (dir == 0 || dir == 3) {
			vertexData.push_back(point.x_); // x
			vertexData.push_back(point.y_); // y
			vertexData.push_back(dir < 3 ? 0.5f : -0.5f); // z
		}
		else if (dir == 1 || dir == 4) {
			vertexData.push_back(point.x_); // x
			vertexData.push_back(dir < 3 ? -0.5f : 0.5f); // y
			vertexData.push_back(point.y_); // z
		}
		else {
			vertexData.push_back(dir < 3 ? 0.5f : -0.5f); // x
			vertexData.push_back(point.x_); // y
			vertexData.push_back(point.y_); // z

		}

		// Add Normals... These will be calculated dynamicly
		vertexData.push_back(0.0f);
		vertexData.push_back(0.0f);
		vertexData.push_back(0.0f);

		indexData.push_back(indexPosCounter++);

	}

	for (int i = 0; i < indexData.size(); i += 3)
	{
		Vector3& v1 = *(reinterpret_cast<Vector3*>(&vertexData.at(6 * i)));
		Vector3& v2 = *(reinterpret_cast<Vector3*>(&vertexData.at(6 * (i + 1))));
		Vector3 & v3 = *(reinterpret_cast<Vector3*>(&vertexData.at(6 * (i + 2))));
		Vector3 & n1 = *(reinterpret_cast<Vector3*>(&vertexData.at(6 * i + 3)));
		Vector3 & n2 = *(reinterpret_cast<Vector3*>(&vertexData.at(6 * (i + 1) + 3)));
		Vector3 & n3 = *(reinterpret_cast<Vector3*>(&vertexData.at(6 * (i + 2) + 3)));

		Vector3 edge1 = v1 - v2;
		Vector3 edge2 = v1 - v3;
		n1 = n2 = n3 = edge1.CrossProduct(edge2).Normalized();
	}

	return Face(vertexData, indexData, indexData.size());

}


SharedPtr<Model> MeshGenerator::CreateModel(MeshDirection directions) {


	std::vector<Face> faces;

	for (uint16_t directionPosition = 0; directionPosition < 6; directionPosition++)
	{
		MeshDirection meshDirection = (MeshDirection)(0b00000001 << directionPosition);
		if ((meshDirection & directions) >> directionPosition == 1) {
			Face face = GenerateVertexesForFace(directionPosition);
			faces.push_back(face);
		}
	}
	Mesh mesh = Mesh(faces);
	std::vector<float> vertexData = mesh.GetAllVertexData();
	std::vector<uint16_t> indexData = mesh.GetAllIndexData();
	int numOfVerticies = indexData.size() / 6;
	SharedPtr<Model> fromScratchModel(new Model(context_));
	SharedPtr<VertexBuffer> vb(new VertexBuffer(context_));
	SharedPtr<IndexBuffer> ib(new IndexBuffer(context_));
	SharedPtr<Geometry> geom(new Geometry(context_));

	// Shadowed buffer needed for raycasts to work, and so that data can be automatically restored on device loss
	vb->SetShadowed(true);
	// We could use the "legacy" element bitmask to define elements for more compact code, but let's demonstrate
	// defining the vertex elements explicitly to allow any element types and order
	PODVector<VertexElement> elements;
	elements.Push(VertexElement(TYPE_VECTOR3, SEM_POSITION));
	elements.Push(VertexElement(TYPE_VECTOR3, SEM_NORMAL));
	vb->SetSize(numOfVerticies, elements);
	vb->SetData(static_cast<void*>(vertexData.data()));

	ib->SetShadowed(true);
	ib->SetSize(numOfVerticies, false);
	ib->SetData(static_cast<void*>(indexData.data()));

	geom->SetVertexBuffer(0, vb);
	geom->SetIndexBuffer(ib);
	geom->SetDrawRange(TRIANGLE_LIST, 0, numOfVerticies);

	fromScratchModel->SetNumGeometries(1);
	fromScratchModel->SetGeometry(0, 0, geom);
	fromScratchModel->SetBoundingBox(BoundingBox(Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.5f, 0.5f, 0.5f)));

	// Though not necessary to render, the vertex & index buffers must be listed in the model so that it can be saved properly
	Vector<SharedPtr<VertexBuffer> > vertexBuffers;
	Vector<SharedPtr<IndexBuffer> > indexBuffers;
	vertexBuffers.Push(vb);
	indexBuffers.Push(ib);
	// Morph ranges could also be not defined. Here we simply define a zero range (no morphing) for the vertex buffer
	PODVector<unsigned> morphRangeStarts;
	PODVector<unsigned> morphRangeCounts;
	morphRangeStarts.Push(0);
	morphRangeCounts.Push(0);
	fromScratchModel->SetVertexBuffers(vertexBuffers, morphRangeStarts, morphRangeCounts);
	fromScratchModel->SetIndexBuffers(indexBuffers);

	return fromScratchModel;
}



Face::Face(std::vector<float> p_vertexData, std::vector<uint16_t> p_indexData, uint16_t p_count) :
	vertexData(p_vertexData),
	indexData(p_indexData),
	count(p_count) {}



Mesh::Mesh(std::vector<Face> faces) :
	faces_(faces) {}

std::vector<uint16_t> Mesh::GetAllIndexData() {
	
	std::vector<uint16_t> indexData;
	int indexCounter = 0;
	for (Face face : faces_)
	{
		for (int i = 0; i < face.vertexData.size(); i++) {
			indexData.push_back(indexCounter);
			indexCounter++;
		}
	}

	return indexData;
}

std::vector<float> Mesh::GetAllVertexData() {

	std::vector<float> vertexData;
	for (Face face : faces_)
	{
		vertexData.insert(vertexData.end(), face.vertexData.begin(), face.vertexData.end());
	}

	return vertexData;
}
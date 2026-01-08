#include "OBJLoader.h"

#include <fstream>

#include "MTLLoader.h"

bool OBJLoader::Load(const std::filesystem::path& fullPath, Model& model)
{
	model.path = fullPath;
	return Parse(model);
}

bool OBJLoader::Parse(Model& model)
{
	std::ifstream file(model.path);
	if (!file.is_open())
	{
		std::cerr << "Failed to open file: " << model.path.generic_string() << std::endl;
		return false;
	}

	auto endSubMesh = [&](Mesh& mesh) {
		std::vector<SubMesh>& subMeshes = mesh.subMeshes;
			if (!subMeshes.empty()) {
				subMeshes.back().count = static_cast<uint32_t>(mesh.indices.size()) - subMeshes.back().startIndex;
			}
		};

	Mesh currentMesh;
	std::string line;
	Vec3i lastSize = {};
	while (std::getline(file, line)) {
		std::istringstream iss(line);
		std::string token;
		iss >> token;

		if (token == "o" || token == "g")
		{
			lastSize = lastSize + Vec3i{ static_cast<int>(currentMesh.positions.size()), static_cast<int>(currentMesh.textureUVs.size()), static_cast<int>(currentMesh.normals.size()) };
			endSubMesh(currentMesh);
			if (!currentMesh.name.empty()) {
				model.meshes.push_back(currentMesh);
			}
			currentMesh = Mesh();
			iss >> currentMesh.name;
		}
		if (token == "mtllib")
		{
			std::string mtlFileName;
			iss >> mtlFileName;
			model.materials = MTLLoader::Load(model.path.parent_path() / mtlFileName);
		}
		if (token == "usemtl")
		{
			endSubMesh(currentMesh);
			std::string materialName;
			iss >> materialName;
			SubMesh subMesh = SubMesh();
			subMesh.startIndex = static_cast<uint32_t>(currentMesh.indices.size());
			subMesh.materialName = materialName;

			currentMesh.subMeshes.push_back(subMesh);
		}
		if (token == "v")
		{
			Vec3f position;
			iss >> position.x >> position.y >> position.z;
			currentMesh.positions.push_back(position);
		}
		else if (token == "vt")
		{
			Vec2f uv;
			iss >> uv.x >> uv.y;
			uv.y = 1 - uv.y;
			currentMesh.textureUVs.push_back(uv);
		}
		else if (token == "vn")
		{
			Vec3f normal;
			iss >> normal.x >> normal.y >> normal.z;

			currentMesh.normals.push_back(normal);
		}
		else if (token == "f")
		{
			if (currentMesh.subMeshes.empty())
			{
				SubMesh subMesh = SubMesh();
				subMesh.startIndex = static_cast<uint32_t>(currentMesh.indices.size());
				currentMesh.subMeshes.push_back(subMesh);
			}

			size_t count = 0;
			std::string indexStr;
			while (iss >> indexStr) {
				Vec3i indices;
				ParseFaceIndex(std::ref(indices), indexStr);
				currentMesh.indices.push_back(indices - lastSize);
				count++;
			}

			if (count == 4)
			{
				size_t lastIndex = currentMesh.indices.size() - 1;
				Vec3i i1 = currentMesh.indices[lastIndex - 3];
				Vec3i i2 = currentMesh.indices[lastIndex - 2];
				Vec3i i3 = currentMesh.indices[lastIndex - 1];
				Vec3i i4 = currentMesh.indices[lastIndex];

				// Remove the quad indices
				currentMesh.indices.pop_back();
				currentMesh.indices.pop_back();
				currentMesh.indices.pop_back();
				currentMesh.indices.pop_back();

				// Push the first triangle indices
				currentMesh.indices.push_back(i1);
				currentMesh.indices.push_back(i2);
				currentMesh.indices.push_back(i3);

				// Push the second triangle indices
				currentMesh.indices.push_back(i1);
				currentMesh.indices.push_back(i3);
				currentMesh.indices.push_back(i4);
			}
		}
	}
	if (!currentMesh.name.empty()) {
		endSubMesh(currentMesh);
		model.meshes.push_back(currentMesh);
	}

	for (auto& m_mesh : model.meshes)
	{
		ComputeVertices(m_mesh);
	}

	return true;
}


void OBJLoader::ParseFaceIndex(Vec3i& indices, const std::string& indexStr)
{
	size_t firstSlash = indexStr.find('/');
	size_t secondSlash = indexStr.find('/', firstSlash + 1);

	// Vertex index
	if (firstSlash != std::string::npos) {
		indices.x = std::stoi(indexStr.substr(0, firstSlash)) - 1; // OBJ indices start from 1
	}

	// Texture index
	if (firstSlash != std::string::npos && secondSlash != std::string::npos) {
		std::string uvIndexStr = indexStr.substr(firstSlash + 1, secondSlash - firstSlash - 1);
		if (!uvIndexStr.empty()) {
			indices.y = std::stoi(uvIndexStr) - 1;
		}
	}

	// Normal index
	if (secondSlash != std::string::npos && secondSlash + 1 < indexStr.size()) {
		std::string normalIndexStr = indexStr.substr(secondSlash + 1);
		if (!normalIndexStr.empty()) {
			indices.z = std::stoi(normalIndexStr) - 1;
		}
	}
}

bool AreVerticesSimilar(const Vec3f& v1, const Vec2f& uv1, const Vec3f& n1,
	const Vec3f& v2, const Vec2f& uv2, const Vec3f& n2)
{
	constexpr float epsilon = 0.0001f;
	return (std::abs(v1.x - v2.x) < epsilon &&
		std::abs(v1.y - v2.y) < epsilon &&
		std::abs(v1.z - v2.z) < epsilon &&
		std::abs(uv1.x - uv2.x) < epsilon &&
		std::abs(uv1.y - uv2.y) < epsilon &&
		std::abs(n1.x - n2.x) < epsilon &&
		std::abs(n1.y - n2.y) < epsilon &&
		std::abs(n1.z - n2.z) < epsilon);
}

void OBJLoader::ComputeVertices(Mesh& mesh)
{
	mesh.tangents.resize(mesh.indices.size());

	for (size_t k = 0; k < mesh.indices.size(); k += 3)
	{
		const Vec3i& idx0 = mesh.indices[k];
		const Vec3i& idx1 = mesh.indices[k + 1];
		const Vec3i& idx2 = mesh.indices[k + 2];

		const Vec3f& Edge1 = mesh.positions[idx1.x] - mesh.positions[idx0.x];
		const Vec3f& Edge2 = mesh.positions[idx2.x] - mesh.positions[idx0.x];

		const float DeltaU1 = mesh.textureUVs[idx1.y].x - mesh.textureUVs[idx0.y].x;
		const float DeltaV1 = mesh.textureUVs[idx1.y].y - mesh.textureUVs[idx0.y].y;
		const float DeltaU2 = mesh.textureUVs[idx2.y].x - mesh.textureUVs[idx0.y].x;
		const float DeltaV2 = mesh.textureUVs[idx2.y].y - mesh.textureUVs[idx0.y].y;

		float f = DeltaU1 * DeltaV2 - DeltaU2 * DeltaV1;
		if (fabs(f) < 1e-6) {
			f = 1.0f; // Prevent division by zero and provide a default value for 'f'
		}
		else {
			f = 1.0f / f;
		}

		Vec3f Tangent;

		Tangent.x = f * (DeltaV2 * Edge1.x - DeltaV1 * Edge2.x);
		Tangent.y = f * (DeltaV2 * Edge1.y - DeltaV1 * Edge2.y);
		Tangent.z = f * (DeltaV2 * Edge1.z - DeltaV1 * Edge2.z);

		Tangent.Normalize();

		mesh.tangents[k] = Tangent;
		mesh.tangents[k + 1] = Tangent;
		mesh.tangents[k + 2] = Tangent;
	}

	for (size_t i = 0; i < mesh.indices.size(); i++)
	{
		const Vec3i& idx = mesh.indices[i];

		mesh.finalVertices.push_back(mesh.positions[idx.x].x);
		mesh.finalVertices.push_back(mesh.positions[idx.x].y);
		mesh.finalVertices.push_back(mesh.positions[idx.x].z);

		mesh.finalVertices.push_back(mesh.textureUVs[idx.y].x);
		mesh.finalVertices.push_back(mesh.textureUVs[idx.y].y);

		mesh.finalVertices.push_back(mesh.normals[idx.z].x);
		mesh.finalVertices.push_back(mesh.normals[idx.z].y);
		mesh.finalVertices.push_back(mesh.normals[idx.z].z);

		mesh.finalVertices.push_back(mesh.tangents[i].x);
		mesh.finalVertices.push_back(mesh.tangents[i].y);
		mesh.finalVertices.push_back(mesh.tangents[i].z);
	}
}
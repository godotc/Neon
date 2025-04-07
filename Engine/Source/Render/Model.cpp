#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "Model.h"
#include "Texture.h"

#include "Core/FileSystem/FileSystem.h"
#include "Core/Log.h"
#include "Render/CommandBuffer.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>


bool Model::loadFromOBJ(const std::string &filePath, std::shared_ptr<CommandBuffer> commandBuffer)
{
    if (isLoaded) {
        return true;
    }

    // Get directory path
    size_t lastSlash = filePath.find_last_of("/\\");
    directory        = (lastSlash != std::string::npos) ? filePath.substr(0, lastSlash + 1) : "";

    // Use FileSystem to check if file exists
    if (!FileSystem::get()->isFileExists(filePath)) {
        NE_CORE_ERROR("Model file does not exist: {}", filePath);
        return false;
    }

    // Read the file using FileSystem
    std::string fileContent;
    if (!FileSystem::get()->readFileToString(filePath, fileContent)) {
        NE_CORE_ERROR("Failed to read model file: {}", filePath);
        return false;
    }

    // Create Assimp importer
    Assimp::Importer importer;

    // Load the model from memory
    const aiScene *scene = importer.ReadFileFromMemory(
        fileContent.data(),
        fileContent.size(),
        aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_FlipUVs |
            aiProcess_CalcTangentSpace);

    // Check for errors
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        NE_CORE_ERROR("Assimp error: {}", importer.GetErrorString());
        return false;
    }

    // Process all meshes in the scene
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh *mesh = scene->mMeshes[i];
        Mesh    newMesh;

        // Get mesh name
        newMesh.name = mesh->mName.length > 0 ? mesh->mName.C_Str() : "unnamed_mesh";

        // Process vertices
        for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
            Vertex vertex;

            // Position
            vertex.position.x = mesh->mVertices[j].x;
            vertex.position.y = mesh->mVertices[j].y;
            vertex.position.z = mesh->mVertices[j].z;

            // Normal
            if (mesh->HasNormals()) {
                vertex.normal.x = mesh->mNormals[j].x;
                vertex.normal.y = mesh->mNormals[j].y;
                vertex.normal.z = mesh->mNormals[j].z;
            }

            // Texture coordinates
            if (mesh->mTextureCoords[0]) {
                vertex.texCoord.x = mesh->mTextureCoords[0][j].x;
                vertex.texCoord.y = mesh->mTextureCoords[0][j].y;
            }
            else {
                vertex.texCoord = glm::vec2(0.0f, 0.0f);
            }

            // Colors
            if (mesh->HasVertexColors(0)) {
                vertex.color.r = mesh->mColors[0][j].r;
                vertex.color.g = mesh->mColors[0][j].g;
                vertex.color.b = mesh->mColors[0][j].b;
                vertex.color.a = mesh->mColors[0][j].a;
            }

            newMesh.vertices.push_back(vertex);
        }

        // Process indices
        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            aiFace face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++) {
                newMesh.indices.push_back(face.mIndices[k]);
            }
        }

        // Process materials/textures
        if (mesh->mMaterialIndex >= 0 && commandBuffer) {
            aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];

            // Load diffuse texture
            if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString texturePath;
                if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
                    std::string fullPath = directory;
                    if (!fullPath.empty() && fullPath.back() != '/' && fullPath.back() != '\\') {
                        fullPath += '/';
                    }
                    fullPath += texturePath.C_Str();
                    if (FileSystem::get()->isFileExists(fullPath)) {
                        newMesh.diffuseTexture = Texture::Create(fullPath, commandBuffer);
                        if (!newMesh.diffuseTexture) {
                            NE_CORE_WARN("Failed to load diffuse texture: {}", fullPath);
                        }
                    }
                }
            }
        }

        meshes.push_back(newMesh);
    }

    isLoaded = true;
    return true;
}

void Model::draw(SDL_GPURenderPass *renderPass, SDL_GPUTexture *defaultTexture)
{
    // This function would be called if we had direct access to GPU buffers
    // In our current setup, we'll refactor Entry.cpp to handle the drawing
    // This is just a placeholder for potential future improvements
}

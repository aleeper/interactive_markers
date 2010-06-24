/*
 * Copyright (c) 2010, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <rviz_mesh_loader/assimp_parser.h>
#include <rviz_mesh_loader/exception.h>

#include <rviz_msgs/Mesh.h>

#include <rviz_uuid/uuid.h>

#include <resource_retriever/retriever.h>

#include <boost/filesystem.hpp>

#include <assimp/assimp.hpp>
#include <assimp/aiScene.h>
#include <assimp/aiPostProcess.h>
#include <assimp/IOStream.h>
#include <assimp/IOSystem.h>

#include <ros/assert.h>

namespace fs = boost::filesystem;

namespace rviz_mesh_loader
{

class ResourceIOStream : public Assimp::IOStream
{
public:
  ResourceIOStream(const resource_retriever::MemoryResource& res)
  : res_(res)
  , pos_(res.data.get())
  {}

  ~ResourceIOStream()
  {}

  size_t Read(void* buffer, size_t size, size_t count)
  {
    size_t to_read = size * count;
    if (pos_ + to_read > res_.data.get() + res_.size)
    {
      to_read = res_.size - (pos_ - res_.data.get());
    }

    memcpy(buffer, pos_, to_read);
    pos_ += to_read;

    return to_read;
  }

  size_t Write( const void* buffer, size_t size, size_t count) { ROS_BREAK(); return 0; }

  aiReturn Seek( size_t offset, aiOrigin origin)
  {
    uint8_t* new_pos = 0;
    switch (origin)
    {
    case aiOrigin_SET:
      new_pos = res_.data.get() + offset;
      break;
    case aiOrigin_CUR:
      new_pos = pos_ + offset; // TODO is this right?  can offset really not be negative
      break;
    case aiOrigin_END:
      new_pos = res_.data.get() + res_.size - offset; // TODO is this right?
      break;
    default:
      ROS_BREAK();
    }

    if (new_pos < res_.data.get() || new_pos > res_.data.get() + res_.size)
    {
      return aiReturn_FAILURE;
    }

    pos_ = new_pos;
    return aiReturn_SUCCESS;
  }

  size_t Tell() const
  {
    return pos_ - res_.data.get();
  }

  size_t FileSize() const
  {
    return res_.size;
  }

  void Flush() {}

private:
  resource_retriever::MemoryResource res_;
  uint8_t* pos_;
};

class ResourceIOSystem : public Assimp::IOSystem
{
public:
  ResourceIOSystem()
  {
  }

  ~ResourceIOSystem()
  {
  }

  // Check whether a specific file exists
  bool Exists(const char* file) const
  {
    // Ugly -- two retrievals where there should be one (Exists + Open)
    // resource_retriever needs a way of checking for existence
    // TODO: cache this
    resource_retriever::MemoryResource res;
    try
    {
      res = retriever_.get(file);
    }
    catch (resource_retriever::Exception& e)
    {
      return false;
    }

    return true;
  }

  // Get the path delimiter character we'd like to see
  char getOsSeparator() const
  {
    return '/';
  }

  // ... and finally a method to open a custom stream
  Assimp::IOStream* Open(const char* file, const char* mode)
  {
    ROS_ASSERT(mode == std::string("r") || mode == std::string("rb"));

    // Ugly -- two retrievals where there should be one (Exists + Open)
    // resource_retriever needs a way of checking for existence
    resource_retriever::MemoryResource res;
    try
    {
      res = retriever_.get(file);
    }
    catch (resource_retriever::Exception& e)
    {
      return 0;
    }

    return new ResourceIOStream(res);
  }

  void Close(Assimp::IOStream* stream) { delete stream; }

private:
  mutable resource_retriever::Retriever retriever_;
};

// Mostly stolen from gazebo
void buildMesh(const aiScene* scene, const aiNode* node, rviz_msgs::Mesh& out_mesh)
{
  if (!node)
  {
    return;
  }

  aiMatrix4x4 transform = node->mTransformation;
  aiNode *pnode = node->mParent;
  while (pnode)
  {
    // Don't convert to y-up orientation, which is what the root node in
    // Assimp does
    //if (pnode->mParent != NULL)
      transform = pnode->mTransformation * transform;
    pnode = pnode->mParent;
  }

  for (uint32_t i = 0; i < node->mNumMeshes; i++)
  {
    aiMesh* input_mesh = scene->mMeshes[node->mMeshes[i]];
    out_mesh.submeshes.resize(out_mesh.submeshes.size() + 1);
    rviz_msgs::SubMesh& submesh = out_mesh.submeshes.back();

    submesh.has_normals = input_mesh->HasNormals();
    submesh.has_tex_coords = input_mesh->HasTextureCoords(0);
    submesh.has_vertex_colors = input_mesh->HasVertexColors(0);

    // Add the vertices
    for (uint32_t j = 0; j < input_mesh->mNumVertices; j++)
    {
      aiVector3D p = input_mesh->mVertices[j];
      p *= transform;
      rviz_msgs::Vertex v;
      v.position.x = p.x;
      v.position.y = p.y;
      v.position.z = p.z;

      if (input_mesh->HasNormals())
      {
        v.normal.x = input_mesh->mNormals[j].x;
        v.normal.y = input_mesh->mNormals[j].y;
        v.normal.z = input_mesh->mNormals[j].z;
      }

      if (input_mesh->HasTextureCoords(0))
      {
        v.tex.u = input_mesh->mTextureCoords[0][j].x;
        v.tex.v = input_mesh->mTextureCoords[0][j].y;
        v.texcoord_dims = 2;
      }

      if (input_mesh->HasVertexColors(0))
      {
        v.color.r = input_mesh->mColors[0][j].r;
        v.color.g = input_mesh->mColors[0][j].g;
        v.color.b = input_mesh->mColors[0][j].b;
        v.color.a = input_mesh->mColors[0][j].a;
      }

      submesh.vertices.push_back(v);
    }

    // add the indices
    for (uint32_t j = 0; j < input_mesh->mNumFaces; j++)
    {
      aiFace& face = input_mesh->mFaces[j];
      for (uint32_t k = 0; k < face.mNumIndices; ++k)
      {
        submesh.indices.push_back(face.mIndices[k]);
      }
    }
  }

  for (uint32_t i=0; i < node->mNumChildren; ++i)
  {
    buildMesh(scene, node->mChildren[i], out_mesh);
  }
}

void loadMaterialsForMesh(const std::string& resource_path, const aiScene* scene, rviz_msgs::Mesh& mesh)
{
  for (uint32_t i = 0; i < scene->mNumMaterials; i++)
  {
    rviz_msgs::Material mat;
    mat.id = rviz_uuid::UUID::Generate();

    aiMaterial *amat = scene->mMaterials[i];

    // Just pull out texture and diffuse color for now

    aiString texName;
    aiTextureMapping mapping;
    uint32_t uvIndex;
    if (amat->GetTexture(aiTextureType_DIFFUSE,0, &texName, &mapping, &uvIndex) == aiReturn_SUCCESS)
    {
      // Assume textures are in paths relative to the mesh
      std::string texture_path = fs::path(resource_path).parent_path().string() + "/" + texName.data;
      mat.texture = texture_path;
      mat.has_texture = true;
    }

    float opacity = 1.0;
    amat->Get(AI_MATKEY_OPACITY, opacity);

    aiColor3D clr;
    if (amat->Get(AI_MATKEY_COLOR_DIFFUSE, clr) == aiReturn_SUCCESS)
    {
      mat.color.r = clr.r;
      mat.color.b = clr.g;
      mat.color.g = clr.b;
      mat.color.a = opacity;
      mat.has_color = true;
    }

    mesh.materials.push_back(mat);
  }

  for (size_t i = 0; i < mesh.submeshes.size(); ++i)
  {
    mesh.submeshes[i].material_index = scene->mMeshes[i]->mMaterialIndex;
  }
}

void meshFromAssimpScene(const std::string& filename, const aiScene* scene, rviz_msgs::Mesh& out_mesh)
{
  if (!scene->HasMeshes())
  {
    throw ParseException("No meshes found in file");
  }

  buildMesh(scene, scene->mRootNode, out_mesh);
  loadMaterialsForMesh(filename, scene, out_mesh);
}

void parseWithAssimp(uint8_t* buffer, size_t buffer_size, const std::string& filename, rviz_msgs::Mesh& out_mesh)
{
  Assimp::Importer importer;
  importer.SetIOHandler(new ResourceIOSystem());
  std::string extension = fs::extension(fs::path(filename)).substr(1);
  const aiScene* scene = importer.ReadFileFromMemory(buffer, buffer_size, aiProcess_SortByPType|aiProcess_GenNormals|aiProcess_Triangulate|aiProcess_GenUVCoords|aiProcess_FlipUVs, extension.c_str());
  if (!scene)
  {
    throw ParseException(importer.GetErrorString());
  }

  meshFromAssimpScene(filename, scene, out_mesh);
}

} // namespace rviz_mesh_loader

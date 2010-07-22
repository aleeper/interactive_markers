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

#include <rviz_renderer_ogre/points_renderer.h>
#include <rviz_renderer_ogre/init.h>
#include <rviz_renderer_ogre/renderer.h>
#include <rviz_renderer_ogre/points_material_generator.h>
#include <rviz_renderer_ogre/convert.h>

#include <rviz_msgs/Points.h>

#include <OGRE/OgreRoot.h>
#include <OGRE/OgreSceneNode.h>
#include <OGRE/OgreHardwareBufferManager.h>
#include <OGRE/OgreHardwareVertexBuffer.h>

#define POINTS_PER_VBO (1024 * 8)

namespace rviz_renderer_ogre
{

static float g_point_vertices[3] =
{
  0.0f, 0.0f, 0.0f
};

static float g_billboard_vertices[6*3] =
{
  -0.5f, 0.5f, 0.0f,
  -0.5f, -0.5f, 0.0f,
  0.5f, 0.5f, 0.0f,
  0.5f, 0.5f, 0.0f,
  -0.5f, -0.5f, 0.0f,
  0.5f, -0.5f, 0.0f,
};

#if 01
static float* g_billboard_sphere_vertices = g_billboard_vertices;
#else
static float g_billboard_sphere_vertices[3*3] =
{
  0.0f, 1.5f, 0.0f,
  -1.5f, -1.5f, 0.0f,
  1.5f, -1.5f, 0.0f,
};
#endif

static float g_box_vertices[6*6*3] =
{
  // front
  -0.5f, 0.5f, -0.5f,
  -0.5f, -0.5f, -0.5f,
  0.5f, 0.5f, -0.5f,
  0.5f, 0.5f, -0.5f,
  -0.5f, -0.5f, -0.5f,
  0.5f, -0.5f, -0.5f,

  // back
  -0.5f, 0.5f, 0.5f,
  0.5f, 0.5f, 0.5f,
  -0.5f, -0.5f, 0.5f,
  0.5f, 0.5f, 0.5f,
  0.5f, -0.5f, 0.5f,
  -0.5f, -0.5f, 0.5f,

  // right
  0.5, 0.5, 0.5,
  0.5, 0.5, -0.5,
  0.5, -0.5, 0.5,
  0.5, 0.5, -0.5,
  0.5, -0.5, -0.5,
  0.5, -0.5, 0.5,

  // left
  -0.5, 0.5, 0.5,
  -0.5, -0.5, 0.5,
  -0.5, 0.5, -0.5,
  -0.5, 0.5, -0.5,
  -0.5, -0.5, 0.5,
  -0.5, -0.5, -0.5,

  // top
  -0.5, 0.5, -0.5,
  0.5, 0.5, -0.5,
  -0.5, 0.5, 0.5,
  0.5, 0.5, -0.5,
  0.5, 0.5, 0.5,
  -0.5, 0.5, 0.5,

  // bottom
  -0.5, -0.5, -0.5,
  -0.5, -0.5, 0.5,
  0.5, -0.5, -0.5,
  0.5, -0.5, -0.5,
  -0.5, -0.5, 0.5,
  0.5, -0.5, 0.5,
};

static float g_box_normals[6*6*3] =
{
  // front
  0.0, 0.0, 1.0,
  0.0, 0.0, 1.0,
  0.0, 0.0, 1.0,
  0.0, 0.0, 1.0,
  0.0, 0.0, 1.0,
  0.0, 0.0, 1.0,

  // back
  0.0, 0.0, -1.0,
  0.0, 0.0, -1.0,
  0.0, 0.0, -1.0,
  0.0, 0.0, -1.0,
  0.0, 0.0, -1.0,
  0.0, 0.0, -1.0,

  // right
  1.0, 0.0, 0.0,
  1.0, 0.0, 0.0,
  1.0, 0.0, 0.0,
  1.0, 0.0, 0.0,
  1.0, 0.0, 0.0,
  1.0, 0.0, 0.0,

  // left
  -1.0, 0.0, 0.0,
  -1.0, 0.0, 0.0,
  -1.0, 0.0, 0.0,
  -1.0, 0.0, 0.0,
  -1.0, 0.0, 0.0,
  -1.0, 0.0, 0.0,

  // top
  0.0, 1.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 1.0, 0.0,

  // bottom
  0.0, -1.0, 0.0,
  0.0, -1.0, 0.0,
  0.0, -1.0, 0.0,
  0.0, -1.0, 0.0,
  0.0, -1.0, 0.0,
  0.0, -1.0, 0.0,
};

const Ogre::String PointsRenderer::sm_type("PointsRenderable");

PointsRenderable::PointsRenderable(PointsRenderer* parent, const PointsRendererDesc& desc, bool alpha)
: parent_(parent)
, desc_(desc)
, point_count_(0)
, alpha_(alpha)
{
  supports_geometry_programs_ = getRenderer()->useGeometryShaders();
  needs_offsets_ = !supports_geometry_programs_ && desc.type != rviz_msgs::Points::TYPE_POINTS;
  needs_normals_ = !supports_geometry_programs_ && desc.type == rviz_msgs::Points::TYPE_BOXES;

  if (supports_geometry_programs_)
  {
    mRenderOp.operationType = Ogre::RenderOperation::OT_POINT_LIST;
  }
  else
  {
    switch (desc.type)
    {
    case rviz_msgs::Points::TYPE_POINTS:
      mRenderOp.operationType = Ogre::RenderOperation::OT_POINT_LIST;
      break;
    default:
      mRenderOp.operationType = Ogre::RenderOperation::OT_TRIANGLE_LIST;
    }
  }
  mRenderOp.useIndexes = false;
  mRenderOp.vertexData = new Ogre::VertexData;
  mRenderOp.vertexData->vertexStart = 0;
  mRenderOp.vertexData->vertexCount = 0;

  Ogre::VertexDeclaration *decl = mRenderOp.vertexData->vertexDeclaration;
  size_t offset = 0;

  decl->addElement(0, offset, Ogre::VET_FLOAT3, Ogre::VES_POSITION);
  offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT3);

  if (needs_normals_)
  {
    decl->addElement(0, offset, Ogre::VET_FLOAT3, Ogre::VES_NORMAL, 0);
    offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT3);
  }

  uint32_t tex_coord_num = 0;
  if (needs_offsets_)
  {
    decl->addElement(0, offset, Ogre::VET_FLOAT3, Ogre::VES_TEXTURE_COORDINATES, tex_coord_num++);
    offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT3);
  }

  if (desc.has_normals)
  {
    // We use a texture coordinate set here because boxes need another per-vertex normal
    decl->addElement(0, offset, Ogre::VET_FLOAT3, Ogre::VES_TEXTURE_COORDINATES, tex_coord_num++);
    offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT3);
  }

#if 01
  if (desc.has_orientations)
  {
    decl->addElement(0, offset, Ogre::VET_FLOAT4, Ogre::VES_TEXTURE_COORDINATES, tex_coord_num++);
    offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT4);
  }
#endif

  decl->addElement(0, offset, Ogre::VET_COLOUR, Ogre::VES_DIFFUSE);

  Ogre::HardwareVertexBufferSharedPtr vbuf =
    Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(
      mRenderOp.vertexData->vertexDeclaration->getVertexSize(0),
          POINTS_PER_VBO * getVerticesPerPoint(),
      Ogre::HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY);

  // Bind buffer
  mRenderOp.vertexData->vertexBufferBinding->setBinding(0, vbuf);
}

PointsRenderable::~PointsRenderable()
{
  delete mRenderOp.vertexData;
  delete mRenderOp.indexData;
}

void PointsRenderable::add(const rviz_msgs::Points& points, uint32_t start, uint32_t& out_start, uint32_t& out_count)
{
  ROS_ASSERT(points.positions.size() == points.colors.size());
  ROS_ASSERT(!desc_.has_orientations || (points.positions.size() == points.orientations.size() || points.positions.size() == points.normals.size()));
  ROS_ASSERT(!desc_.has_normals || points.positions.size() == points.normals.size());

  bool orientation_from_normal = desc_.has_orientations && points.orientations.empty() && points.positions.size() == points.normals.size();

  Ogre::Root* root = Ogre::Root::getSingletonPtr();

  uint32_t verts_per_point = getVerticesPerPoint();
  uint32_t point_stride = getPointStride();
  float* vertices = getVertices();
  float* normals = getNormals();
  bool alpha = alpha_;

  Ogre::HardwareVertexBufferSharedPtr vbuf = mRenderOp.vertexData->vertexBufferBinding->getBuffer(0);
  out_start = (mRenderOp.vertexData->vertexStart / verts_per_point) + (mRenderOp.vertexData->vertexCount / verts_per_point);

  uint32_t end = std::min((size_t)POINTS_PER_VBO, out_start + (points.positions.size() - start));
  out_count = 0;

  uint32_t lock_start = out_start * point_stride * verts_per_point;
  uint32_t lock_size = end * point_stride * verts_per_point;
  void* data = vbuf->lock(lock_start, lock_size, Ogre::HardwareVertexBuffer::HBL_NO_OVERWRITE);
  float* fptr = (float*)data;

  rviz_msgs::Points::_positions_type::const_iterator pos_it = points.positions.begin() + start;
  rviz_msgs::Points::_orientations_type::const_iterator orient_it = points.orientations.begin() + start;
  rviz_msgs::Points::_colors_type::const_iterator col_it = points.colors.begin() + start;
  rviz_msgs::Points::_normals_type::const_iterator norm_it = points.normals.begin() + start;
  for (uint32_t i = out_start; i < end; ++i, ++pos_it, ++col_it)
  {
    float a = col_it->a;
    if ((a < 0.99) != alpha)
    {
      continue;
    }

    Ogre::Vector3 pos = fromRobot(Ogre::Vector3(pos_it->x, pos_it->y, pos_it->z));
    mBox.merge(pos);

    float r = col_it->r;
    float g = col_it->g;
    float b = col_it->b;

    uint32_t color = 0;
    root->convertColourValue(Ogre::ColourValue(r, g, b, a), &color);

    ++out_count;

    for (uint32_t j = 0; j < verts_per_point; ++j)
    {
      *fptr++ = pos.x;
      *fptr++ = pos.y;
      *fptr++ = pos.z;

      if (needs_normals_)
      {
        *fptr++ = normals[(j*3)];
        *fptr++ = normals[(j*3) + 1];
        *fptr++ = normals[(j*3) + 2];
      }

      if (needs_offsets_)
      {
        *fptr++ = vertices[(j*3)];
        *fptr++ = vertices[(j*3) + 1];
        *fptr++ = vertices[(j*3) + 2];
      }

      if (desc_.has_normals)
      {
        Ogre::Vector3 norm(normalFromRobot(Ogre::Vector3(norm_it->x, norm_it->y, norm_it->z)));

        *fptr++ = norm.x;
        *fptr++ = norm.y;
        *fptr++ = norm.z;
      }

#if 01
      if (desc_.has_orientations)
      {
        if (orientation_from_normal)
        {
          Ogre::Vector3 norm(normalFromRobot(Ogre::Vector3(norm_it->x, norm_it->y, norm_it->z)));
          Ogre::Quaternion quat(Ogre::Vector3::UNIT_Z.getRotationTo(norm));
          *fptr++ = quat.x;
          *fptr++ = quat.y;
          *fptr++ = quat.z;
          *fptr++ = quat.w;
        }
        else
        {
          *fptr++ = orient_it->x;
          *fptr++ = orient_it->y;
          *fptr++ = orient_it->z;
          *fptr++ = orient_it->w;
        }
      }
#endif

      uint32_t* iptr = (uint32_t*)fptr;
      *iptr = color;
      ++fptr;
    }

    if (desc_.has_normals || orientation_from_normal)
    {
      ++norm_it;
    }

    if (desc_.has_orientations && !orientation_from_normal)
    {
      ++orient_it;
    }
  }

  vbuf->unlock();

  mRenderOp.vertexData->vertexCount += out_count * verts_per_point;
  point_count_ += out_count;
}

void PointsRenderable::remove(uint32_t start, uint32_t count)
{
  ROS_ASSERT_MSG(count <= point_count_, "count = %d, point_count_ = %d", count, point_count_);
  ROS_ASSERT_MSG(start + count < POINTS_PER_VBO, "start = %d, count = %d, POINTS_PER_VBO = %d", start, count, POINTS_PER_VBO);

  uint32_t verts_per_point = getVerticesPerPoint();
  uint32_t point_stride = getPointStride();

  Ogre::HardwareVertexBufferSharedPtr vbuf = mRenderOp.vertexData->vertexBufferBinding->getBuffer(0);
  uint8_t* data = (uint8_t*)vbuf->lock(start * point_stride * verts_per_point, count * point_stride * verts_per_point, Ogre::HardwareVertexBuffer::HBL_NO_OVERWRITE);

  uint32_t total_vertices = verts_per_point * count;
  data = data + (point_stride * verts_per_point * start);
  for (uint32_t i = 0; i < total_vertices; ++i)
  {
    // Assign positions outside the viewable area
    float* fptr = (float*)data;
    *fptr++ = 99999999.0f;
    *fptr++ = 99999999.0f;
    *fptr++ = 99999999.0f;

    data += point_stride;
  }

  point_count_ -= count;

  vbuf->unlock();

  if (isEmpty())
  {
    mBox.setNull();
  }
}

bool PointsRenderable::isEmpty()
{
  return point_count_ == 0;
}

bool PointsRenderable::isFull()
{
  return point_count_ == POINTS_PER_VBO;
}

uint32_t PointsRenderable::getPointStride()
{
  return mRenderOp.vertexData->vertexDeclaration->getVertexSize(0);
}

float* PointsRenderable::getNormals()
{
  if (supports_geometry_programs_)
  {
    return 0;
  }
  else
  {
    if (desc_.type == rviz_msgs::Points::TYPE_POINTS)
    {
      return 0;
    }
    else if (desc_.type == rviz_msgs::Points::TYPE_BILLBOARDS)
    {
      return 0;
    }
    else if (desc_.type == rviz_msgs::Points::TYPE_BILLBOARD_SPHERES)
    {
      return 0;
    }
#if 0
    else if (desc_.type == RM_BILLBOARDS_COMMON_FACING)
    {
      return 0;
    }
#endif
    else if (desc_.type == rviz_msgs::Points::TYPE_BOXES)
    {
      return g_box_normals;
    }
  }

  ROS_ASSERT_MSG(false, "Unknown points type %d", desc_.type);

  return 0;
}

float* PointsRenderable::getVertices()
{
  if (supports_geometry_programs_)
  {
    return g_point_vertices;
  }
  else
  {
    if (desc_.type == rviz_msgs::Points::TYPE_POINTS)
    {
      return g_point_vertices;
    }
    else if (desc_.type == rviz_msgs::Points::TYPE_BILLBOARDS)
    {
      return g_billboard_vertices;
    }
    else if (desc_.type == rviz_msgs::Points::TYPE_BILLBOARD_SPHERES)
    {
      return g_billboard_sphere_vertices;
    }
#if 0
    else if (desc_.type == RM_BILLBOARDS_COMMON_FACING)
    {
      return g_billboard_vertices;
    }
#endif
    else if (desc_.type == rviz_msgs::Points::TYPE_BOXES)
    {
      return g_box_vertices;
    }
  }

  ROS_ASSERT_MSG(false, "Unknown points type %d", desc_.type);

  return 0;
}

uint32_t PointsRenderable::getVerticesPerPoint()
{
  if (supports_geometry_programs_)
  {
    return 1;
  }

  if (desc_.type == rviz_msgs::Points::TYPE_POINTS)
  {
    return 1;
  }

  if (desc_.type == rviz_msgs::Points::TYPE_BILLBOARDS)
  {
    return 6;
  }

#if 0
  if (desc_.type == RM_BILLBOARDS_COMMON_FACING)
    {
      return 6;
    }
#endif

  if (desc_.type == rviz_msgs::Points::TYPE_BILLBOARD_SPHERES)
  {
    return 6;
  }

  if (desc_.type == rviz_msgs::Points::TYPE_BOXES)
  {
    return 36;
  }

  ROS_ASSERT_MSG(false, "Unknown point type %d", desc_.type);

  return 1;
}


//////////////////////////////////////////////////////////////////////////////////////////
// Renderable overrides

void PointsRenderable::_notifyCurrentCamera(Ogre::Camera* camera)
{
  SimpleRenderable::_notifyCurrentCamera( camera );
}

Ogre::Real PointsRenderable::getBoundingRadius(void) const
{
  return Ogre::Math::Sqrt(std::max(mBox.getMaximum().squaredLength(), mBox.getMinimum().squaredLength()));
}

Ogre::Real PointsRenderable::getSquaredViewDepth(const Ogre::Camera* cam) const
{
  Ogre::Vector3 vMin, vMax, vMid, vDist;
  vMin = mBox.getMinimum();
  vMax = mBox.getMaximum();
  vMid = ((vMax - vMin) * 0.5) + vMin;
  vDist = cam->getDerivedPosition() - vMid;

  return vDist.squaredLength();
}

void PointsRenderable::getWorldTransforms(Ogre::Matrix4* xform) const
{
   *xform = m_matWorldTransform * parent_->getParentNode()->_getFullTransform();
}

const Ogre::LightList& PointsRenderable::getLights() const
{
  return parent_->queryLights();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

PointsRenderer::PointsRenderer(Ogre::SceneManager* scene_manager, const PointsRendererDesc& desc)
: desc_(desc)
, scene_manager_(scene_manager)
, scene_node_(0)
{
  std::pair<Ogre::MaterialPtr, Ogre::MaterialPtr> mats = generateMaterialsForPoints(desc);
  opaque_material_ = mats.first;
  alpha_material_ = mats.second;

  scene_node_ = scene_manager_->getRootSceneNode()->createChildSceneNode();
  scene_node_->attachObject(this);

  id_gen_.seed(time(0));

  bounding_radius_ = 0;
  bounding_box_.setNull();
}

PointsRenderer::~PointsRenderer()
{
  scene_manager_->destroySceneNode(scene_node_);
}

uint32_t PointsRenderer::add(const rviz_msgs::Points& points)
{
  PointsInfo pinfo;

  uint32_t total_count = 0;
  uint32_t total_opaque_count = 0;
  uint32_t total_alpha_count = 0;
  uint32_t point_count = points.positions.size();
  while (total_count < point_count)
  {
    // Add opaque points
    PointsRenderablePtr opaque_rend = getOrCreateRenderable(false);

    uint32_t opaque_count = 0;
    uint32_t opaque_start = 0;
    opaque_rend->add(points, total_opaque_count, opaque_start, opaque_count);
    total_opaque_count += opaque_count;

    // Add transparent points
    PointsRenderablePtr alpha_rend = getOrCreateRenderable(true);

    uint32_t alpha_count = 0;
    uint32_t alpha_start = 0;
    alpha_rend->add(points, total_alpha_count, alpha_start, alpha_count);
    total_alpha_count += alpha_count;

    total_count = total_alpha_count + total_opaque_count;

    PointsInfo::RenderableInfo rinfo;
    rinfo.opaque_rend = opaque_rend;
    rinfo.opaque_start = opaque_start;
    rinfo.opaque_count = opaque_count;
    rinfo.alpha_rend = alpha_rend;
    rinfo.alpha_start = alpha_start;
    rinfo.alpha_count = alpha_count;
    pinfo.rends.push_back(rinfo);

    bounding_box_.merge(opaque_rend->getBoundingBox());
    bounding_box_.merge(alpha_rend->getBoundingBox());
  }

  uint32_t id = 0;

  while (id == 0 || points_.find(id) != points_.end())
  {
    id = id_gen_();
  }

  points_.insert(std::make_pair(id, pinfo));

  bounding_radius_ = Ogre::Math::Sqrt(std::max(bounding_box_.getMaximum().squaredLength(), bounding_box_.getMinimum().squaredLength()));

  scene_node_->needUpdate();

  return id;
}

void PointsRenderer::remove(uint32_t id)
{
  M_PointsInfo::iterator it = points_.find(id);
  if (it == points_.end())
  {
    return;
  }

  const PointsInfo& pinfo = it->second;
  for (size_t i = 0; i < pinfo.rends.size(); ++i)
  {
    const PointsInfo::RenderableInfo& rinfo = pinfo.rends[i];
    rinfo.opaque_rend->remove(rinfo.opaque_start, rinfo.opaque_count);
    rinfo.alpha_rend->remove(rinfo.alpha_start, rinfo.alpha_count);
  }

  points_.erase(it);

  shrinkRenderables();

  scene_node_->needUpdate();
}

void PointsRenderer::clear()
{
  points_.clear();
  renderables_.clear();
  bounding_radius_ = 0.0;
  bounding_box_.setNull();

  scene_node_->needUpdate();
}

void PointsRenderer::shrinkRenderables()
{
  uint32_t empty_count = 0;
  V_PointsRenderable::iterator it = renderables_.begin();
  V_PointsRenderable::iterator end = renderables_.end();
  for (; it != end;)
  {
    const PointsRenderablePtr& rend = *it;
    if (rend->isEmpty())
    {
      ++empty_count;
    }
    else
    {
      continue;
    }

    // Keep one empty renderable at any given time
    if (empty_count > 0)
    {
      it = renderables_.erase(it);
    }
    else
    {
      ++it;
    }
  }

  recalculateBounds();

  scene_node_->needUpdate();
}

PointsRenderablePtr PointsRenderer::getOrCreateRenderable(bool alpha)
{
  V_PointsRenderable::iterator it = renderables_.begin();
  V_PointsRenderable::iterator end = renderables_.end();
  for (; it != end; ++it)
  {
    const PointsRenderablePtr& rend = *it;
    if (!rend->isFull() && alpha == rend->isAlpha())
    {
      return rend;
    }
  }

  PointsRenderablePtr rend(new PointsRenderable(this, desc_, alpha));
  rend->setMaterial(alpha ? alpha_material_->getName() : opaque_material_->getName());
  Ogre::Vector4 size(desc_.scale.x, desc_.scale.y, desc_.scale.z, 0.0f);
  rend->setCustomParameter(PointsRendererDesc::CustomParam_Size, size);
  scene_node_->attachObject(rend.get());
  renderables_.push_back(rend);

  return rend;
}

void PointsRenderer::recalculateBounds()
{
  bounding_box_.setNull();
  V_PointsRenderable::iterator it = renderables_.begin();
  V_PointsRenderable::iterator end = renderables_.end();
  for (; it != end; ++it)
  {
    const PointsRenderablePtr& rend = *it;
    bounding_box_.merge(rend->getBoundingBox());
  }

  bounding_radius_ = Ogre::Math::Sqrt(std::max(bounding_box_.getMaximum().squaredLength(), bounding_box_.getMinimum().squaredLength()));
}

////////////////////////////////////////////////////////////////////////////////////////
// MovableObject overrides
const Ogre::AxisAlignedBox& PointsRenderer::getBoundingBox() const
{
  return bounding_box_;
}

float PointsRenderer::getBoundingRadius() const
{
  return bounding_radius_;
}

void PointsRenderer::getWorldTransforms(Ogre::Matrix4* xform) const
{
  *xform = _getParentNodeFullTransform();
}

void PointsRenderer::_notifyCurrentCamera(Ogre::Camera* camera)
{
  MovableObject::_notifyCurrentCamera( camera );
}

void PointsRenderer::_updateRenderQueue(Ogre::RenderQueue* queue)
{
  V_PointsRenderable::iterator it = renderables_.begin();
  V_PointsRenderable::iterator end = renderables_.end();
  for (; it != end; ++it)
  {
    queue->addRenderable((*it).get());
  }
}

void PointsRenderer::_notifyAttached(Ogre::Node *parent, bool isTagPoint)
{
  MovableObject::_notifyAttached(parent, isTagPoint);
}

void PointsRenderer::visitRenderables(Ogre::Renderable::Visitor* visitor, bool debugRenderables)
{
 // TODO
}


} // namespace rviz_renderer_ogre

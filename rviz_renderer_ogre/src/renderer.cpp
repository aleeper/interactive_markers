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

#include "rviz_renderer_ogre/renderer.h"
#include "rviz_renderer_ogre/render_window.h"
#include "rviz_renderer_ogre/scene.h"
#include "rviz_renderer_ogre/camera.h"
#include "rviz_renderer_ogre/disable_rendering_scheme_listener.h"
#include "rviz_renderer_ogre/mesh_loader.h"

#include <OGRE/OgreRoot.h>
#include <OGRE/OgreRenderSystem.h>
#include <OGRE/OgreRenderWindow.h>
#include <OGRE/OgreEntity.h>
#include <OGRE/OgreCamera.h>
#include <OGRE/OgreSceneManager.h>

#include <ros/console.h>
#include <ros/package.h>
#include <ros/callback_queue.h>

using namespace rviz_uuid;

namespace rviz_renderer_ogre
{

Renderer::Renderer(bool enable_ogre_log)
: running_(false)
, first_window_created_(false)
, enable_ogre_log_(enable_ogre_log)
, callback_queue_(new ros::CallbackQueue)
{
}

Renderer::~Renderer()
{
  stop();
}

void Renderer::start()
{
  if (running_)
  {
    return;
  }

  init();

  running_ = true;
  render_thread_ = boost::thread(&Renderer::renderThread, this);
}

void Renderer::stop()
{
  if (!running_)
  {
    return;
  }

  running_ = false;
  render_thread_.join();

  scenes_.clear();
  materials_.clear();
  meshes_.clear();
  delete Ogre::Root::getSingletonPtr();
}

void Renderer::init()
{
  Ogre::LogManager* log_manager = new Ogre::LogManager();
  log_manager->createLog( "Ogre.log", false, false, !enable_ogre_log_ );

  Ogre::Root* root = new Ogre::Root();
  root->loadPlugin( "RenderSystem_GL" );
  root->loadPlugin( "Plugin_OctreeSceneManager" );
  root->loadPlugin( "Plugin_CgProgramManager" );

  // Taken from gazebo
  Ogre::RenderSystem* render_system = NULL;
#if OGRE_VERSION_MAJOR >=1 && OGRE_VERSION_MINOR >= 7
  const Ogre::RenderSystemList& rsList = root->getAvailableRenderers();
  Ogre::RenderSystemList::const_iterator renderIt = rsList.begin();
  Ogre::RenderSystemList::const_iterator renderEnd = rsList.end();
#else
  Ogre::RenderSystemList* rsList = root->getAvailableRenderers();
  Ogre::RenderSystemList::iterator renderIt = rsList->begin();
  Ogre::RenderSystemList::iterator renderEnd = rsList->end();
#endif
  for ( ; renderIt != renderEnd; ++renderIt )
  {
    render_system = *renderIt;

    if ( render_system->getName() == "OpenGL Rendering Subsystem" )
    {
      break;
    }
  }

  if ( render_system == NULL )
  {
    throw std::runtime_error( "Could not find the opengl rendering subsystem!\n" );
  }

  render_system->setConfigOption("FSAA","4");
  render_system->setConfigOption("RTT Preferred Mode", "FBO");

  root->setRenderSystem( render_system );

  root->initialise( false );

  scheme_listener_.reset(new DisableRenderingSchemeListener);
  Ogre::MaterialManager::getSingleton().addListener(scheme_listener_.get(), "GBuffer");
  Ogre::MaterialManager::getSingleton().addListener(scheme_listener_.get(), "GBufferStippleAlpha");
  Ogre::MaterialManager::getSingleton().addListener(scheme_listener_.get(), "AlphaBlend");
  Ogre::MaterialManager::getSingleton().addListener(scheme_listener_.get(), "WeightedAverageAlpha");

  Ogre::LogManager::getSingleton().getDefaultLog()->setDebugOutputEnabled(true);
  Ogre::LogManager::getSingleton().getDefaultLog()->setLogDetail(Ogre::LL_BOREME);
}

void Renderer::oneTimeInit()
{
  Ogre::ResourceGroupManager::getSingleton().createResourceGroup(ROS_PACKAGE_NAME);

  std::string pkg_path = ros::package::getPath(ROS_PACKAGE_NAME);
  Ogre::ResourceGroupManager::getSingleton().addResourceLocation( pkg_path + "/media/textures", "FileSystem", ROS_PACKAGE_NAME );
  Ogre::ResourceGroupManager::getSingleton().addResourceLocation( pkg_path + "/media/fonts", "FileSystem", ROS_PACKAGE_NAME );
  Ogre::ResourceGroupManager::getSingleton().addResourceLocation( pkg_path + "/media/models", "FileSystem", ROS_PACKAGE_NAME );
  Ogre::ResourceGroupManager::getSingleton().addResourceLocation( pkg_path + "/media/materials/programs", "FileSystem", ROS_PACKAGE_NAME );
  Ogre::ResourceGroupManager::getSingleton().addResourceLocation( pkg_path + "/media/materials/scripts", "FileSystem", ROS_PACKAGE_NAME );
  Ogre::ResourceGroupManager::getSingleton().addResourceLocation( pkg_path + "/media/compositors", "FileSystem", ROS_PACKAGE_NAME );
  Ogre::ResourceGroupManager::getSingleton().addResourceLocation( pkg_path + "/media/shaderlib", "FileSystem", ROS_PACKAGE_NAME );
  Ogre::ResourceGroupManager::getSingleton().addResourceLocation( pkg_path + "/media/shaderlib/points", "FileSystem", ROS_PACKAGE_NAME );
  Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

  // Create our 3d stipple pattern for stipple-alpha
  Ogre::DataStreamPtr stream = Ogre::ResourceGroupManager::getSingleton().openResource("3d_stipple.bytes", ROS_PACKAGE_NAME);
  Ogre::Image image;
  image.loadRawData(stream, 4, 4, 5, Ogre::PF_A8);
  Ogre::TextureManager::getSingleton().loadImage("3d_stipple", ROS_PACKAGE_NAME, image, Ogre::TEX_TYPE_3D, 0);
}

RenderWindow* Renderer::createRenderWindow(const rviz_uuid::UUID& id, const std::string& parent_window, uint32_t width, uint32_t height)
{
  if (render_windows_.count(id) > 0)
  {
    std::stringstream ss;
    ss << "Render window with id [" << id << "] already exists";
    throw std::runtime_error(ss.str());
  }

  Ogre::Root* root = Ogre::Root::getSingletonPtr();
  Ogre::NameValuePairList params;
  params["parentWindowHandle"] = parent_window;
  params["FSAA"] = "8";

  std::stringstream id_str;
  id_str << id;
  Ogre::RenderWindow* win = root->createRenderWindow(id_str.str(), width, height, false, &params);

  if (!first_window_created_)
  {
    oneTimeInit();
    first_window_created_ = true;
  }

  win->setActive(true);
  win->setVisible(true);
  win->setAutoUpdated(false);

  RenderWindowPtr ptr(new RenderWindow(id, win, this));
  render_windows_[id] = ptr;

  return ptr.get();
}

void Renderer::destroyRenderWindow(const rviz_uuid::UUID& id)
{
  M_RenderWindow::iterator it = render_windows_.find(id);
  if (it == render_windows_.end())
  {
    std::stringstream ss;
    ss << "Tried to destroy render window [" << id << "] which does not exist";
    throw std::runtime_error(ss.str());
  }

  const RenderWindowPtr& win = it->second;

  Ogre::Root* root = Ogre::Root::getSingletonPtr();
  Ogre::RenderWindow* ogre_win = win->getOgreRenderWindow();
  ogre_win->destroy();
  root->getRenderSystem()->destroyRenderWindow(ogre_win->getName());
  render_windows_.erase(it);
}

RenderWindow* Renderer::getRenderWindow(const rviz_uuid::UUID& id)
{
  M_RenderWindow::iterator it = render_windows_.find(id);
  if (it == render_windows_.end())
  {
    std::stringstream ss;
    ss << "Render window [" << id << "] does not exist";
    throw std::runtime_error(ss.str());
  }

  return it->second.get();
}

Scene* Renderer::createScene(const UUID& id)
{
  if (scenes_.count(id) > 0)
  {
    ROS_WARN_STREAM("UUID " << id << " collided when creating a scene!");
    return 0;
  }

  Ogre::Root* root = Ogre::Root::getSingletonPtr();
  Ogre::SceneManager* scene_manager = root->createSceneManager(Ogre::ST_GENERIC);
  ScenePtr scene(new Scene(id, scene_manager));
  scenes_[id] = scene;

  return scene.get();
}

void Renderer::destroyScene(const UUID& id)
{
  M_Scene::iterator it = scenes_.find(id);
  if (it == scenes_.end())
  {
    std::stringstream ss;
    ss << "Scene " << id << " does not exist!";
    throw std::runtime_error(ss.str());
  }

  const ScenePtr& scene = it->second;
  Ogre::Root* root = Ogre::Root::getSingletonPtr();
  root->destroySceneManager(scene->getSceneManager());
}

Scene* Renderer::getScene(const UUID& id)
{
  M_Scene::iterator it = scenes_.find(id);
  if (it == scenes_.end())
  {
    std::stringstream ss;
    ss << "Scene " << id << " does not exist!";
    throw std::runtime_error(ss.str());
  }

  return it->second.get();
}

Camera* Renderer::getCamera(const UUID& id)
{
  M_Scene::iterator it = scenes_.begin();
  M_Scene::iterator end = scenes_.end();
  for (; it != end; ++it)
  {
    const ScenePtr& scene = it->second;
    Camera* cam = scene->getCamera(id);
    if (cam)
    {
      return cam;
    }
  }

  return 0;
}

void Renderer::addMaterial(const rviz_uuid::UUID& id, const MaterialPtr& mat)
{
  materials_[id] = mat;
}

void Renderer::removeMaterial(const rviz_uuid::UUID& id)
{
  materials_.erase(id);
}

MaterialPtr Renderer::getMaterial(const rviz_uuid::UUID& id)
{
  M_Material::iterator it = materials_.find(id);
  if (it == materials_.end())
  {
    std::stringstream ss;
    ss << "Material [" << id << "] does not exist";
    throw std::runtime_error(ss.str());
  }

  return it->second;
}

void Renderer::addMesh(const std::string& resource_name, const MeshPtr& mesh)
{
  meshes_[resource_name] = mesh;
}

void Renderer::removeMesh(const std::string& resource_name)
{
  meshes_.erase(resource_name);
}

MeshPtr Renderer::getMesh(const std::string& resource_name)
{
  M_Mesh::iterator it = meshes_.find(resource_name);
  if (it == meshes_.end())
  {
    std::stringstream ss;
    ss << "Mesh [" << resource_name << "] does not exist";
    throw std::runtime_error(ss.str());
  }

  return it->second;
}

bool Renderer::meshExists(const std::string& resource_name)
{
  return meshes_.find(resource_name) != meshes_.end();
}

bool Renderer::useGeometryShaders()
{
  return Ogre::Root::getSingleton().getRenderSystem()->getCapabilities()->hasCapability(Ogre::RSC_GEOMETRY_PROGRAM);
  //return false;
}

void Renderer::renderThread()
{
  //init();

  while (running_)
  {
    ros::WallTime start = ros::WallTime::now();

#if 0
    Ogre::Root::getSingleton().renderOneFrame();
#else
    if (Ogre::Root::getSingleton()._fireFrameStarted())
    {

      M_RenderWindow::iterator it = render_windows_.begin();
      M_RenderWindow::iterator end = render_windows_.end();
      for (; it != end; ++it)
      {
        const RenderWindowPtr& wnd = it->second;
        wnd->beginRender();
      }

      callback_queue_->callAvailable();

      it = render_windows_.begin();
      end = render_windows_.end();
      for (; it != end; ++it)
      {
        const RenderWindowPtr& wnd = it->second;
        wnd->finishRender();
      }

      Ogre::Root::getSingleton()._fireFrameEnded();
    }
    else
#endif
    {
      callback_queue_->callAvailable();
    }

    ros::WallTime end = ros::WallTime::now();
    ROS_INFO("Frame took %f", (end - start).toSec());
  }
}

} // namespace rviz_renderer_ogre

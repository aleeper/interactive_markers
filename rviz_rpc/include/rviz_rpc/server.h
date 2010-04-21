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

#ifndef RVIZ_RPC_SERVER_H
#define RVIZ_RPC_SERVER_H

#include "traits.h"
#include <rviz_uuid/uuid.h>

#include <string>
#include <boost/shared_ptr.hpp>

#include <ros/ros.h>
#include <ros/callback_queue.h>

#include <boost/thread.hpp>

namespace rviz_rpc
{

template<typename Req, typename Res>
class Server
{
public:
  typedef boost::shared_ptr<Req> ReqPtr;
  typedef boost::shared_ptr<Req const> ReqConstPtr;
  typedef boost::shared_ptr<Res> ResPtr;
  typedef boost::shared_ptr<Res const> ResConstPtr;
  typedef boost::function<ResPtr(const ReqConstPtr&)> Callback;

  Server(const std::string& name, ros::NodeHandle& nh, const Callback& cb)
  : nh_(nh, name)
  , cb_(cb)
  {
    pub_ = nh_.advertise<Res>("response", 0);
    sub_ = nh_.subscribe("request", 0, &Server::cb, this);
  }

private:

  void cb(const ReqConstPtr& req)
  {
    try
    {
      ResPtr res = cb_(req);
      ROS_ASSERT(res);

      traits::RequestID<Res>::reference(*res) = traits::RequestID<Req>::value(*req);

      pub_.publish(res);
    }
    catch (std::exception& e)
    {
      ResPtr res(new Res);
      traits::ErrorCode<Res>::reference(*res) = error_codes::Exception;
      traits::ErrorString<Res>::reference(*res) = e.what();
      traits::RequestID<Res>::reference(*res) = traits::RequestID<Req>::value(*req);

      pub_.publish(res);
    }
  }

  ros::Subscriber sub_;
  ros::Publisher pub_;
  ros::NodeHandle nh_;
  Callback cb_;
};

} // namespace rviz_rpc

#endif // RVIZ_RPC_SERVER_H

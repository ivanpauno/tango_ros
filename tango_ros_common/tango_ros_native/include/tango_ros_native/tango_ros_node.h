// Copyright 2016 Intermodalics All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef TANGO_ROS_NODE_H_
#define TANGO_ROS_NODE_H_
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <string>

#include <jni.h>

#include <tango_client_api/tango_client_api.h>
#include <tango_support_api/tango_support_api.h>

#include <opencv2/core/core.hpp>

#include <geometry_msgs/TransformStamped.h>
#include <ros/ros.h>
#include <ros/node_handle.h>
#include <sensor_msgs/CompressedImage.h>
#include <sensor_msgs/PointCloud2.h>
#include <tf/transform_broadcaster.h>
#include <tf2_ros/static_transform_broadcaster.h>

#include "tango_ros_native/PublisherConfig.h"

namespace tango_ros_native {
const int NUMBER_OF_FIELDS_IN_POINT_CLOUD = 4;
constexpr char CV_IMAGE_COMPRESSING_FORMAT[] = ".jpg";
constexpr char ROS_IMAGE_COMPRESSING_FORMAT[] = "jpeg";
// Compressing quality for OpenCV to compress an image to JPEG format,
// can take a value from 0 to 100 (the higher is the better).
const int IMAGE_COMPRESSING_QUALITY = 50;

// Camera bitfield values.
const uint32_t CAMERA_NONE = 0;
const uint32_t CAMERA_FISHEYE = (1 << 1);
const uint32_t CAMERA_COLOR = (1 << 2);

struct PublisherConfiguration {
  // True if pose needs to be published.
  bool publish_device_pose = false;
  // True if point cloud needs to be published.
  bool publish_point_cloud = false;
  // Flag corresponding to which cameras need to be published.
  uint32_t publish_camera = CAMERA_NONE;

  // Topic name for the point cloud publisher.
  std::string point_cloud_topic = "tango/point_cloud";
  // Topic name for the fisheye image publisher.
  std::string fisheye_camera_topic = "tango/camera/fisheye/image_raw/compressed";
  // Topic name for the color image publisher.
  std::string color_camera_topic = "tango/camera/color/image_raw/compressed";
};

// Node collecting tango data and publishing it on ros topic.
class TangoRosNode {
 public:
  TangoRosNode(const PublisherConfiguration& publisher_config);
  ~TangoRosNode();
  // Checks the installed version of the TangoCore. If it is too old, then
  // it will not support the most up to date features.
  // @return returns true if tango version if supported.
  bool IsTangoVersionOk(JNIEnv* env, jobject activity);
  // Binds to the tango service.
  // @return returns true if setting the binder ended successfully.
  bool SetBinder(JNIEnv* env, jobject binder);
  // Sets the tango config and connects to the tango service.
  // It also publishes the necessary static transforms (device_T_camera_*).
  // @return returns true if it ended successfully.
  bool OnTangoServiceConnected();
  // Disconnects from the tango service.
  void TangoDisconnect();
  // Start the threads that publish data.
  void StartPublishingThreads();
  // Stop the threads that publish data.
  // Will not return until all the internal threads have exited.
  void StopPublishingThreads();
  // Sets a new PublisherConfiguration and calls PublishStaticTransforms with
  // the new publisher_config.
  void UpdatePublisherConfiguration(const PublisherConfiguration& publisher_config);

  // Function called when a new device pose is available.
  void OnPoseAvailable(const TangoPoseData* pose);
  // Function called when a new point cloud is available.
  void OnPointCloudAvailable(const TangoPointCloud* point_cloud);
  // Function called when a new camera image is available.
  void OnFrameAvailable(TangoCameraId camera_id, const TangoImageBuffer* buffer);

 private:
  // Sets the tango config to be able to collect all necessary data from tango.
  // @return returns TANGO_SUCCESS if the config was set successfully.
  TangoErrorType TangoSetupConfig();
  // Connects to the tango service and to the necessary callbacks.
  // @return returns TANGO_SUCCESS if connecting to tango ended successfully.
  TangoErrorType TangoConnect();
  // Publishes the necessary static transforms (device_T_camera_*).
  void PublishStaticTransforms();
  // Publishes the available data (device pose, point cloud, images).
  void PublishDevicePose();
  void PublishPointCloud();
  void PublishFisheyeImage();
  void PublishColorImage();
  // Thread methods for publishing data.
  void publish_device_pose_thread();
  void publish_pointcloud_thread();
  void publish_fisheye_image_thread();
  void publish_color_image_thread();

  void DynamicReconfigureCallback(PublisherConfig &config, uint32_t level);
  void ros_spin_thread();

  TangoConfig tango_config_;
  ros::NodeHandle node_handle_;

  PublisherConfiguration publisher_config_;
  std::mutex publisher_config_mutex_;
  std::thread ros_spin_thread_;
  std::thread publish_device_pose_thread_;
  std::thread publish_pointcloud_thread_;
  std::thread publish_fisheye_image_thread_;
  std::thread publish_color_image_thread_;
  bool run_threads_ = false;
  std::mutex run_threads_mutex_;

  std::atomic_bool device_pose_lock_;
  std::atomic_bool point_cloud_lock_;
  std::atomic_bool fisheye_image_lock_;
  std::atomic_bool color_image_lock_;

  std::atomic_bool new_pose_available_;
  std::atomic_bool new_point_cloud_available_;
  std::atomic_bool new_fisheye_image_available_;
  std::atomic_bool new_color_image_available_;

  double time_offset_ = 0.; // Offset between tango time and ros time in ms.

  tf::TransformBroadcaster tf_broadcaster_;
  geometry_msgs::TransformStamped start_of_service_T_device_;
  tf2_ros::StaticTransformBroadcaster tf_static_broadcaster_;
  geometry_msgs::TransformStamped device_T_camera_depth_;
  geometry_msgs::TransformStamped device_T_camera_fisheye_;
  geometry_msgs::TransformStamped device_T_camera_color_;

  ros::Publisher point_cloud_publisher_;
  sensor_msgs::PointCloud2 point_cloud_;

  ros::Publisher fisheye_image_publisher_;
  sensor_msgs::CompressedImage fisheye_compressed_image_;
  cv::Mat fisheye_image_;

  ros::Publisher color_image_publisher_;
  sensor_msgs::CompressedImage color_compressed_image_;
  cv::Mat color_image_;
};
}  // namespace tango_ros_native
#endif  // TANGO_ROS_NODE_H_

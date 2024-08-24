#include <pcl/filters/random_sample.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/point_types.h>
#include <pcl_ros/point_cloud.h>
#include <pcl_ros/transforms.h>
#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <string>
#include <vector>

class OutlierRemovalFilter
{
public:
  OutlierRemovalFilter(void);

protected:
  typedef pcl::PointXYZ PointT;
  typedef pcl::PointCloud<PointT> PointCloudT;

  void cloud_callback(const sensor_msgs::PointCloud2ConstPtr &msg);
  void random_sample_filter(PointCloudT::Ptr cloud, const int size);
  void add_dummy_ground_points(PointCloudT::Ptr cloud, const int point_num, const float size);
  void outlier_removal_filter(PointCloudT::Ptr cloud, const int num_neighbors, const float std_dev_mul_thresh);

  std::string target_frame_;
  ros::NodeHandle nh_;
  ros::NodeHandle private_nh_;
  ros::Subscriber point_cloud_sub_;
  ros::Publisher point_cloud_pub_;
  tf::TransformListener tfListener_;
};

OutlierRemovalFilter::OutlierRemovalFilter(void) : private_nh_("~")
{
  private_nh_.param<std::string>("target_frame", target_frame_, {"base_link"});

  point_cloud_sub_ = nh_.subscribe(
      "/cloud", 1, &OutlierRemovalFilter::cloud_callback, this, ros::TransportHints().reliable().tcpNoDelay());
  point_cloud_pub_ = nh_.advertise<sensor_msgs::PointCloud2>("/cloud/filtered", 1);
}

void OutlierRemovalFilter::cloud_callback(const sensor_msgs::PointCloud2ConstPtr &msg)
{
  PointCloudT::Ptr pc_ptr(new PointCloudT);
  pcl::fromROSMsg(*msg, *pc_ptr);

  // transform
  PointCloudT::Ptr pc_transformed_ptr(new PointCloudT);
  pcl_ros::transformPointCloud(target_frame_, *pc_ptr, *pc_transformed_ptr, tfListener_);
  random_sample_filter(pc_transformed_ptr, pc_transformed_ptr->size() / 1000);

  // add dummy data
  add_dummy_ground_points(pc_transformed_ptr, 100, 1);

  // filter
  outlier_removal_filter(pc_transformed_ptr, 50, 1.0);

  // convert pcl::PointCloud to sensor_msgs::PointCloud2
  sensor_msgs::PointCloud2 cloud_msg;
  pcl::toROSMsg(*pc_transformed_ptr, cloud_msg);
  cloud_msg.header.stamp = ros::Time::now();
  cloud_msg.header.frame_id = target_frame_;
  point_cloud_pub_.publish(cloud_msg);
}

void OutlierRemovalFilter::random_sample_filter(PointCloudT::Ptr cloud, const int size)
{
  pcl::RandomSample<PointT> sor;
  sor.setInputCloud(cloud);
  sor.setSample(size);
  sor.filter(*cloud);
}

void OutlierRemovalFilter::add_dummy_ground_points(PointCloudT::Ptr cloud, const int point_num, const float size)
{
  pcl::PointCloud<PointT> dummy_cloud;
  dummy_cloud.points.resize(point_num);
  for (auto &point : dummy_cloud)
  {
    point.x = size * rand() / (RAND_MAX + 1.0f) - size / 2.0;
    point.y = size * rand() / (RAND_MAX + 1.0f) - size / 2.0;
    point.z = 0.0;
  }
  cloud->insert(cloud->end(), dummy_cloud.begin(), dummy_cloud.end());
}

void OutlierRemovalFilter::outlier_removal_filter(
    PointCloudT::Ptr cloud, const int num_neighbors, const float std_dev_mul_thresh)
{
  pcl::StatisticalOutlierRemoval<PointT> sor;
  sor.setInputCloud(cloud);
  sor.setMeanK(num_neighbors);
  sor.setStddevMulThresh(std_dev_mul_thresh);
  sor.filter(*cloud);
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "realsense_outlier_removal_filter");
  OutlierRemovalFilter outlier_removal_filter;
  ros::spin();

  return 0;
}

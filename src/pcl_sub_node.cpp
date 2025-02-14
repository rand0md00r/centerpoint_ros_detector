#include "pointCloudCallback.h"


pointCloudCallbackClass::pointCloudCallbackClass(trtParams& params) : 
        centerpoint(params),
        input_cloud(new pcl::PointCloud<pcl::PointXYZ>),
        range_filtered_cloud(new pcl::PointCloud<pcl::PointXYZ>),
        ground_cloud(new pcl::PointCloud<pcl::PointXYZ>),
        non_ground_cloud(new pcl::PointCloud<pcl::PointXYZ>),
        inliers(new pcl::PointIndices),
        coefficients(new pcl::ModelCoefficients)
{
    // engin init
    params.setParams();
    centerpoint.init(params);

    if(!centerpoint.engineInitlization()) 
    {
        ROS_ERROR("centerpoint build failed");
    }
    else 
    {
        ROS_INFO("Centerpoint build successed");
    }

    has_camera_init = true;
}

void pointCloudCallbackClass::publishRange()
{
    // 发布一个长为max_x_range - min_x_range, 宽为max_y_range - min_y_range, 高为max_z_range - min_z_range的空心立方体, 表示点云的范围
    visualization_msgs::Marker marker;
    marker.header.stamp = ros::Time::now();
    marker.ns = "range";
    marker.id = 0;
    marker.type = visualization_msgs::Marker::CUBE;
    marker.action = visualization_msgs::Marker::ADD;

    marker.header.frame_id = "os_sensor";
    marker.pose.position.x = max_x_range - (max_x_range - min_x_range) / 2;
    marker.pose.position.y = max_y_range - (max_y_range - min_y_range) / 2;
    // marker.pose.position.z = max_z_range - (max_z_range - min_z_range) / 2;
    marker.pose.position.z = min_z_range - 0.1;
    marker.pose.orientation.w = 1.0;
    marker.pose.orientation.x = 0.0;
    marker.pose.orientation.y = 0.0;
    marker.pose.orientation.z = 0.0;
    marker.scale.x = max_x_range - min_x_range;
    marker.scale.y = max_y_range - min_y_range;
    marker.scale.z = 0.1;
    marker.color.r = 1.0;
    marker.color.g = 1.0;
    marker.color.b = 1.0;
    marker.color.a = 0.3;
    
    range_pub.publish(marker);
}

void pointCloudCallbackClass::publishBoxes(const std::vector<Box>& predResult) 
{
    visualization_msgs::MarkerArray markerArray;
    tf::Stamped<tf::Pose> pose_in_os_sensor;
    tf::Stamped<tf::Pose> pose_in_camera_init;

    for (size_t i = 0; i < predResult.size(); ++i) 
    {
        visualization_msgs::Marker marker;
        marker.header.stamp = ros::Time::now();
        marker.ns = "boxes";
        marker.id = i;
        marker.type = visualization_msgs::Marker::CUBE;
        marker.action = visualization_msgs::Marker::ADD;

        pose_in_os_sensor.frame_id_ = "os_sensor";
        pose_in_os_sensor.setOrigin(tf::Vector3(predResult[i].x, predResult[i].y, predResult[i].z));

        if (has_camera_init) {
            try{
                tf_listener.transformPose("camera_init", pose_in_os_sensor, pose_in_camera_init);
            }
            catch (tf::TransformException& ex){
                ROS_ERROR("Transform exception: %s", ex.what());
            }
            
            marker.header.frame_id = "camera_init";
            marker.pose.position.x = pose_in_camera_init.getOrigin().x();
            marker.pose.position.y = pose_in_camera_init.getOrigin().y();
            marker.pose.position.z = pose_in_camera_init.getOrigin().z();
            marker.pose.orientation.w = 1.0;
            marker.pose.orientation.x = 0.0;
            marker.pose.orientation.y = 0.0;
            marker.pose.orientation.z = 0.0;
        } else {
            marker.header.frame_id = "os_sensor";
            marker.pose.position.x = predResult[i].x;
            marker.pose.position.y = predResult[i].y;
            marker.pose.position.z = predResult[i].z;
            marker.pose.orientation.w = cos(predResult[i].theta / 2.0); // cos(theta/2
            marker.pose.orientation.x = 0.0;
            marker.pose.orientation.y = 0.0;
            marker.pose.orientation.z = sin(predResult[i].theta / 2.0); // sin(theta/2)
        }

        marker.scale.x = predResult[i].l;
        marker.scale.y = predResult[i].h;
        marker.scale.z = predResult[i].w;

        marker.color.r = 1.0;
        marker.color.g = predResult[i].cls;
        marker.color.b = predResult[i].score;
        marker.color.a = 0.5;

        marker.lifetime = ros::Duration(0.45);

        bool isInMinRange = predResult[i].x < 0.5 && predResult[i].y < 0.5;     // 0.5m以内的障碍物不显示
        bool isFitSize = predResult[i].w < 2.0 && predResult[i].l < 2.0 && predResult[i].h < 2.0; // 尺寸过大的障碍物不显示

        if(!predResult[i].isDrop && predResult[i].score > 0.1 && !isInMinRange && isFitSize)
        {
            markerArray.markers.push_back(marker);
        }
    }
    marker_pub.publish(markerArray);
    ROS_INFO("Detected %ld Objects.", markerArray.markers.size());
}


void pointCloudCallbackClass::paramServerInit(ros::NodeHandle& nh)
{
    nh.param<double>("max_x_range", max_x_range, 20.0);
    nh.param<double>("min_x_range", min_x_range, -20.0);
    nh.param<double>("max_y_range", max_y_range, 20.0);
    nh.param<double>("min_y_range", min_y_range, -20.0);
    nh.param<double>("max_z_range", max_z_range, 1.0);
    nh.param<double>("min_z_range", min_z_range, -0.7);
    nh.param<double>("ransac_distance_threshold", ransac_distance_threshold, 0.1);
    nh.param<double>("ransac_max_iterations", ransac_max_iterations, 500);

    ROS_INFO("max_x_range: %f", max_x_range);
    ROS_INFO("min_x_range: %f", min_x_range);
    ROS_INFO("max_y_range: %f", max_y_range);
    ROS_INFO("min_y_range: %f", min_y_range);
    ROS_INFO("max_z_range: %f", max_z_range);
    ROS_INFO("min_z_range: %f", min_z_range);
    ROS_INFO("ransac_distance_threshold: %f", ransac_distance_threshold);
    ROS_INFO("ransac_max_iterations: %f", ransac_max_iterations);

    ROS_INFO("Waiting for point cloud . . .");
}


void pointCloudCallbackClass::pointCloudCallback(const sensor_msgs::PointCloud2ConstPtr &input)
{
    // ====================点云滤波====================
    // auto start = std::chrono::system_clock::now();
    pcl::fromROSMsg (*input, *input_cloud);

    // range filter
    range_filter.setInputCloud(input_cloud);
    range_filter.setFilterFieldName("x");
    range_filter.setFilterLimits(min_x_range, max_x_range);
    range_filter.filter(*range_filtered_cloud);

    range_filter.setInputCloud(range_filtered_cloud);
    range_filter.setFilterFieldName("y");
    range_filter.setFilterLimits(min_y_range, max_y_range);
    range_filter.filter(*range_filtered_cloud);

    range_filter.setInputCloud(range_filtered_cloud);
    range_filter.setFilterFieldName("z");
    range_filter.setFilterLimits(min_z_range, max_z_range);
    range_filter.filter(*range_filtered_cloud);

    // removal ground point
    seg.setOptimizeCoefficients(true);
    seg.setModelType(pcl::SACMODEL_PLANE);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setMaxIterations(ransac_max_iterations);
    seg.setDistanceThreshold(ransac_distance_threshold);

    seg.setInputCloud(range_filtered_cloud);
    seg.segment(*inliers, *coefficients);

    // extract ground cloud
    extract.setInputCloud(range_filtered_cloud);
    extract.setIndices(inliers);
    extract.setNegative(false);
    extract.filter(*ground_cloud);

    // extract non ground cloud
    extract.setNegative(true);
    extract.filter(*non_ground_cloud);

    // assign global cloud
    centerpoint.global_cloud = non_ground_cloud;

}

void pointCloudCallbackClass::timerCallback(const ros::TimerEvent& event)
{
    // ====================publish non ground====================
    pcl::toROSMsg(*centerpoint.global_cloud, non_ground_cloud_msg);
    non_grd_pub.publish(non_ground_cloud_msg);

    auto after_filter = std::chrono::system_clock::now();
    // std::chrono::duration<double> filter_time = after_filter - start;
    // ROS_DEBUG("filter time: %f", filter_time.count());
    


    // ====================centerpoint infer====================
    if (!centerpoint.infer()) ROS_ERROR("infer failed! ");

    auto after_infer = std::chrono::system_clock::now();
    std::chrono::duration<double> infer_time = after_infer - after_filter;
    // std::chrono::duration<double> all_time = after_infer - start;
    ROS_DEBUG("infer time: %f", infer_time.count());
    // ROS_DEBUG("all time of a frame: %f", all_time.count());

    
    
    // ====================publish boxes====================
    if(centerpoint.predResult.size() == 0) ROS_WARN("no boxes detected! ");
    publishBoxes(centerpoint.predResult);
    publishRange();
}


int main(int argc, char** argv)
{
    ros::init (argc, argv, "my_pcl_subscriber");

    // Set the logger level and time stamp
    auto start = std::chrono::system_clock::now();
    if(ros::console::set_logger_level(ROSCONSOLE_DEFAULT_NAME, ros::console::levels::Info)) {ros::console::notifyLoggerLevelsChanged();}

    ros::NodeHandle nh;

    trtParams params;
    trtCenterPoint centerpoint(params);

    // 将centerpoint传入pointCloudCallbackClass类中, 以便在pointCloudCallback中使用
    pointCloudCallbackClass pointCloudCallback(params);

    // 从param server中读取参数
    pointCloudCallback.paramServerInit(nh);
    
    // Subscriber and Publisher
    cloud_sub   = nh.subscribe<sensor_msgs::PointCloud2> ("/os_cloud_node/points", 1, &pointCloudCallbackClass::pointCloudCallback, &pointCloudCallback);
    
    non_grd_pub = nh.advertise<sensor_msgs::PointCloud2> ("/non_ground_points", 1);
    marker_pub  = nh.advertise<visualization_msgs::MarkerArray> ("/centerpoint/dets", 1);
    range_pub   = nh.advertise<visualization_msgs::Marker> ("/filtered_points_range", 1);
    // 创建一个定时器，每隔1秒触发一次回调函数
    ros::Timer timer = nh.createTimer(ros::Duration(0.2), &pointCloudCallbackClass::timerCallback, &pointCloudCallback);

    // Start a spinner with 4 threads
    ros::AsyncSpinner spinner(4);
    spinner.start();
    ros::waitForShutdown();
}
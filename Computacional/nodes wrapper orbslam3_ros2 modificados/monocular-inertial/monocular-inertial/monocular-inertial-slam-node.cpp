#include "monocular-inertial-slam-node.hpp"

#include<opencv2/core/core.hpp>

using std::placeholders::_1;

MonocularInertialSlamNode::MonocularInertialSlamNode(ORB_SLAM3::System* pSLAM)
:   Node("ORB_SLAM3_ROS2")
{
    m_SLAM = pSLAM;
    // std::cout << "slam changed" << std::endl;
    m_image_subscriber = this->create_subscription<ImageMsg>(
        "camera",
        10,
        std::bind(&MonocularInertialSlamNode::GrabImage, this, std::placeholders::_1));
    
    imu_subscriber = this->create_subscription<ImuMsg>(
        "imu",
        1000,
        std::bind(&MonocularInertialSlamNode::GrabImu, this, std::placeholders::_1));
    )

    syncThread_ = new std::thread(&StereoInertialNode::SyncWithImu, this);

    std::cout << "slam changed" << std::endl;
}

MonocularInertialSlamNode::~MonocularInertialSlamNode()
{
    //Delete sync thread
    syncThread_->join();
    delete syncThread_;

    // Stop all threads
    m_SLAM->Shutdown();

    // Save camera trajectory
    m_SLAM->SaveKeyFrameTrajectoryTUM("KeyFrameTrajectory.txt");
}

void MonocularInertialSlamNode::GrabImu(const ImuMsg::SharedPtr msg)
{
    //Não entendi muito bem, mas de alguma forma esse Mutex impede que outras threads alterem imuBuff_ ao mesmo tempo
    buffImuMutex_.lock();
    imuBuf_.push(msg);
    bufImuMutex.unlock();
}

void MonocularInertialSlamNode::GrabImage(const ImageMsg::SharedPtr msg)
{
    bufImgMutex_.lock();
    imgBuf_.push(msg);
    bufImgMutex_.lock();
}

void MonocularInertialSlamNode::GetImage(const ImageMsg::SharedPtr msg)
{

    // Copy the ros image message to cv::Mat.
    cv_bridge::CvImageConstPtr m_cvImPtr;

    try
    {
        m_cvImPtr = cv_bridge::toCvShare(msg, sensor_msgs::image_encodings::MONO8);
    }
    catch (cv_bridge::Exception& e)
    {
        RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
        return;
    }

    if (m_cvImPtr->image.type() == 0)
    {
        return m_cvImPtr->image.clone();
    }
    else
    {
        std::cerr << "Error image type" << std::endl;
        return m_cvImPtr->image.clone();
    }
    /* Aqui não é o lugar pra chamar. Tem que sincronizar o Imu com as Imagens antes.
    std::cout<<"one frame has been sent"<<std::endl;
    m_SLAM->TrackMonocular(m_cvImPtr->image, Utility::StampToSec(msg->header.stamp));
    */
}

void MonocularInertialSlamNode::SyncWithImu()
{   
    
    while(1) //Sempre rodando, i guess
    {
        cv::Mat Img;

        double tImg = Utility::StampToSec(imgBuf_.front()->header.stamp);

        bufImgMutex_.lock();
        Img = GetImage(imgBuf_.front());
        imgBuf_.pop();
        bufImgMutex_.unlock();

        vector<ORB_SLAM3::IMU::Point> vImuMeas;
        bufMutex_.lock();
        if (!imuBuf_.empty())
        {
            //Load imu measurements from buffer
            vImuMeas.clear();
            while(!imuBuf_.empty() && Utility::StampToSec(imuBuf_.front()->header.stamp) <= tImg)
            {
                double t = Utility::StampToSec(imuBuf_.front()->header.stamp);
                cv::Point3f acc(imuBuf_.front()->linear_acceleration.x, imuBuf_.front()->linear_acceleration.y, imuBuf_.front()->linear_acceleration.z);
                cv::Point3f gyr(imuBuf_.front()->angular_velocity.x, imuBuf_.front()->angular_velocity.y, imuBuf_.front()->angular_velocity.z);
                vImuMeas.push_back(ORB_SLAM3::IMU::Point(acc, gyr, t));
                imuBuf_.pop();
            }
        }
        bufMutex_.unlock();

        m_SLAM->TrackMonocular(Img, tImg, vImuMeas);

        //TODO: Talvez precise colocar um sleep igual o q tem em stereo-inertial.

    }
}

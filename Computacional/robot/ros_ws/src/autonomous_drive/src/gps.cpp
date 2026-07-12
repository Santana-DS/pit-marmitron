#include <rclcpp/rclcpp.hpp>
#include <gazebo_msgs/msg/link_states.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <random>
#include <chrono>

class GroundTruthPublisher : public rclcpp::Node
{
public:
    GroundTruthPublisher()
    : Node("ground_truth_publisher")
    {
        // Declare parameters
        this->declare_parameter("link_name", "base_link");
        this->declare_parameter("gaussian_noise", 0.01);
        this->declare_parameter("update_rate", 20.0);  // GPS-like rate
        
        link_name_ = this->get_parameter("link_name").as_string();
        noise_std_ = this->get_parameter("gaussian_noise").as_double();
        update_rate_ = this->get_parameter("update_rate").as_double();
        
        // Initialize random number generator
        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        generator_ = std::default_random_engine(seed);
        noise_distribution_ = std::normal_distribution<double>(0.0, noise_std_);
        
        // Publishers
        odom_pub_ = this->create_publisher<nav_msgs::msg::Odometry>(
            "/ground_truth/odom", 10);
        pose_pub_ = this->create_publisher<geometry_msgs::msg::PoseStamped>(
            "/ground_truth/pose", 10);
        
        // Subscriber to Gazebo link states
        link_states_sub_ = this->create_subscription<gazebo_msgs::msg::LinkStates>(
            "/link_states", 10,
            std::bind(&GroundTruthPublisher::linkStatesCallback, this, std::placeholders::_1));
        
        // Timer to publish at specified rate
        auto period = std::chrono::duration<double>(1.0 / update_rate_);
        timer_ = this->create_wall_timer(
            period,
            std::bind(&GroundTruthPublisher::timerCallback, this));
        
        RCLCPP_INFO(this->get_logger(), 
                    "Ground truth publisher started for link: %s at %.1f Hz", 
                    link_name_.c_str(), update_rate_);
    }

private:
    void linkStatesCallback(const gazebo_msgs::msg::LinkStates::SharedPtr msg)
    {
        // Just store the latest pose
        for (size_t i = 0; i < msg->name.size(); ++i)
        {
            if (msg->name[i].find(link_name_) != std::string::npos)
            {
                latest_pose_ = msg->pose[i];
                has_pose_ = true;
                break;
            }
        }
    }
    
    void timerCallback()
    {
        if (!has_pose_) return;
        
        auto pose_msg = geometry_msgs::msg::PoseStamped();
        pose_msg.header.stamp = this->now();
        pose_msg.header.frame_id = "world";
        pose_msg.pose = latest_pose_;
        
        // Add noise if configured
        if (noise_std_ > 0.0) {
            pose_msg.pose.position.x += noise_distribution_(generator_);
            pose_msg.pose.position.y += noise_distribution_(generator_);
            pose_msg.pose.position.z += noise_distribution_(generator_);
        }
        
        pose_pub_->publish(pose_msg);
        
        auto odom_msg = nav_msgs::msg::Odometry();
        odom_msg.header = pose_msg.header;
        odom_msg.pose.pose = pose_msg.pose;
        odom_msg.child_frame_id = link_name_;
        odom_pub_->publish(odom_msg);
    }
    
    // Publishers and subscribers
    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr pose_pub_;
    rclcpp::Subscription<gazebo_msgs::msg::LinkStates>::SharedPtr link_states_sub_;
    rclcpp::TimerBase::SharedPtr timer_;
    
    // Latest pose storage
    geometry_msgs::msg::Pose latest_pose_;
    bool has_pose_ = false;
    
    // Parameters
    std::string link_name_;
    double noise_std_;
    double update_rate_;
    
    // Random number generation
    std::default_random_engine generator_;
    std::normal_distribution<double> noise_distribution_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<GroundTruthPublisher>());
    rclcpp::shutdown();
    return 0;
}
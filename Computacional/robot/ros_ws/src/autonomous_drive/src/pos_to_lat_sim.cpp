#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "sensor_msgs/msg/nav_sat_fix.hpp"
#include <cmath>
#include <random>

class FakeGps : public rclcpp::Node
{
public:
    FakeGps() : Node("gps_sim")
    {
        // Declare parameters
        this->declare_parameter("datum_lat", -15.793889);
        this->declare_parameter("datum_lon", -47.882778);
        this->declare_parameter("update_rate", 1.0);
        
        datum_lat_ = this->get_parameter("datum_lat").as_double();
        datum_lon_ = this->get_parameter("datum_lon").as_double();
        double rate = this->get_parameter("update_rate").as_double();
        
        // Subscribe to ground truth pose
        sub_ = this->create_subscription<geometry_msgs::msg::PoseStamped>(
            "/ground_truth/pose", 10,
            std::bind(&FakeGps::callback, this, std::placeholders::_1));

        // Publish GPS fix
        pub_ = this->create_publisher<sensor_msgs::msg::NavSatFix>("/gps/fix", 10);
        
        // Timer for GPS updates using the rate parameter
        int period_ms = static_cast<int>(1000.0 / rate);
        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(period_ms),
            std::bind(&FakeGps::timer_callback, this));
        
        // Random noise generators for realistic GPS
        std::random_device rd;
        gen_ = std::mt19937(rd());
        noise_x_ = std::normal_distribution<>(0.0, 1.9947);  // erro de aproximadamente 2.5 m
        noise_y_ = std::normal_distribution<>(0.0, 1.9947);  // erro de aproximadamente 2.5 m
        noise_z_ = std::normal_distribution<>(0.0, 3.0);  // 3.0m std in z (worse)
        
        RCLCPP_INFO(this->get_logger(), 
                    "Realistic GPS simulator started at %.1f Hz with datum: %.6f, %.6f", 
                    rate, datum_lat_, datum_lon_);
    }

private:
    void callback(const geometry_msgs::msg::PoseStamped::SharedPtr msg)
    {
        // Store the latest pose for timer-based publishing
        latest_pose_ = *msg;
        has_pose_ = true;
    }
    
    void timer_callback()
    {
        if (!has_pose_) {
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                                "Waiting for ground truth pose data...");
            return;
        }
        
        // Add realistic GPS noise to position
        double x = latest_pose_.pose.position.x + noise_x_(gen_);
        double y = latest_pose_.pose.position.y + noise_y_(gen_);
        double z = latest_pose_.pose.position.z + noise_z_(gen_);

        // Convert meters to lat/lon using flat earth approximation
        const double EARTH_RADIUS = 6378137.0;  // WGS84 equatorial radius
        double dlat = (y / EARTH_RADIUS) * (180.0 / M_PI);
        double dlon = (x / (EARTH_RADIUS * std::cos(M_PI * datum_lat_ / 180.0))) * (180.0 / M_PI);

        // Create NavSatFix message
        sensor_msgs::msg::NavSatFix fix;
        fix.header.stamp = this->now();
        fix.header.frame_id = "gps_link";
        fix.latitude = datum_lat_ + dlat;
        fix.longitude = datum_lon_ + dlon;
        fix.altitude = z;
        
        // GPS status
        fix.status.status = sensor_msgs::msg::NavSatStatus::STATUS_FIX;
        fix.status.service = sensor_msgs::msg::NavSatStatus::SERVICE_GPS;

        // Realistic GPS covariance matrix
        // Diagonal: variance in m^2 (std_dev^2)
        fix.position_covariance = {
            2.25, 0.0,  0.0,    // (1.5m)^2 = 2.25 in lat
            0.0,  2.25, 0.0,    // (1.5m)^2 = 2.25 in lon
            0.0,  0.0,  9.0     // (3.0m)^2 = 9.0 in altitude
        };
        fix.position_covariance_type = sensor_msgs::msg::NavSatFix::COVARIANCE_TYPE_DIAGONAL_KNOWN;

        pub_->publish(fix);
    }

    // ROS2 interfaces
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr sub_;
    rclcpp::Publisher<sensor_msgs::msg::NavSatFix>::SharedPtr pub_;
    rclcpp::TimerBase::SharedPtr timer_;
    
    // Latest pose storage
    geometry_msgs::msg::PoseStamped latest_pose_;
    bool has_pose_ = false;
    
    // GPS parameters
    double datum_lat_;
    double datum_lon_;
    
    // Random noise generators
    std::mt19937 gen_;
    std::normal_distribution<> noise_x_;
    std::normal_distribution<> noise_y_;
    std::normal_distribution<> noise_z_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<FakeGps>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
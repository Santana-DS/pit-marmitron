#include <cmath>
#include <memory>
#include "rclcpp/rclcpp.hpp"

#include "std_msgs/msg/bool.hpp"
#include "geometry_msgs/msg/twist.hpp"

//Nó simples que se inscreve em 3 entradas, e dependendo se a primeira (Override) é "True" 
//ele publica somenta o que recebe de /cmd_vel_manual 
//caso "False" ele passa a velocidade moothed para frente

class Mux : public rclcpp::Node
{
    public: Mux():Node("Mux_Vel")
    {
        Sub_Override_ = this->create_subscription<std_msgs::msg::Bool>(
            "Overryde", 10,
            [this](const std_msgs::msg::Bool::SharedPtr msg) {
            override_ = msg->data;
        });

        Sub_vel_nav_ = this->create_subscription<geometry_msgs::msg::Twist>(
            "/cmd_vel", 10,
            [this](const geometry_msgs::msg::Twist::SharedPtr msg) {
            if (!override_) {
            Redirect(msg);
            }
        });

        Sub_vel_manual = this->create_subscription<geometry_msgs::msg::Twist>(
            "/cmd_vel_manual", 10,
            [this](const geometry_msgs::msg::Twist::SharedPtr msg) {
                if (override_) {
                Redirect(msg);
                }
            });

        Vel_output_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel_end",10);
    }

    private:
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr Sub_Override_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr Sub_vel_nav_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr Sub_vel_manual;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr Vel_output_;
    bool override_ = false;
    
    void Redirect(const geometry_msgs::msg::Twist::SharedPtr msg) {
        Vel_output_->publish(*msg);
    }
};

int main(int argc, char *argv[]){
    rclcpp::init(argc,argv);
    rclcpp::spin(std::make_shared<Mux>());
    rclcpp::shutdown();
    return 0;
}

#include <cmath>
#include <memory>
#include "rclcpp/rclcpp.hpp"

#include "geometry_msgs/msg/twist.hpp"
#include "std_msgs/msg/float32.hpp"

class Vel_Calc : public rclcpp::Node
{
    public: Vel_Calc():Node("Calc_Vel_Rodas")
    {
        //Seta a inscrição e publição do nó, lembrar de mudar quando necessário, ele deve conectar após o mux
        subscription_ = this->create_subscription<geometry_msgs::msg::Twist>(
            "/cmd_vel_end", 10,
            [this](const geometry_msgs::msg::Twist::SharedPtr msg) {
                this->calculo(msg);
            }
        ); 

        vel_direita_ = this->create_publisher<std_msgs::msg::Float32>("vel_roda_dir",10);
        vel_esquerda_ = this->create_publisher<std_msgs::msg::Float32>("vel_roda_esquerda",10);
    }

    private:

    void calculo(const geometry_msgs::msg::Twist::SharedPtr msg)
{
    const double R = 0.215;// Distancia do centro do robo para as rodas em metros (Ajustar com os valores finais)
    const double r = 0.05; //raio da roda (Ajustar)
    const double PI = 3.141593; //Pi

    double linear_x = msg->linear.x;
    double angular = msg->angular.z;

    // Caso esteja em linha reta ambas rodas devem girar na mesma velocidade linear
    // velocidade angular: visto que o rviz2 trata o sentido antihorario como positivo
    // Então se a velocidade angular é positiva (gira sentido antihorario) deve-se aumentar a velocidade da roda direita e diminuir a da esquerda
    double vel_dir = linear_x + (angular * R / 2.0);
    double vel_esq = linear_x - (angular * R / 2.0);

    double rpm_dir = (vel_dir / (2.0 * PI * r)) * 60.0;
    double rpm_esq = (vel_esq / (2.0 * PI * r)) * 60.0;


    //Transformando os valores para Float32 que o publisher aceita
    std_msgs::msg::Float32 dir_msg; //cria a msg
    dir_msg.data = static_cast<float>(rpm_dir); //atribui o valor ao local certo
    std_msgs::msg::Float32 esq_msg;
    esq_msg.data = static_cast<float>(rpm_esq);

    this->vel_direita_->publish(dir_msg);
    this->vel_esquerda_->publish(esq_msg);
}
    //Declarações
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr subscription_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr vel_direita_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr vel_esquerda_;

};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<Vel_Calc>());
    rclcpp::shutdown();
    return 0;
}
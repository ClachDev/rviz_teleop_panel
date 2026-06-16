#pragma once

#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rviz_common/panel.hpp>

namespace rviz_teleop_panel
{

class TeleopPanel : public rviz_common::Panel
{
  Q_OBJECT

public:
  explicit TeleopPanel(QWidget * parent = nullptr);
  ~TeleopPanel() override;

  void onInitialize() override;
  void load(const rviz_common::Config & config) override;
  void save(rviz_common::Config config) const override;

protected:
  void keyPressEvent(QKeyEvent * event) override;
  void keyReleaseEvent(QKeyEvent * event) override;

private Q_SLOTS:
  void onTopicChanged();
  void onForwardPressed();
  void onBackwardPressed();
  void onLeftPressed();
  void onRightPressed();
  void onStopPressed();
  void onDirectionalReleased();
  void publishVelocity();

private:
  void startDriving(double linear, double angular);
  void stopDriving();
  void recreatePublisher();

  QLineEdit * topic_edit_;
  QDoubleSpinBox * linear_speed_box_;
  QDoubleSpinBox * angular_speed_box_;
  QPushButton * btn_forward_;
  QPushButton * btn_backward_;
  QPushButton * btn_left_;
  QPushButton * btn_right_;
  QPushButton * btn_stop_;
  QLabel * status_label_;

  QTimer * publish_timer_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;

  double current_linear_{0.0};
  double current_angular_{0.0};
  QString topic_name_{"/cmd_vel"};
};

}  // namespace rviz_teleop_panel

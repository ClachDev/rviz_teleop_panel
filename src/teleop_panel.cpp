#include "rviz_teleop_panel/teleop_panel.hpp"

#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <rviz_common/config.hpp>
#include <rviz_common/display_context.hpp>
#include <rviz_common/ros_integration/ros_node_abstraction_iface.hpp>

namespace rviz_teleop_panel
{

TeleopPanel::TeleopPanel(QWidget * parent)
: rviz_common::Panel(parent)
{
  setFocusPolicy(Qt::StrongFocus);

  // ── Topic row ────────────────────────────────────────────────────────────
  topic_edit_ = new QLineEdit(topic_name_);
  auto * topic_layout = new QHBoxLayout;
  topic_layout->addWidget(new QLabel("Topic:"));
  topic_layout->addWidget(topic_edit_);

  // ── Speed row ────────────────────────────────────────────────────────────
  linear_speed_box_ = new QDoubleSpinBox;
  linear_speed_box_->setRange(0.01, 5.0);
  linear_speed_box_->setValue(0.3);
  linear_speed_box_->setSingleStep(0.05);
  linear_speed_box_->setSuffix(" m/s");

  angular_speed_box_ = new QDoubleSpinBox;
  angular_speed_box_->setRange(0.01, 6.28);
  angular_speed_box_->setValue(0.5);
  angular_speed_box_->setSingleStep(0.05);
  angular_speed_box_->setSuffix(" rad/s");

  auto * speed_layout = new QHBoxLayout;
  speed_layout->addWidget(new QLabel("Linear:"));
  speed_layout->addWidget(linear_speed_box_);
  speed_layout->addWidget(new QLabel("Angular:"));
  speed_layout->addWidget(angular_speed_box_);

  // ── D-pad buttons ────────────────────────────────────────────────────────
  //        [↑]
  //  [←]  [■]  [→]
  //        [↓]
  QFont btn_font;
  btn_font.setPointSize(18);

  auto make_btn = [&](const QString & label) {
    auto * b = new QPushButton(label);
    b->setFont(btn_font);
    b->setFixedSize(60, 60);
    b->setFocusPolicy(Qt::NoFocus);  // keep keyboard focus on the panel
    return b;
  };

  btn_forward_  = make_btn("↑");  // ↑
  btn_backward_ = make_btn("↓");  // ↓
  btn_left_     = make_btn("←");  // ←
  btn_right_    = make_btn("→");  // →
  btn_stop_     = make_btn("■");  // ■

  auto * dpad = new QGridLayout;
  dpad->addWidget(btn_forward_,  0, 1);
  dpad->addWidget(btn_left_,     1, 0);
  dpad->addWidget(btn_stop_,     1, 1);
  dpad->addWidget(btn_right_,    1, 2);
  dpad->addWidget(btn_backward_, 2, 1);

  auto * dpad_row = new QHBoxLayout;
  dpad_row->addStretch();
  dpad_row->addLayout(dpad);
  dpad_row->addStretch();

  // ── Status label ─────────────────────────────────────────────────────────
  status_label_ = new QLabel("Click panel or arrow keys to drive");
  status_label_->setAlignment(Qt::AlignCenter);

  // ── Main layout ───────────────────────────────────────────────────────────
  auto * layout = new QVBoxLayout;
  layout->addLayout(topic_layout);
  layout->addLayout(speed_layout);
  layout->addLayout(dpad_row);
  layout->addWidget(status_label_);
  setLayout(layout);

  // ── Publish timer at 10 Hz ───────────────────────────────────────────────
  publish_timer_ = new QTimer(this);
  publish_timer_->setInterval(100);
  connect(publish_timer_, &QTimer::timeout, this, &TeleopPanel::publishVelocity);

  // ── Button signals ───────────────────────────────────────────────────────
  connect(btn_forward_,  &QPushButton::pressed,  this, &TeleopPanel::onForwardPressed);
  connect(btn_backward_, &QPushButton::pressed,  this, &TeleopPanel::onBackwardPressed);
  connect(btn_left_,     &QPushButton::pressed,  this, &TeleopPanel::onLeftPressed);
  connect(btn_right_,    &QPushButton::pressed,  this, &TeleopPanel::onRightPressed);
  connect(btn_stop_,     &QPushButton::clicked,  this, &TeleopPanel::onStopPressed);

  connect(btn_forward_,  &QPushButton::released, this, &TeleopPanel::onDirectionalReleased);
  connect(btn_backward_, &QPushButton::released, this, &TeleopPanel::onDirectionalReleased);
  connect(btn_left_,     &QPushButton::released, this, &TeleopPanel::onDirectionalReleased);
  connect(btn_right_,    &QPushButton::released, this, &TeleopPanel::onDirectionalReleased);

  connect(topic_edit_, &QLineEdit::editingFinished, this, &TeleopPanel::onTopicChanged);
}

TeleopPanel::~TeleopPanel() = default;

void TeleopPanel::onInitialize()
{
  recreatePublisher();
}

void TeleopPanel::load(const rviz_common::Config & config)
{
  rviz_common::Panel::load(config);

  QString topic;
  if (config.mapGetString("Topic", &topic)) {
    topic_name_ = topic;
    topic_edit_->setText(topic);
  }
  float val;
  if (config.mapGetFloat("LinearSpeed", &val)) {
    linear_speed_box_->setValue(val);
  }
  if (config.mapGetFloat("AngularSpeed", &val)) {
    angular_speed_box_->setValue(val);
  }
}

void TeleopPanel::save(rviz_common::Config config) const
{
  rviz_common::Panel::save(config);
  config.mapSetValue("Topic", topic_name_);
  config.mapSetValue("LinearSpeed", static_cast<float>(linear_speed_box_->value()));
  config.mapSetValue("AngularSpeed", static_cast<float>(angular_speed_box_->value()));
}

// ── Keyboard control ──────────────────────────────────────────────────────────

void TeleopPanel::keyPressEvent(QKeyEvent * event)
{
  if (event->isAutoRepeat()) {
    return;
  }
  switch (event->key()) {
    case Qt::Key_Up:    onForwardPressed();  break;
    case Qt::Key_Down:  onBackwardPressed(); break;
    case Qt::Key_Left:  onLeftPressed();     break;
    case Qt::Key_Right: onRightPressed();    break;
    case Qt::Key_Space: onStopPressed();     break;
    default: rviz_common::Panel::keyPressEvent(event);
  }
}

void TeleopPanel::keyReleaseEvent(QKeyEvent * event)
{
  if (event->isAutoRepeat()) {
    return;
  }
  switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right:
      onDirectionalReleased();
      break;
    default: rviz_common::Panel::keyReleaseEvent(event);
  }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void TeleopPanel::onTopicChanged()
{
  topic_name_ = topic_edit_->text();
  recreatePublisher();
}

void TeleopPanel::onForwardPressed()
{
  startDriving(linear_speed_box_->value(), 0.0);
}

void TeleopPanel::onBackwardPressed()
{
  startDriving(-linear_speed_box_->value(), 0.0);
}

void TeleopPanel::onLeftPressed()
{
  startDriving(0.0, angular_speed_box_->value());
}

void TeleopPanel::onRightPressed()
{
  startDriving(0.0, -angular_speed_box_->value());
}

void TeleopPanel::onStopPressed()
{
  stopDriving();
}

void TeleopPanel::onDirectionalReleased()
{
  stopDriving();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void TeleopPanel::startDriving(double linear, double angular)
{
  current_linear_  = linear;
  current_angular_ = angular;
  publish_timer_->start();
  publishVelocity();

  status_label_->setText(
    QString("linear: %1  angular: %2")
      .arg(current_linear_,  0, 'f', 2)
      .arg(current_angular_, 0, 'f', 2));
}

void TeleopPanel::stopDriving()
{
  publish_timer_->stop();
  current_linear_  = 0.0;
  current_angular_ = 0.0;
  publishVelocity();
  status_label_->setText("Stopped");
}

void TeleopPanel::publishVelocity()
{
  if (!publisher_) {
    return;
  }
  geometry_msgs::msg::Twist msg;
  msg.linear.x  = current_linear_;
  msg.angular.z = current_angular_;
  publisher_->publish(msg);
}

void TeleopPanel::recreatePublisher()
{
  if (!getDisplayContext()) {
    return;
  }
  auto node = getDisplayContext()->getRosNodeAbstraction().lock()->get_raw_node();
  publisher_ = node->create_publisher<geometry_msgs::msg::Twist>(
    topic_name_.toStdString(), rclcpp::QoS(1));
}

}  // namespace rviz_teleop_panel

#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(rviz_teleop_panel::TeleopPanel, rviz_common::Panel)

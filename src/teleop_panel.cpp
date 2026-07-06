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
  stamped_check_ = new QCheckBox("Stamped");
  stamped_check_->setChecked(stamped_);  // default on (TwistStamped)
  stamped_check_->setFocusPolicy(Qt::NoFocus);  // keep keyboard focus on the panel
  stamped_check_->setToolTip(
    "Publish geometry_msgs/TwistStamped (required by ros2_control diff_drive_controller "
    "on Jazzy). Uncheck for plain geometry_msgs/Twist.");
  auto * topic_layout = new QHBoxLayout;
  topic_layout->addWidget(new QLabel("Topic:"));
  topic_layout->addWidget(topic_edit_);
  topic_layout->addWidget(stamped_check_);

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
  // Each directional button holds its direction while pressed and releases it
  // on release, so multiple held directions combine (see updateTwist).
  auto bind_dir = [this](QPushButton * b, Direction dir) {
    connect(b, &QPushButton::pressed,  this, [this, dir] { setHeld(dir, true); });
    connect(b, &QPushButton::released, this, [this, dir] { setHeld(dir, false); });
  };
  bind_dir(btn_forward_,  Forward);
  bind_dir(btn_backward_, Backward);
  bind_dir(btn_left_,     Left);
  bind_dir(btn_right_,    Right);
  connect(btn_stop_, &QPushButton::clicked, this, &TeleopPanel::onStopPressed);

  connect(topic_edit_, &QLineEdit::editingFinished, this, &TeleopPanel::onTopicChanged);
  connect(stamped_check_, &QCheckBox::toggled, this, &TeleopPanel::onStampedToggled);
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
  bool stamped;
  if (config.mapGetBool("Stamped", &stamped)) {
    stamped_ = stamped;
    stamped_check_->setChecked(stamped);
  }

  // Rebuild the publisher so it matches the loaded topic/type. Safe if node_ is
  // not ready yet: recreatePublisher() early-returns and onInitialize() builds it.
  recreatePublisher();
}

void TeleopPanel::save(rviz_common::Config config) const
{
  rviz_common::Panel::save(config);
  config.mapSetValue("Topic", topic_name_);
  config.mapSetValue("LinearSpeed", static_cast<float>(linear_speed_box_->value()));
  config.mapSetValue("AngularSpeed", static_cast<float>(angular_speed_box_->value()));
  config.mapSetValue("Stamped", stamped_check_->isChecked());
}

// ── Keyboard control ──────────────────────────────────────────────────────────

void TeleopPanel::keyPressEvent(QKeyEvent * event)
{
  if (event->isAutoRepeat()) {
    return;
  }
  switch (event->key()) {
    case Qt::Key_Up:    setHeld(Forward,  true); break;
    case Qt::Key_Down:  setHeld(Backward, true); break;
    case Qt::Key_Left:  setHeld(Left,     true); break;
    case Qt::Key_Right: setHeld(Right,    true); break;
    case Qt::Key_Space: onStopPressed();         break;
    default: rviz_common::Panel::keyPressEvent(event);
  }
}

void TeleopPanel::keyReleaseEvent(QKeyEvent * event)
{
  if (event->isAutoRepeat()) {
    return;
  }
  switch (event->key()) {
    case Qt::Key_Up:    setHeld(Forward,  false); break;
    case Qt::Key_Down:  setHeld(Backward, false); break;
    case Qt::Key_Left:  setHeld(Left,     false); break;
    case Qt::Key_Right: setHeld(Right,    false); break;
    default: rviz_common::Panel::keyReleaseEvent(event);
  }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void TeleopPanel::onTopicChanged()
{
  topic_name_ = topic_edit_->text();
  recreatePublisher();
}

void TeleopPanel::onStampedToggled(bool checked)
{
  stamped_ = checked;
  recreatePublisher();  // message type changed, so swap the publisher
}

void TeleopPanel::onStopPressed()
{
  // Emergency stop: drop every held direction.
  held_[Forward] = held_[Backward] = held_[Left] = held_[Right] = false;
  updateTwist();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void TeleopPanel::setHeld(Direction dir, bool held)
{
  held_[dir] = held;
  updateTwist();
}

void TeleopPanel::updateTwist()
{
  const double lin = linear_speed_box_->value();
  const double ang = angular_speed_box_->value();

  // Opposite directions cancel; perpendicular directions combine.
  current_linear_  = (held_[Forward] ? lin : 0.0) - (held_[Backward] ? lin : 0.0);
  current_angular_ = (held_[Left]    ? ang : 0.0) - (held_[Right]    ? ang : 0.0);

  const bool any_held =
    held_[Forward] || held_[Backward] || held_[Left] || held_[Right];

  // Keep streaming at 10 Hz while anything is held; stop when all released.
  if (any_held && !publish_timer_->isActive()) {
    publish_timer_->start();
  } else if (!any_held) {
    publish_timer_->stop();
  }

  publishVelocity();  // push the new command (or a final zero) right away

  if (any_held) {
    status_label_->setText(
      QString("linear: %1  angular: %2")
        .arg(current_linear_,  0, 'f', 2)
        .arg(current_angular_, 0, 'f', 2));
  } else {
    status_label_->setText("Stopped");
  }
}

void TeleopPanel::publishVelocity()
{
  if (stamped_) {
    if (!stamped_publisher_) {
      return;
    }
    geometry_msgs::msg::TwistStamped msg;
    msg.header.stamp     = node_->now();
    msg.twist.linear.x   = current_linear_;
    msg.twist.angular.z  = current_angular_;
    stamped_publisher_->publish(msg);
  } else {
    if (!publisher_) {
      return;
    }
    geometry_msgs::msg::Twist msg;
    msg.linear.x  = current_linear_;
    msg.angular.z = current_angular_;
    publisher_->publish(msg);
  }
}

void TeleopPanel::recreatePublisher()
{
  if (!node_) {
    if (!getDisplayContext()) {
      return;
    }
    node_ = getDisplayContext()->getRosNodeAbstraction().lock()->get_raw_node();
  }

  // Only one publisher is live at a time; drop the other so we don't leave a
  // stale endpoint of the wrong type on the topic.
  publisher_.reset();
  stamped_publisher_.reset();

  const std::string topic = topic_name_.toStdString();
  if (stamped_) {
    stamped_publisher_ = node_->create_publisher<geometry_msgs::msg::TwistStamped>(
      topic, rclcpp::QoS(1));
  } else {
    publisher_ = node_->create_publisher<geometry_msgs::msg::Twist>(
      topic, rclcpp::QoS(1));
  }
}

}  // namespace rviz_teleop_panel

#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(rviz_teleop_panel::TeleopPanel, rviz_common::Panel)

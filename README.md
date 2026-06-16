# rviz_teleop_panel

An RViz2 panel plugin that gives you an on-screen D-pad for driving a robot via
`geometry_msgs/Twist` messages on a `cmd_vel` topic. It fills the gap left by the
lack of a maintained ROS 2 port of the old arrow-button teleop panel.

Built for the [mote](../mote) robot, but it works with any ROS 2 robot that
subscribes to a `Twist` `cmd_vel` topic.

## Features

- **D-pad** — hold ↑ ↓ ← → to drive, ■ to stop. Commands publish at 10 Hz while
  held; releasing sends a zero `Twist`.
- **Keyboard control** — arrow keys drive and Space stops while the panel has
  focus.
- **Configurable** — set the topic name and linear/angular speeds from the
  panel. Settings persist in the `.rviz` config.

## Build

This is a standard `ament_cmake` package. Drop it into a colcon workspace's
`src/` and build it. For the `mote` setup it builds inside the pixi environment
(ROS 2 Jazzy via RoboStack):

```sh
# from your colcon workspace, with rviz_teleop_panel under src/
pixi run colcon build --packages-select rviz_teleop_panel
```

Or with a sourced ROS 2 environment:

```sh
colcon build --packages-select rviz_teleop_panel
```

## Usage

1. Source the workspace install (`source install/setup.bash`, or it's handled by
   the pixi activation script).
2. Launch RViz2.
3. **Panels → Add New Panel → rviz_teleop_panel → TeleopPanel**.
4. Set the topic (default `cmd_vel`) and speeds, then drive with the buttons or
   the arrow keys.

## License

Apache-2.0. See [LICENSE](LICENSE).

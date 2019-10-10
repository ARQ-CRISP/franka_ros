// Copyright (c) 2019 Franka Emika GmbH
// Use of this source code is governed by the Apache-2.0 license, see LICENSE
#pragma once

#include <array>
#include <atomic>
#include <exception>
#include <functional>
#include <string>
#include <thread>

#include <actionlib/server/simple_action_server.h>
#include <franka/control_types.h>
#include <franka/duration.h>
#include <franka/exception.h>
#include <franka/model.h>
#include <franka/robot.h>
#include <franka/robot_state.h>
#include <hardware_interface/joint_command_interface.h>
#include <hardware_interface/joint_state_interface.h>
#include <hardware_interface/robot_hw.h>
#include <joint_limits_interface/joint_limits_interface.h>
#include <ros/node_handle.h>
#include <ros/time.h>
#include <urdf/model.h>

#include <franka_control/ErrorRecoveryAction.h>
#include <franka_control/services.h>
#include <franka_hw/control_mode.h>
#include <franka_hw/franka_cartesian_command_interface.h>
#include <franka_hw/franka_model_interface.h>
#include <franka_hw/franka_state_interface.h>

using franka_control::ServiceContainer;
using franka_hw::ControlMode;

namespace franka_combinable_hw {

class FrankaCombinableHW : public hardware_interface::RobotHW {
 public:
  /**
   * Creates an instance of FrankaCombinableHW.
   *
   */
  FrankaCombinableHW();

  /**
   * Initializes the FrankaCombinableHW class as far as possible without connecting to a robot. This
   * includes setting up state, limit and command interfaces, publishers, services and actions.
   * Note: This method is mainly for testing purposes. Use the \ref init() method to control real
   * hardware.
   *
   * @param[in] root_nh A NodeHandle in the root of the caller namespace.
   * @param[in] robot_hw_nh A NodeHandle in the namespace from which the RobotHW.
   * @return True if the disconnected initialization was successful, false otherwise.
   */
  bool initROSInterfaces(ros::NodeHandle& root_nh, ros::NodeHandle& robot_hw_nh);

  /**
   * Initializes the FrankaCombinableHW with model, state, limit and command interfaces and connects
   * to a panda robot via an IP provided via the parameter server.
   *
   * @param[in] root_nh A NodeHandle in the root of the caller namespace.
   * @param[in] robot_hw_nh A NodeHandle in the namespace from which the RobotHW.
   * should read its configuration.
   *
   * @return True if initialization was successful, false otherwise.
   */
  bool init(ros::NodeHandle& root_nh, ros::NodeHandle& robot_hw_nh) override;

  ~FrankaCombinableHW() override = default;

  /**
   * Runs the currently active controller in a realtime loop.
   *
   * If no controller is active, the function immediately exits. When running a controller,
   * the function only exits when ros_callback returns false.
   *
   * @param[in] robot  A libfranka Robot instance.
   *
   * @throw franka::ControlException if an error related to torque control or motion generation
   * occurred.
   * @throw franka::InvalidOperationException if a conflicting operation is already running.
   * @throw franka::NetworkException if the connection is lost, e.g. after a timeout.
   * @throw franka::RealtimeException if realtime priority cannot be set for the current thread.
   */
  void control(franka::Robot& robot);

  /**
   * Updates the robot state in the controller interfaces from the given robot state.
   *
   * @param[in] robot_state Current robot state.
   */
  void update(const franka::RobotState& robot_state);

  /**
   * Indicates whether there is an active controller.
   *
   * @return True if a controller is currently active, false otherwise.
   */
  bool controllerActive() const noexcept;

  /**
   * Checks whether a requested controller can be run, based on the resources and interfaces it
   * claims.
   *
   * @param[in] info Controllers to be running at the same time.
   *
   * @return True in case of a conflict, false in case of valid controllers.
   */
  bool checkForConflict(const std::list<hardware_interface::ControllerInfo>& info) const override;

  /**
   * Performs controller switching (real-time capable).
   */
  void doSwitch(const std::list<hardware_interface::ControllerInfo>&,
                const std::list<hardware_interface::ControllerInfo>&) override;

  /**
   * Prepares switching between controllers (not real-time capable).
   *
   * @param[in] start_list Controllers requested to be started.
   * @param[in] stop_list Controllers requested to be stopped.
   *
   * @return True if the preparation has been successful, false otherwise.
   */
  bool prepareSwitch(const std::list<hardware_interface::ControllerInfo>& start_list,
                     const std::list<hardware_interface::ControllerInfo>& stop_list) override;

  /**
   * Reads data from the franka robot.
   *
   * @param[in] time The current time.
   * @param[in] period The time passed since the last call to \ref read.
   */
  void read(const ros::Time& time, const ros::Duration& period) override;

  /**
   * Writes data to the franka robot.
   *
   * @param[in] time The current time.
   * @param[in] period The time passed since the last call to \ref write.
   */
  void write(const ros::Time& time, const ros::Duration& period) override;

  /**
   * Gets the current joint torque command.
   *
   * @return Current joint torque command.
   */
  std::array<double, 7> getJointEffortCommand() const noexcept;

  /**
   * Gets the current joint position command.
   *
   * @return Current joint position command.
   */
  std::array<double, 7> getJointPositionCommand() const noexcept;

  /**
   * Gets the current joint velocity command.
   *
   * @return Current joint velocity command.
   */
  std::array<double, 7> getJointVelocityCommand() const noexcept;

  /**
   * Enforces limits on torque level.
   *
   * @param[in] period Duration of the current cycle.
   */
  void enforceLimits(const ros::Duration& period);

  /**
   * Gets arm id.
   */
  std::string getArmID();

  /**
   * Trigger error state to stop controller.
   */
  void triggerError();

  /**
   * Returns error state.
   *
   * @return true if there is currently error.
   */
  bool hasError();

  /**
   * Reset error state.
   */
  void resetError();

  /**
   * Returns whether the controller needs to be reset.
   */
  bool controllerNeedsReset();

  /**
   * Returns whether the torque command contains NaN values.
   *
   * @param[in] command The torque commmand to check.
   *
   * @return true if the command contains NaN, false otherwise.
   */
  static bool commandHasNaN(const franka::Torques& command);

  /**
   * Returns whether the position command contains NaN values.
   *
   * @param[in] command The position commmand to check.
   *
   * @return true if the command contains NaN, false otherwise.
   */
  static bool commandHasNaN(const franka::JointPositions& command);

  /**
   * Returns whether the velocity command contains NaN values.
   *
   * @param[in] command The velocity commmand to check.
   *
   * @return true if the command contains NaN, false otherwise.
   */
  static bool commandHasNaN(const franka::JointVelocities& command);

  /**
   * Returns whether the Cartesian pose command contains NaN values.
   *
   * @param[in] command The Cartesian pose commmand to check.
   *
   * @return true if the command contains NaN, false otherwise.
   */
  static bool commandHasNaN(const franka::CartesianPose& command);

  /**
   * Returns whether the Cartesian velocity command contains NaN values.
   *
   * @param[in] command The Cartesian velocity commmand to check.
   *
   * @return true if the command contains NaN, false otherwise.
   */
  static bool commandHasNaN(const franka::CartesianVelocities& command);

 private:
  template <size_t size>
  static bool arrayHasNaN(const std::array<double, size> array) {
    return std::any_of(array.begin(), array.end(), [](const double& e) { return std::isnan(e); });
  }

  template <typename T>
  T controlCallback(const T& command,
                    const franka::RobotState& robot_state,
                    franka::Duration time_step) {
    if (commandHasNaN(command)) {
      ROS_ERROR("FrankaCombinableHW: Got NaN value in command!");
      throw franka::CommandException("Got NaN value in command");
    }

    checkJointLimits();

    libfranka_state_mutex_.lock();
    robot_state_libfranka_ = robot_state;
    libfranka_state_mutex_.unlock();

    libfranka_cmd_mutex_.lock();
    T current_cmd = command;
    libfranka_cmd_mutex_.unlock();

    if (has_error_ || !controller_active_) {
      return franka::MotionFinished(current_cmd);
    }
    return current_cmd;
  }

  void controlLoop();

  void setupServicesAndActionServers(ros::NodeHandle& node_handle);

  static std::array<double, 7> saturateTorqueRate(const std::array<double, 7>& tau_d_calculated,
                                                  const std::array<double, 7>& tau_J_d);

  bool initRobot(ros::NodeHandle& robot_hw_nh);

  void publishErrorState(bool error);

  void checkJointLimits();

  std::unique_ptr<franka::Robot> robot_;
  std::unique_ptr<franka::Model> model_;

  hardware_interface::JointStateInterface joint_state_interface_{};
  franka_hw::FrankaStateInterface franka_state_interface_{};
  hardware_interface::EffortJointInterface effort_joint_interface_{};
  hardware_interface::PositionJointInterface position_joint_interface_{};
  hardware_interface::VelocityJointInterface velocity_joint_interface_{};
  franka_hw::FrankaPoseCartesianInterface franka_pose_cartesian_interface_{};
  franka_hw::FrankaVelocityCartesianInterface franka_velocity_cartesian_interface_{};
  franka_hw::FrankaModelInterface franka_model_interface_{};

  joint_limits_interface::EffortJointSoftLimitsInterface effort_joint_limit_interface_{};
  joint_limits_interface::VelocityJointSoftLimitsInterface velocity_joint_limit_interface_{};
  joint_limits_interface::PositionJointSoftLimitsInterface position_joint_limit_interface_{};

  urdf::Model urdf_model_;

  franka::RobotState robot_state_libfranka_{};
  franka::RobotState robot_state_ros_{};

  std::mutex libfranka_state_mutex_;
  std::mutex ros_state_mutex_;

  std::array<std::string, 7> joint_names_;
  std::string arm_id_;
  std::string robot_ip_;

  // command data of libfranka
  std::mutex libfranka_cmd_mutex_;
  franka::Torques effort_joint_command_libfranka_;
  franka::CartesianPose pose_cartesian_command_libfranka_;
  franka::CartesianVelocities velocity_cartesian_command_libfranka_;

  // command data of frankahw ros
  std::mutex ros_cmd_mutex_;
  franka::JointPositions position_joint_command_ros_;
  franka::JointVelocities velocity_joint_command_ros_;
  franka::Torques effort_joint_command_ros_;
  franka::CartesianPose pose_cartesian_command_ros_;
  franka::CartesianVelocities velocity_cartesian_command_ros_;

  std::function<void(franka::Robot&)> run_function_;

  bool limit_rate_{true};
  std::atomic_bool controller_active_{false};
  ControlMode current_control_mode_ = ControlMode::None;
  double joint_limit_warning_threshold_{10 * 3.14 / 180};

  std::unique_ptr<std::thread> control_loop_thread_;

  ros::Publisher has_error_pub_;
  ServiceContainer services_;
  std::unique_ptr<actionlib::SimpleActionServer<franka_control::ErrorRecoveryAction>>
      recovery_action_server_;

  std::atomic<bool> has_error_;
  std::atomic<bool> error_recovered_;
  bool controller_needs_reset_;
  bool initialized_ = false;
};

}  // namespace franka_combinable_hw

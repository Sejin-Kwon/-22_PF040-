import math
import numpy

from geometry_msgs.msg import Pose
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile
from rclpy.qos import ReliabilityPolicy, QoSProfile
from sensor_msgs.msg import LaserScan


class Slamtopic(Node):
    def __init__(self):
        super().__init__('slam_topic')

        qos = QoSProfile(depth=10)

        # Initialise publishers
        self.scan_pub = self.create_publisher(LaserScan, '/scan', qos)

        self.scan_sub = self.create_subscription(
            LaserScan,
            '/gazebo_ros_head_hokuyo_controller/out',
            self.scan_callback,
            QoSProfile(depth=10, reliability=ReliabilityPolicy.BEST_EFFORT))

    def scan_callback(self, msg):
        scan_msg = msg
        self.scan_pub.publish(scan_msg)


def main(args=None):
    rclpy.init(args=args)
    slam_topic = Slamtopic()
    rclpy.spin(slam_topic)

    slam_topic.destroy()
    rclpy.shutdown()


if __name__ == '__main__':
    main()

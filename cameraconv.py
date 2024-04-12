# convert cam_pose.txt format into camera.json format.

# Timestamp, x,y,z,qx,qy,qz,qw

from collections import namedtuple
import io
import json

import numpy as np
import quaternion  # pip install numpy-quaternion

def quat_to_mat(qx, qy, qz, qw):
    q = np.quaternion(qw, qx, qy, qz)
    return quaternion.as_rotation_matrix(q).tolist()

"""
inline Matrix<Scalar> Matrix<Scalar>::FromQuat(const Quat<Scalar>& q)
{
    Matrix<Scalar> m;
    m.SetXAxis(q.Rotate(Vector3<Scalar>(1, 0, 0)));
    m.SetYAxis(q.Rotate(Vector3<Scalar>(0, 1, 0)));
    m.SetZAxis(q.Rotate(Vector3<Scalar>(0, 0, 1)));
    m.SetTrans(Vector3<Scalar>(0, 0, 0));
    return m;
}
"""

Camera = namedtuple("Camera", ["timestamp", "x", "y", "z", "qx", "qy", "qz", "qw"])

cameras = []
with open("cam_pose.txt", "r") as f:
    for line in f:
        if line.strip():  # Check if the line is not empty
            fields = line.strip().split(" ")
            record = Camera(
                timestamp=float(fields[0]),
                x=float(fields[1]),
                y=float(fields[2]),
                z=float(fields[3]),
                qx=float(fields[4]),
                qy=float(fields[5]),
                qz=float(fields[6]),
                qw=float(fields[7]),
            )
            cameras.append(record)

objs = []
i = 0
for cam in cameras:
    objs.append(
        {
            "id": i,
            "img_name": str(i),
            "timestamp": cam.timestamp,
            "width": 1920,
            "height": 1080,
            "position": [cam.x, cam.y, cam.z],
            "rotation": quat_to_mat(cam.qx, cam.qy, cam.qz, cam.qw),
            "fx": 1000.0,
            "fy": 1000.0,
        }
    )
    i = i + 1

with open("cameras.json", "w") as f:
    f.write(json.dumps(objs, indent=2))

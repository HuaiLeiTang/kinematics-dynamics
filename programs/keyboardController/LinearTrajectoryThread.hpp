// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#ifndef __LINEAR_TRAJECTORY_THREAD_HPP__
#define __LINEAR_TRAJECTORY_THREAD_HPP__

#include <mutex>
#include <vector>

#include <yarp/os/PeriodicThread.h>

#include <kdl/trajectory.hpp>

#include "ICartesianControl.h"

namespace roboticslab
{

/**
 * @ingroup keyboardController
 *
 * @brief Periodic thread that encapsulates a linear trajectory
 */
class LinearTrajectoryThread : public yarp::os::PeriodicThread
{
public:
    LinearTrajectoryThread(int period, ICartesianControl * iCartesianControl);
    ~LinearTrajectoryThread();
    bool checkStreamingConfig();
    bool configure(const std::vector<double> & vels);
    void useTcpFrame(bool enableTcpFrame) { usingTcpFrame = enableTcpFrame; }

protected:
    virtual void run();

private:
    double period;
    ICartesianControl * iCartesianControl;
    KDL::Trajectory * trajectory;
    double startTime;
    bool usingStreamingCommandConfig;
    bool usingTcpFrame;
    std::vector<double> deltaX;
    mutable std::mutex mtx;
};

}  // namespace roboticslab

#endif  // __LINEAR_TRAJECTORY_THREAD_HPP__

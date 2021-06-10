// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#ifndef __GRABBER_RESPONDER_HPP__
#define __GRABBER_RESPONDER_HPP__

#include <yarp/os/Bottle.h>
#include <yarp/os/PortReaderBuffer.h>

#include "ICartesianControl.h"

namespace roboticslab
{

/**
 * @ingroup haarDetectionController
 *
 * @brief Callback class for dealing with incoming grabber
 * data streams.
 */
class GrabberResponder : public yarp::os::TypedReaderCallback<yarp::os::Bottle>
{
public:
    GrabberResponder() : iCartesianControl(nullptr),
                         isStopped(true),
                         noApproach(false)
    {}

    void onRead(yarp::os::Bottle &b) override;

    void setICartesianControlDriver(roboticslab::ICartesianControl *iCartesianControl)
    {
        this->iCartesianControl = iCartesianControl;
    }

    void setNoApproachSetting(bool noApproach)
    {
        this->noApproach = noApproach;
    }

private:
    roboticslab::ICartesianControl *iCartesianControl;

    bool isStopped, noApproach;
};

} // namespace roboticslab

#endif // __GRABBER_RESPONDER_HPP__

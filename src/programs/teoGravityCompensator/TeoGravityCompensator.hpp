// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#ifndef __TEO_GRAVITY_COMPENSATOR__
#define __TEO_GRAVITY_COMPENSATOR__

#include <yarp/os/RFModule.h>

#include <yarp/dev/all.h>

#include "ColorDebug.hpp"

#include "GravityRateThread.hpp"

#define DEFAULT_SOLVER "kdlsolver"

namespace teo
{

/**
 * @ingroup TeoGravityCompensator
 *
 * The TeoGravityCompensator class implements a server part that receives a connection from a remote
 * \ref cartesianServer module.
 * 
 */
class TeoGravityCompensator : public yarp::os::RFModule {

    public:

        TeoGravityCompensator() {}
        bool configure(yarp::os::ResourceFinder &rf);

    protected:

        GravityRateThread gravityRateThread;

        yarp::dev::PolyDriver solverDeviceRA;

        yarp::dev::PolyDriver robotDeviceRA;

        bool updateModule();
        bool interruptModule();
        // double getPeriod();


};

}  // namespace teo

#endif  // __TEO_GRAVITY_COMPENSATOR__

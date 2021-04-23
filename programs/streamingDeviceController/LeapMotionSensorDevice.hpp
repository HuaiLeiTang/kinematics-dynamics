#ifndef __LEAP_MOTION_SENSOR_DEVICE_HPP__
#define __LEAP_MOTION_SENSOR_DEVICE_HPP__

#include "StreamingDevice.hpp"

#include <vector>

#include <yarp/dev/IAnalogSensor.h>

namespace roboticslab
{

/**
 * @ingroup streamingDeviceController
 *
 * @brief Represents a LeapMotion device wrapped as an
 * analog sensor by YARP.
 */
class LeapMotionSensorDevice : public StreamingDevice
{
public:

    //! Constructor
    LeapMotionSensorDevice(yarp::os::Searchable & config, bool usingMovi);

    virtual bool acquireInterfaces();

    virtual bool initialize(bool usingStreamingPreset);

    virtual bool acquireData();

    virtual bool transformData(double scaling);

    virtual int getActuatorState();

    virtual void sendMovementCommand(double timestamp);

    virtual void stopMotion()
    {}

private:

    yarp::dev::IAnalogSensor * iAnalogSensor;

    bool usingMovi;

    std::vector<double> initialTcpOffset;
    std::vector<double> initialLeapOffset;

    bool hasActuator;
    bool grab, pinch;
};

}  // namespace roboticslab

#endif  // __LEAP_MOTION_SENSOR_DEVICE_HPP__

// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include "CartesianControlServer.hpp"

#include <sstream>

#include <yarp/os/Bottle.h>
#include <yarp/os/LogStream.h>
#include <yarp/os/Vocab.h>

using namespace roboticslab;

// -----------------------------------------------------------------------------

namespace
{
    inline bool isGroupParam(const yarp::os::Bottle& in)
    {
        return in.size() > 1 && in.get(1).asVocab() == VOCAB_CC_CONFIG_PARAMS;
    }

    inline void addValue(yarp::os::Bottle& b, int vocab, double value)
    {
        if (vocab == VOCAB_CC_CONFIG_FRAME || vocab == VOCAB_CC_CONFIG_STREAMING_CMD)
        {
            b.addVocab(value);
        }
        else
        {
            b.addFloat64(value);
        }
    }

    inline double asValue(int vocab, const yarp::os::Value& v)
    {
        if (vocab == VOCAB_CC_CONFIG_FRAME || vocab == VOCAB_CC_CONFIG_STREAMING_CMD)
        {
            return v.asVocab();
        }
        else
        {
            return v.asFloat64();
        }
    }
}

// ------------------- RpcResponder Related ------------------------------------

bool RpcResponder::respond(const yarp::os::Bottle& in, yarp::os::Bottle& out)
{
    switch (in.get(0).asVocab())
    {
    case VOCAB_CC_STAT:
        return handleStatMsg(in, out);
    case VOCAB_CC_INV:
        return handleFunctionCmdMsg(in, out, &ICartesianControl::inv);
    case VOCAB_CC_MOVJ:
        return handleConsumerCmdMsg(in, out, &ICartesianControl::movj);
    case VOCAB_CC_RELJ:
        return handleConsumerCmdMsg(in, out, &ICartesianControl::relj);
    case VOCAB_CC_MOVL:
        return handleConsumerCmdMsg(in, out, &ICartesianControl::movl);
    case VOCAB_CC_MOVV:
        return handleConsumerCmdMsg(in, out, &ICartesianControl::movv);
    case VOCAB_CC_GCMP:
        return handleRunnableCmdMsg(in, out, &ICartesianControl::gcmp);
    case VOCAB_CC_FORC:
        return handleConsumerCmdMsg(in, out, &ICartesianControl::forc);
    case VOCAB_CC_STOP:
        return handleRunnableCmdMsg(in, out, &ICartesianControl::stopControl);
    case VOCAB_CC_WAIT:
        return handleWaitMsg(in, out);
    case VOCAB_CC_TOOL:
        return handleConsumerCmdMsg(in, out, &ICartesianControl::tool);
    case VOCAB_CC_ACT:
        return handleActMsg(in, out);
    case VOCAB_CC_SET:
        return isGroupParam(in) ? handleParameterSetterGroup(in, out) : handleParameterSetter(in, out);
    case VOCAB_CC_GET:
        return isGroupParam(in) ? handleParameterGetterGroup(in, out) : handleParameterGetter(in, out);
    default:
        return DeviceResponder::respond(in, out);
    }
}

// -----------------------------------------------------------------------------

void RpcResponder::makeUsage()
{
    std::stringstream ss;

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_STAT) << "]";
    addUsage(ss.str().c_str(), "get controller state, current position in cartesian space and encoder acquisition timestamp");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_INV) << "] coord1 coord2 ...";
    addUsage(ss.str().c_str(), "accept desired position in cartesian space, return result in joint space");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_MOVJ) << "] coord1 coord2 ...";
    addUsage(ss.str().c_str(), "joint move to desired position (absolute coordinates in cartesian space)");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_RELJ) << "] coord1 coord2 ...";
    addUsage(ss.str().c_str(), "joint move to desired position (relative coordinates in cartesian space)");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_MOVL) << "] coord1 coord2 ...";
    addUsage(ss.str().c_str(), "linear move to desired position (absolute coordinates in cartesian space)");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_MOVV) << "] coord1 coord2 ...";
    addUsage(ss.str().c_str(), "velocity move using supplied vector (cartesian space)");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_GCMP) << "]";
    addUsage(ss.str().c_str(), "enable gravity compensation");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_FORC) << "] coord1 coord2 ...";
    addUsage(ss.str().c_str(), "enable torque control, apply input forces (cartesian space)");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_STOP) << "]";
    addUsage(ss.str().c_str(), "stop control");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_WAIT) << "] timeout";
    addUsage(ss.str().c_str(), "wait until completion with timeout (optional, 0.0 means no timeout)");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_TOOL) << "] coord1 coord2 ...";
    addUsage(ss.str().c_str(), "append fixed link to end effector");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_ACT) << "] vocab";
    addUsage(ss.str().c_str(), "actuate tool using selected command vocab");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_SET) << "] vocab value";
    addUsage(ss.str().c_str(), "set configuration parameter");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_GET) << "] vocab";
    addUsage(ss.str().c_str(), "get configuration parameter");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_SET) << "] [" << yarp::os::Vocab::decode(VOCAB_CC_CONFIG_PARAMS) << "] (vocab value) ...";
    addUsage(ss.str().c_str(), "set multiple configuration parameters");
    ss.str("");

    ss << "[" << yarp::os::Vocab::decode(VOCAB_CC_GET) << "] [" << yarp::os::Vocab::decode(VOCAB_CC_CONFIG_PARAMS) << "]";
    addUsage(ss.str().c_str(), "get all configuration parameters");
    ss.str("");

    ss << "... [" << yarp::os::Vocab::decode(VOCAB_CC_CONFIG_GAIN) << "] value";
    addUsage(ss.str().c_str(), "(config param) controller gain");
    ss.str("");

    ss << "... [" << yarp::os::Vocab::decode(VOCAB_CC_CONFIG_TRAJ_DURATION) << "] value";
    addUsage(ss.str().c_str(), "(config param) trajectory duration");
    ss.str("");

    ss << "... [" << yarp::os::Vocab::decode(VOCAB_CC_CONFIG_CMC_PERIOD) << "] value";
    addUsage(ss.str().c_str(), "(config param) CMC period [ms]");
    ss.str("");

    std::stringstream ss_wait;
    ss_wait << "(config param) check period of [" << yarp::os::Vocab::decode(VOCAB_CC_WAIT) << "] command [ms]";

    ss << "... [" << yarp::os::Vocab::decode(VOCAB_CC_CONFIG_WAIT_PERIOD) << "] value";
    addUsage(ss.str().c_str(), ss_wait.str().c_str());
    ss.str("");

    std::stringstream ss_frame;
    ss_frame << "(config param) reference frame, available (base/TCP):";
    ss_frame << " [" << yarp::os::Vocab::decode(ICartesianSolver::BASE_FRAME) << "]";
    ss_frame << " [" << yarp::os::Vocab::decode(ICartesianSolver::TCP_FRAME) << "]";

    ss << "... [" << yarp::os::Vocab::decode(VOCAB_CC_CONFIG_FRAME) << "] vocab";
    addUsage(ss.str().c_str(), ss_frame.str().c_str());
    ss.str("");

    std::stringstream ss_cmd;
    ss_cmd << "(config param) preset streaming command, available:";
    ss_cmd << " [" << yarp::os::Vocab::decode(VOCAB_CC_TWIST) << "]";
    ss_cmd << " [" << yarp::os::Vocab::decode(VOCAB_CC_POSE) << "]";
    ss_cmd << " [" << yarp::os::Vocab::decode(VOCAB_CC_MOVI) << "]";

    ss << "... [" << yarp::os::Vocab::decode(VOCAB_CC_CONFIG_STREAMING_CMD) << "] vocab";
    addUsage(ss.str().c_str(), ss_cmd.str().c_str());
    ss.str("");
}

// -----------------------------------------------------------------------------

bool RpcResponder::handleStatMsg(const yarp::os::Bottle& in, yarp::os::Bottle& out)
{
    std::vector<double> x;
    int state;
    double timestamp;

    if (iCartesianControl->stat(x, &state, &timestamp))
    {
        if (!transformOutgoingData(x))
        {
            out.addVocab(VOCAB_CC_FAILED);
            return false;
        }

        out.addVocab(state);

        for (size_t i = 0; i < x.size(); i++)
        {
            out.addFloat64(x[i]);
        }

        out.addFloat64(timestamp);

        return true;
    }
    else
    {
        out.addVocab(VOCAB_CC_FAILED);
        return false;
    }
}

// -----------------------------------------------------------------------------

bool RpcResponder::handleWaitMsg(const yarp::os::Bottle& in, yarp::os::Bottle& out)
{
    bool res;

    if (in.size() > 1)
    {
        double timeout = in.get(1).asFloat64();
        res = iCartesianControl->wait(timeout);
    }
    else
    {
        res = iCartesianControl->wait();
    }

    if (!res)
    {
        out.addVocab(VOCAB_CC_FAILED);
        return false;
    }

    out.addVocab(VOCAB_CC_OK);
    return true;
}

// -----------------------------------------------------------------------------

bool RpcResponder::handleActMsg(const yarp::os::Bottle& in, yarp::os::Bottle& out)
{
    if (in.size() > 1)
    {
        int commandCode = in.get(1).asVocab();

        if (iCartesianControl->act(commandCode))
        {
            out.addVocab(VOCAB_CC_OK);
            return true;
        }
        else
        {
            out.addVocab(VOCAB_CC_FAILED);
            return false;
        }
    }
    else
    {
        yError() << "Size error:" << in.size();
        out.addVocab(VOCAB_CC_FAILED);
        return false;
    }
}

// -----------------------------------------------------------------------------

bool RpcResponder::handleRunnableCmdMsg(const yarp::os::Bottle& in, yarp::os::Bottle& out, RunnableFun cmd)
{
    if ((iCartesianControl->*cmd)())
    {
        out.addVocab(VOCAB_CC_OK);
        return true;
    }
    else
    {
        out.addVocab(VOCAB_CC_FAILED);
        return false;
    }
}

// -----------------------------------------------------------------------------

bool RpcResponder::handleConsumerCmdMsg(const yarp::os::Bottle& in, yarp::os::Bottle& out, ConsumerFun cmd)
{
    if (in.size() > 1)
    {
        std::vector<double> vin;

        for (size_t i = 1; i < in.size(); i++)
        {
            vin.push_back(in.get(i).asFloat64());
        }

        if (!transformIncomingData(vin) || !(iCartesianControl->*cmd)(vin))
        {
            out.addVocab(VOCAB_CC_FAILED);
            return false;
        }

        out.addVocab(VOCAB_CC_OK);
        return true;
    }
    else
    {
        yError() << "Size error:" << in.size();
        out.addVocab(VOCAB_CC_FAILED);
        return false;
    }
}

// -----------------------------------------------------------------------------

bool RpcResponder::handleFunctionCmdMsg(const yarp::os::Bottle& in, yarp::os::Bottle& out, FunctionFun cmd)
{
    if (in.size() > 1)
    {
        std::vector<double> vin, vout;

        for (size_t i = 1; i < in.size(); i++)
        {
            vin.push_back(in.get(i).asFloat64());
        }

        if (!transformIncomingData(vin) || !(iCartesianControl->*cmd)(vin, vout))
        {
            out.addVocab(VOCAB_CC_FAILED);
            return false;
        }

        for (size_t i = 0; i < vout.size(); i++)
        {
            out.addFloat64(vout[i]);
        }

        return true;
    }
    else
    {
        yError() << "Size error:" << in.size();
        out.addVocab(VOCAB_CC_FAILED);
        return false;
    }
}

// -----------------------------------------------------------------------------

bool RpcResponder::handleParameterSetter(const yarp::os::Bottle& in, yarp::os::Bottle& out)
{
    if (in.size() > 2)
    {
        int vocab = in.get(1).asVocab();
        double value = asValue(vocab, in.get(2));

        if (!iCartesianControl->setParameter(vocab, value))
        {
            out.addVocab(VOCAB_CC_FAILED);
            return false;
        }

        out.addVocab(VOCAB_CC_OK);
        return true;
    }
    else
    {
        yError() << "Size error:" << in.size();
        out.addVocab(VOCAB_CC_FAILED);
        return false;
    }
}

// -----------------------------------------------------------------------------

bool RpcResponder::handleParameterGetter(const yarp::os::Bottle& in, yarp::os::Bottle& out)
{
    if (in.size() > 1)
    {
        int vocab = in.get(1).asVocab();
        double value;

        if (!iCartesianControl->getParameter(vocab, &value))
        {
            out.addVocab(VOCAB_CC_FAILED);
            return false;
        }

        addValue(out, vocab, value);
        return true;
    }
    else
    {
        yError() << "Size error:" << in.size();
        out.addVocab(VOCAB_CC_FAILED);
        return false;
    }
}

// -----------------------------------------------------------------------------

bool RpcResponder::handleParameterSetterGroup(const yarp::os::Bottle& in, yarp::os::Bottle& out)
{
    if (in.size() > 2)
    {
        std::map<int, double> params;

        for (int i = 2; i < in.size(); i++)
        {
            if (!in.get(i).isList() || in.get(i).asList()->size() != 2)
            {
                yError() << "Bottle format error";
                out.addVocab(VOCAB_CC_FAILED);
                return false;
            }

            yarp::os::Bottle * b = in.get(i).asList();
            int vocab = b->get(0).asVocab();
            double value = asValue(vocab, b->get(1));
            params.emplace(std::make_pair(vocab, value));
        }

        if (!iCartesianControl->setParameters(params))
        {
            out.addVocab(VOCAB_CC_FAILED);
            return false;
        }

        out.addVocab(VOCAB_CC_OK);
        return true;
    }
    else
    {
        yError() << "Size error:" << in.size();
        out.addVocab(VOCAB_CC_FAILED);
        return false;
    }
}

// -----------------------------------------------------------------------------

bool RpcResponder::handleParameterGetterGroup(const yarp::os::Bottle& in, yarp::os::Bottle& out)
{
    if (in.size() == 2)
    {
        std::map<int, double> params;

        if (!iCartesianControl->getParameters(params))
        {
            out.addVocab(VOCAB_CC_FAILED);
            return false;
        }

        for (const auto & it : params)
        {
            yarp::os::Bottle & b = out.addList();
            b.addVocab(it.first);
            addValue(b, it.first, it.second);
        }

        return true;
    }
    else
    {
        yError() << "Size error:" << in.size();
        out.addVocab(VOCAB_CC_FAILED);
        return false;
    }
}

// -----------------------------------------------------------------------------

bool RpcTransformResponder::transformIncomingData(std::vector<double>& vin)
{
    return KinRepresentation::encodePose(vin, vin, coord, orient, units);
}

// -----------------------------------------------------------------------------

bool RpcTransformResponder::transformOutgoingData(std::vector<double>& vout)
{
    return KinRepresentation::decodePose(vout, vout, coord, orient, units);
}

// -----------------------------------------------------------------------------

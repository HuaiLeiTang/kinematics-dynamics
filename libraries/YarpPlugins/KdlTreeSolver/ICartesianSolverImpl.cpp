// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include "KdlTreeSolver.hpp"

#include <yarp/os/LogStream.h>

#include <kdl/frames.hpp>
#include <kdl/jntarray.hpp>
#include <kdl/joint.hpp>
#include <kdl/segment.hpp>

#include "KdlVectorConverter.hpp"
#include "KinematicRepresentation.hpp"

using namespace roboticslab;

// -----------------------------------------------------------------------------

bool KdlTreeSolver::getNumJoints(int * numJoints)
{
    *numJoints = tree.getNrOfJoints();
    return true;
}

// -----------------------------------------------------------------------------

bool KdlTreeSolver::appendLink(const std::vector<double> & x)
{
    yError() << "Not supported: appendLink";
    return false;
}

// -----------------------------------------------------------------------------

bool KdlTreeSolver::restoreOriginalChain()
{
    yError() << "Not supported: restoreOriginalChain";
    return false;
}

// -----------------------------------------------------------------------------

bool KdlTreeSolver::changeOrigin(const std::vector<double> & x_old_obj, const std::vector<double> & x_new_old, std::vector<double> & x_new_obj)
{
    KDL::Frame H_old_obj = KdlVectorConverter::vectorToFrame(x_old_obj);
    KDL::Frame H_new_old = KdlVectorConverter::vectorToFrame(x_new_old);
    KDL::Frame H_new_obj = H_new_old * H_old_obj;

    x_new_obj = KdlVectorConverter::frameToVector(H_new_obj);

    return true;
}

// -----------------------------------------------------------------------------

bool KdlTreeSolver::fwdKin(const std::vector<double> & q, std::vector<double> & x)
{
    KDL::JntArray qInRad(tree.getNrOfJoints());

    for (int motor = 0; motor < tree.getNrOfJoints(); motor++)
    {
        qInRad(motor) = KinRepresentation::degToRad(q[motor]);
    }

    x.clear();
    x.reserve(endpoints.size() * 6);

    for (const auto & endpoint : endpoints)
    {
        KDL::Frame fOutCart;

        if (fkSolverPos->JntToCart(qInRad, fOutCart, endpoint) < 0)
        {
            return false;
        }

        auto temp = KdlVectorConverter::frameToVector(fOutCart);
        x.insert(x.end(), temp.cbegin(), temp.cend());
    }

    return true;
}

// -----------------------------------------------------------------------------

bool KdlTreeSolver::poseDiff(const std::vector<double> & xLhs, const std::vector<double> & xRhs, std::vector<double> & xOut)
{
    KDL::Frame fLhs = KdlVectorConverter::vectorToFrame(xLhs);
    KDL::Frame fRhs = KdlVectorConverter::vectorToFrame(xRhs);

    KDL::Twist diff = KDL::diff(fRhs, fLhs); // [fLhs - fRhs] for translation
    xOut = KdlVectorConverter::twistToVector(diff);

    return true;
}

// -----------------------------------------------------------------------------

bool KdlTreeSolver::invKin(const std::vector<double> & xd, const std::vector<double> & qGuess, std::vector<double> & q, const reference_frame frame)
{
    KDL::Frames frames;

    for (auto i = 0; i < endpoints.size(); i++)
    {
        std::vector<double> sub(xd.cbegin() + i * 6, xd.cbegin() + (i + 1) * 6);
        frames.insert(std::make_pair(endpoints[i], KdlVectorConverter::vectorToFrame(sub)));
    }

    KDL::JntArray qGuessInRad(tree.getNrOfJoints());

    for (int motor = 0; motor < tree.getNrOfJoints(); motor++)
    {
        qGuessInRad(motor) = KinRepresentation::degToRad(qGuess[motor]);
    }

    if (frame == TCP_FRAME)
    {
        for (const auto & endpoint : endpoints)
        {
            KDL::Frame fOutCart;

            if (fkSolverPos->JntToCart(qGuessInRad, fOutCart, endpoint) < 0)
            {
                return false;
            }

            auto it = frames.find(endpoint);
            it->second = fOutCart * it->second;
        }
    }
    else if (frame != BASE_FRAME)
    {
        yWarning() << "Unsupported frame";
        return false;
    }

    KDL::JntArray kdlq(tree.getNrOfJoints());

    if (ikSolverPos->CartToJnt(qGuessInRad, frames, kdlq) < 0)
    {
        return false;
    }

    q.resize(tree.getNrOfJoints());

    for (int motor = 0; motor < tree.getNrOfJoints(); motor++)
    {
        q[motor] = KinRepresentation::radToDeg(kdlq(motor));
    }

    return true;
}

// -----------------------------------------------------------------------------

bool KdlTreeSolver::diffInvKin(const std::vector<double> & q, const std::vector<double> & xdot, std::vector<double> & qdot, const reference_frame frame)
{
    KDL::Twists twists;

    for (auto i = 0; i < endpoints.size(); i++)
    {
        std::vector<double> sub(xdot.cbegin() + i * 6, xdot.cbegin() + (i + 1) * 6);
        twists.insert(std::make_pair(endpoints[i], KdlVectorConverter::vectorToTwist(sub)));
    }

    KDL::JntArray qInRad(tree.getNrOfJoints());

    for (int motor = 0; motor < tree.getNrOfJoints(); motor++)
    {
        qInRad(motor) = KinRepresentation::degToRad(q[motor]);
    }

    if (frame == TCP_FRAME)
    {
        for (const auto & endpoint : endpoints)
        {
            KDL::Frame fOutCart;

            if (fkSolverPos->JntToCart(qInRad, fOutCart, endpoint) < 0)
            {
                return false;
            }

            auto it = twists.find(endpoint);

            //-- Transform the basis to which the twist is expressed, but leave the reference point intact
            //-- "Twist and Wrench transformations" @ http://docs.ros.org/latest/api/orocos_kdl/html/geomprim.html
            it->second = fOutCart.M * it->second;
        }
    }
    else if (frame != BASE_FRAME)
    {
        yWarning() << "Unsupported frame";
        return false;
    }

    KDL::JntArray qDotOutRadS(tree.getNrOfJoints());

    if (ikSolverVel->CartToJnt(qInRad, twists, qDotOutRadS) < 0)
    {
        return false;
    }

    qdot.resize(tree.getNrOfJoints());

    for (int motor = 0; motor < tree.getNrOfJoints(); motor++)
    {
        qdot[motor] = KinRepresentation::radToDeg(qDotOutRadS(motor));
    }

    return true;
}

// -----------------------------------------------------------------------------

bool KdlTreeSolver::invDyn(const std::vector<double> & q, std::vector<double> & t)
{
    KDL::JntArray qInRad(tree.getNrOfJoints());

    for (int motor = 0; motor < tree.getNrOfJoints(); motor++)
    {
        qInRad(motor) = KinRepresentation::degToRad(q[motor]);
    }

    KDL::JntArray qdotInRad(tree.getNrOfJoints());
    KDL::JntArray qdotdotInRad(tree.getNrOfJoints());
    KDL::JntArray kdlt(tree.getNrOfJoints());
    KDL::WrenchMap wrenches;

    for (const auto & endpoint : endpoints)
    {
        wrenches.insert(std::make_pair(endpoint, KDL::Wrench::Zero()));
    }

    if (idSolver->CartToJnt(qInRad, qdotInRad, qdotdotInRad, wrenches, kdlt) < 0)
    {
        return false;
    }

    t.resize(tree.getNrOfJoints());

    for (int motor = 0; motor < tree.getNrOfJoints(); motor++)
    {
        t[motor] = kdlt(motor);
    }

    return true;
}

// -----------------------------------------------------------------------------

bool KdlTreeSolver::invDyn(const std::vector<double> & q, const std::vector<double> & qdot, const std::vector<double> & qdotdot, const std::vector<std::vector<double>> & fexts, std::vector<double> & t)
{
    KDL::JntArray qInRad(tree.getNrOfJoints());

    for (int motor = 0; motor < tree.getNrOfJoints(); motor++)
    {
        qInRad(motor) = KinRepresentation::degToRad(q[motor]);
    }

    KDL::JntArray qdotInRad(tree.getNrOfJoints());

    for (int motor = 0; motor < tree.getNrOfJoints(); motor++)
    {
        qdotInRad(motor) = KinRepresentation::degToRad(qdot[motor]);
    }

    KDL::JntArray qdotdotInRad(tree.getNrOfJoints());

    for (int motor = 0; motor < tree.getNrOfJoints(); motor++)
    {
        qdotdotInRad(motor) = KinRepresentation::degToRad(qdotdot[motor]);
    }

    KDL::WrenchMap wrenches;

    for (const auto & endpoint : endpoints)
    {
        // FIXME: mapping this is not trivial
        return false;
    }

    KDL::JntArray kdlt(tree.getNrOfJoints());

    if (idSolver->CartToJnt(qInRad, qdotInRad, qdotdotInRad, wrenches, kdlt) < 0)
    {
        return false;
    }

    t.resize(tree.getNrOfJoints());

    for (int motor = 0; motor < tree.getNrOfJoints(); motor++)
    {
        t[motor] = kdlt(motor);
    }

    return true;
}

// -----------------------------------------------------------------------------

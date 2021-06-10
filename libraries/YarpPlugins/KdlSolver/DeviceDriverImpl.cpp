// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#include "KdlSolver.hpp"

#include <string>

#include <yarp/os/Bottle.h>
#include <yarp/os/LogStream.h>
#include <yarp/os/Property.h>
#include <yarp/os/ResourceFinder.h>
#include <yarp/os/Value.h>

#include <yarp/sig/Matrix.h>

#include <kdl/frames.hpp>
#include <kdl/jntarray.hpp>
#include <kdl/joint.hpp>
#include <kdl/rigidbodyinertia.hpp>
#include <kdl/rotationalinertia.hpp>
#include <kdl/segment.hpp>

#include <kdl/chainfksolverpos_recursive.hpp>
#include <kdl/chainiksolverpos_lma.hpp>
#include <kdl/chainiksolverpos_nr_jl.hpp>
#include <kdl/chainiksolvervel_pinv.hpp>
#include <kdl/chainidsolver_recursive_newton_euler.hpp>

#include <Eigen/Core> // Eigen::Matrix

#include "KinematicRepresentation.hpp"
#include "ConfigurationSelector.hpp"

#include "ChainIkSolverPos_ST.hpp"
#include "ChainIkSolverPos_ID.hpp"

using namespace roboticslab;

constexpr auto DEFAULT_KINEMATICS = "none.ini";
constexpr auto DEFAULT_NUM_LINKS = 1;

constexpr auto DEFAULT_EPSILON = 0.005; // Precision tolerance
constexpr auto DEFAULT_DURATION = 20; // For Trajectory
constexpr auto DEFAULT_MAXVEL = 7.5; // unit/s
constexpr auto DEFAULT_MAXACC = 0.2; // unit/s^2

constexpr auto DEFAULT_EPS = 1e-9;
constexpr auto DEFAULT_MAXITER = 1000;
constexpr auto DEFAULT_IK_SOLVER = "lma";
constexpr auto DEFAULT_LMA_WEIGHTS = "1 1 1 0.1 0.1 0.1";
constexpr auto DEFAULT_STRATEGY = "leastOverallAngularDisplacement";

// ------------------- DeviceDriver Related ------------------------------------

namespace
{
    bool getMatrixFromProperties(const yarp::os::Searchable & options, const std::string & tag, yarp::sig::Matrix & H)
    {
        yarp::os::Bottle * bH = options.find(tag).asList();

        if (!bH)
        {
            yWarning() << "Unable to find tag" << tag;
            return false;
        }

        int i = 0;
        int j = 0;

        H.zero();

        for (int cnt = 0; cnt < bH->size() && cnt < H.rows() * H.cols(); cnt++)
        {
            H(i, j) = bH->get(cnt).asFloat64();

            if (++j >= H.cols())
            {
                i++;
                j = 0;
            }
        }

        return true;
    }

    bool parseLmaFromBottle(const yarp::os::Bottle & b, Eigen::Matrix<double, 6, 1> & L)
    {
        if (b.size() != 6)
        {
            yWarning("Wrong bottle size (expected: %d, was: %zu)", 6, b.size());
            return false;
        }

        for (int i = 0; i < b.size(); i++)
        {
            L(i) = b.get(i).asFloat64();
        }

        return true;
    }

    bool retrieveJointLimits(const yarp::os::Searchable & options, KDL::JntArray & qMin, KDL::JntArray & qMax)
    {
        int nrOfJoints = qMin.rows();

        if (!options.check("mins") || !options.check("maxs"))
        {
            yError() << "Missing 'mins' and/or 'maxs' option(s)";
            return false;
        }

        yarp::os::Bottle * maxs = options.findGroup("maxs", "joint upper limits (meters or degrees)").get(1).asList();
        yarp::os::Bottle * mins = options.findGroup("mins", "joint lower limits (meters or degrees)").get(1).asList();

        if (!maxs || !mins)
        {
            yError() << "Empty 'mins' and/or 'maxs' option(s)";
            return false;
        }

        if (maxs->size() < nrOfJoints || mins->size() < nrOfJoints)
        {
            yError("chain.getNrOfJoints (%d) > maxs.size() or mins.size() (%zu, %zu)", nrOfJoints, maxs->size(), mins->size());
            return false;
        }

        for (int motor = 0; motor < nrOfJoints; motor++)
        {
            qMax(motor) = roboticslab::KinRepresentation::degToRad(maxs->get(motor).asFloat64());
            qMin(motor) = roboticslab::KinRepresentation::degToRad(mins->get(motor).asFloat64());

            if (qMin(motor) == qMax(motor))
            {
                yWarning("qMin[%1$d] == qMax[%1$d] (%2$f)", motor, qMin(motor));
            }
            else if (qMin(motor) > qMax(motor))
            {
                yError("qMin[%1$d] > qMax[%1$d] (%2$f > %3$f)", motor, qMin(motor), qMax(motor));
                return false;
            }
        }

        return true;
    }
}

// -----------------------------------------------------------------------------

bool KdlSolver::open(yarp::os::Searchable& config)
{
    yDebug() << "KdlSolver config:" << config.toString();

    //-- kinematics
    std::string kinematics = config.check("kinematics",yarp::os::Value(DEFAULT_KINEMATICS),"path to file with description of robot kinematics").asString();
    yInfo() << "Kinematics file:" << kinematics;
    yarp::os::ResourceFinder rf;
    rf.setVerbose(false);
    rf.setDefaultContext("kinematics");
    std::string kinematicsFullPath = rf.findFileByName(kinematics);

    yarp::os::Property fullConfig;
    fullConfig.fromConfigFile(kinematicsFullPath.c_str());
    fullConfig.fromString(config.toString(),false);  //-- Can override kinematics file contents.
    fullConfig.setMonitor(config.getMonitor(), "KdlSolver");

    yDebug() << "Full config:" << fullConfig.toString();

    //-- numlinks
    int numLinks = fullConfig.check("numLinks",yarp::os::Value(DEFAULT_NUM_LINKS),"chain number of segments").asInt32();
    yInfo() << "numLinks:" << numLinks;

    //-- gravity
    yarp::os::Value defaultGravityValue;
    yarp::os::Bottle *defaultGravityBottle = defaultGravityValue.asList();
    defaultGravityBottle->addFloat64(0);
    defaultGravityBottle->addFloat64(0);
    defaultGravityBottle->addFloat64(-9.81);

    yarp::os::Value gravityValue = fullConfig.check("gravity", defaultGravityValue, "gravity vector (SI units)");
    yarp::os::Bottle *gravityBottle = gravityValue.asList();
    KDL::Vector gravity(gravityBottle->get(0).asFloat64(),gravityBottle->get(1).asFloat64(),gravityBottle->get(2).asFloat64());
    yInfo() << "gravity:" << gravityBottle->toString();

    //-- H0
    yarp::sig::Matrix defaultYmH0(4,4);
    defaultYmH0.eye();

    yarp::sig::Matrix ymH0(4,4);
    std::string ymH0_str("H0");
    if( ! getMatrixFromProperties(fullConfig, ymH0_str, ymH0))
    {
        ymH0 = defaultYmH0;
    }

    KDL::Vector kdlVec0(ymH0(0,3),ymH0(1,3),ymH0(2,3));
    KDL::Rotation kdlRot0( ymH0(0,0),ymH0(0,1),ymH0(0,2),ymH0(1,0),ymH0(1,1),ymH0(1,2),ymH0(2,0),ymH0(2,1),ymH0(2,2));
    chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::None), KDL::Frame(kdlRot0,kdlVec0)));  //-- H0 = Frame(kdlRot0,kdlVec0);
    yInfo() << "H0:" << ymH0.toString();

    //-- links
    for(int linkIndex=0;linkIndex<numLinks;linkIndex++)
    {
        std::string link("link_");
        std::ostringstream s;
        s << linkIndex;
        link += s.str();
        yarp::os::Bottle &bLink = fullConfig.findGroup(link);
        if( ! bLink.isNull() ) {
            //-- Kinematic
            double linkOffset = bLink.check("offset",yarp::os::Value(0.0), "DH joint angle (degrees)").asFloat64();
            double linkD = bLink.check("D",yarp::os::Value(0.0), "DH link offset (meters)").asFloat64();
            double linkA = bLink.check("A",yarp::os::Value(0.0), "DH link length (meters)").asFloat64();
            double linkAlpha = bLink.check("alpha",yarp::os::Value(0.0), "DH link twist (degrees)").asFloat64();
            //-- Dynamic
            if( bLink.check("mass") && bLink.check("cog") && bLink.check("inertia")) {
                double linkMass = bLink.check("mass",yarp::os::Value(0.0), "link mass (SI units)").asFloat64();
                yarp::os::Bottle linkCog = bLink.findGroup("cog", "vector of link's center of gravity (SI units)").tail();
                yarp::os::Bottle linkInertia = bLink.findGroup("inertia", "vector of link's inertia (SI units)").tail();
                chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::RotZ), KDL::Frame::DH(linkA,KinRepresentation::degToRad(linkAlpha),linkD,KinRepresentation::degToRad(linkOffset)),
                                              KDL::RigidBodyInertia(linkMass,KDL::Vector(linkCog.get(0).asFloat64(),linkCog.get(1).asFloat64(),linkCog.get(2).asFloat64()),
                                                                    KDL::RotationalInertia(linkInertia.get(0).asFloat64(),linkInertia.get(1).asFloat64(),linkInertia.get(2).asFloat64(),0,0,0))));
                yInfo("Added: %s (offset %f) (D %f) (A %f) (alpha %f) (mass %f) (cog %f %f %f) (inertia %f %f %f)",
                           link.c_str(), linkOffset,linkD,linkA,linkAlpha,linkMass,
                           linkCog.get(0).asFloat64(),linkCog.get(1).asFloat64(),linkCog.get(2).asFloat64(),
                           linkInertia.get(0).asFloat64(),linkInertia.get(1).asFloat64(),linkInertia.get(2).asFloat64());
            }
            else //-- No mass -> skip dynamics
            {
                chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::RotZ),KDL::Frame::DH(linkA,KinRepresentation::degToRad(linkAlpha),linkD,KinRepresentation::degToRad(linkOffset))));
                yInfo("Added: %s (offset %f) (D %f) (A %f) (alpha %f)",link.c_str(), linkOffset,linkD,linkA,linkAlpha);
            }
            continue;
        }

        std::string xyzLink("xyzLink_");
        std::ostringstream xyzS;
        xyzS << linkIndex;
        xyzLink += xyzS.str();
        yWarning("Not found: \"%s\", looking for \"%s\" instead", link.c_str(), xyzLink.c_str());
        yarp::os::Bottle &bXyzLink = fullConfig.findGroup(xyzLink);
        if( bXyzLink.isNull() ) {
            yError() << "Not found" << xyzLink << "either";
            return false;
        }
        double linkX = bXyzLink.check("x",yarp::os::Value(0.0), "X coordinate of next frame (meters)").asFloat64();
        double linkY = bXyzLink.check("y",yarp::os::Value(0.0), "Y coordinate of next frame (meters)").asFloat64();
        double linkZ = bXyzLink.check("z",yarp::os::Value(0.0), "Z coordinate of next frame (meters)").asFloat64();

        std::string linkTypes = "joint type (Rot[XYZ]|InvRot[XYZ]|Trans[XYZ]|InvTrans[XYZ]), e.g. 'RotZ'";
        std::string linkType = bXyzLink.check("Type",yarp::os::Value("NULL"), linkTypes.c_str()).asString();
        if(linkType == "RotX") {
            chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::RotX),KDL::Frame(KDL::Vector(linkX,linkY,linkZ))));
        } else if(linkType == "RotY") {
            chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::RotY),KDL::Frame(KDL::Vector(linkX,linkY,linkZ))));
        } else if(linkType == "RotZ") {
            chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::RotZ),KDL::Frame(KDL::Vector(linkX,linkY,linkZ))));
        } else if(linkType == "InvRotX") {
            chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::RotX,-1.0),KDL::Frame(KDL::Vector(linkX,linkY,linkZ))));
        } else if(linkType == "InvRotY") {
            chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::RotY,-1.0),KDL::Frame(KDL::Vector(linkX,linkY,linkZ))));
        } else if(linkType == "InvRotZ") {
            chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::RotZ,-1.0),KDL::Frame(KDL::Vector(linkX,linkY,linkZ))));
        } else if(linkType == "TransX") {
            chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::TransX),KDL::Frame(KDL::Vector(linkX,linkY,linkZ))));
        } else if(linkType == "TransY") {
            chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::TransY),KDL::Frame(KDL::Vector(linkX,linkY,linkZ))));
        } else if(linkType == "TransZ") {
            chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::TransZ),KDL::Frame(KDL::Vector(linkX,linkY,linkZ))));
        } else if(linkType == "InvTransX") {
            chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::TransX,-1.0),KDL::Frame(KDL::Vector(linkX,linkY,linkZ))));
        } else if(linkType == "InvTransY") {
            chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::TransY,-1.0),KDL::Frame(KDL::Vector(linkX,linkY,linkZ))));
        } else if(linkType == "InvTransZ") {
            chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::TransZ,-1.0),KDL::Frame(KDL::Vector(linkX,linkY,linkZ))));
        } else {
            yWarning() << "Link joint type" << linkType << "unrecognized";
        }

        yInfo("Added: %s (Type %s) (x %f) (y %f) (z %f)",xyzLink.c_str(),linkType.c_str(),linkX,linkY,linkZ);
    }

    //-- HN
    yarp::sig::Matrix defaultYmHN(4,4);
    defaultYmHN.eye();

    yarp::sig::Matrix ymHN(4,4);
    std::string ymHN_str("HN");
    if( ! getMatrixFromProperties(fullConfig, ymHN_str, ymHN))
    {
        ymHN = defaultYmHN;
    }

    KDL::Vector kdlVecN(ymHN(0,3),ymHN(1,3),ymHN(2,3));
    KDL::Rotation kdlRotN( ymHN(0,0),ymHN(0,1),ymHN(0,2),ymHN(1,0),ymHN(1,1),ymHN(1,2),ymHN(2,0),ymHN(2,1),ymHN(2,2));
    chain.addSegment(KDL::Segment(KDL::Joint(KDL::Joint::None), KDL::Frame(kdlRotN,kdlVecN)));
    yInfo() << "HN:" << ymHN.toString();

    yInfo() << "Chain number of segments (post- H0 and HN):" << chain.getNrOfSegments();
    yInfo() << "Chain number of joints (post- H0 and HN):" << chain.getNrOfJoints();

    fkSolverPos = new KDL::ChainFkSolverPos_recursive(chain);
    ikSolverVel = new KDL::ChainIkSolverVel_pinv(chain);
    idSolver = new KDL::ChainIdSolver_RNE(chain, gravity);

    //-- IK solver algorithm.
    std::string ik = fullConfig.check("ik", yarp::os::Value(DEFAULT_IK_SOLVER), "IK solver algorithm (lma, nrjl, st, id)").asString();

    if (ik == "lma")
    {
        std::string weightsStr = fullConfig.check("weights", yarp::os::Value(DEFAULT_LMA_WEIGHTS), "LMA algorithm weights (bottle of 6 doubles)").asString();
        yarp::os::Bottle weights(weightsStr);
        Eigen::Matrix<double, 6, 1> L;

        if (!parseLmaFromBottle(weights, L))
        {
            yError() << "Unable to parse LMA weights";
            return false;
        }

        ikSolverPos = new KDL::ChainIkSolverPos_LMA(chain, L);
    }
    else if (ik == "nrjl")
    {
        KDL::JntArray qMax(chain.getNrOfJoints());
        KDL::JntArray qMin(chain.getNrOfJoints());

        //-- Joint limits.
        if (!retrieveJointLimits(fullConfig, qMin, qMax))
        {
            yError() << "Unable to retrieve joint limits";
            return false;
        }

        //-- Precision and max iterations.
        double eps = fullConfig.check("eps", yarp::os::Value(DEFAULT_EPS), "IK solver precision (meters)").asFloat64();
        double maxIter = fullConfig.check("maxIter", yarp::os::Value(DEFAULT_MAXITER), "maximum number of iterations").asInt32();

        ikSolverPos = new KDL::ChainIkSolverPos_NR_JL(chain, qMin, qMax, *fkSolverPos, *ikSolverVel, maxIter, eps);
    }
    else if (ik == "st")
    {
        KDL::JntArray qMax(chain.getNrOfJoints());
        KDL::JntArray qMin(chain.getNrOfJoints());

        //-- Joint limits.
        if (!retrieveJointLimits(fullConfig, qMin, qMax))
        {
            yError() << "Unable to retrieve joint limits";
            return false;
        }

        //-- IK configuration selection strategy.
        std::string strategy = fullConfig.check("invKinStrategy", yarp::os::Value(DEFAULT_STRATEGY), "IK configuration strategy").asString();

        if (strategy == "leastOverallAngularDisplacement")
        {
            ConfigurationSelectorLeastOverallAngularDisplacementFactory factory(qMin, qMax);
            ikSolverPos = ChainIkSolverPos_ST::create(chain, factory);
        }
        else if (strategy == "humanoidGait")
        {
            ConfigurationSelectorHumanoidGaitFactory factory(qMin, qMax);
            ikSolverPos = ChainIkSolverPos_ST::create(chain, factory);
        }
        else
        {
            yError() << "Unsupported IK strategy:" << strategy;
            return false;
        }

        if (!ikSolverPos)
        {
            yError() << "Unable to solve IK";
            return false;
        }
    }
    else if (ik == "id")
    {
        KDL::JntArray qMax(chain.getNrOfJoints());
        KDL::JntArray qMin(chain.getNrOfJoints());

        //-- Joint limits.
        if (!retrieveJointLimits(fullConfig, qMin, qMax))
        {
            yError() << "Unable to retrieve joint limits";
            return false;
        }

        ikSolverPos = new ChainIkSolverPos_ID(chain, qMin, qMax, *fkSolverPos);
    }
    else
    {
        yError() << "Unsupported IK solver algorithm:" << ik.c_str();
        return false;
    }

    originalChain = chain;

    return true;
}

// -----------------------------------------------------------------------------

bool KdlSolver::close()
{
    delete fkSolverPos;
    delete ikSolverPos;
    delete ikSolverVel;
    delete idSolver;

    return true;
}

// -----------------------------------------------------------------------------

// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

#ifndef __SCREW_THEORY_IK_PROBLEM_HPP__
#define __SCREW_THEORY_IK_PROBLEM_HPP__

#include <utility>
#include <vector>

#include <kdl/frames.hpp>
#include <kdl/jntarray.hpp>

#include "ProductOfExponentials.hpp"

namespace roboticslab
{

/**
 * @ingroup ScrewTheoryLib
 * @brief ...
 */
class ScrewTheoryIkSubproblem
{
public:

    virtual ~ScrewTheoryIkSubproblem() {}

    virtual std::vector< std::vector< std::pair<int, double> > > solve(const KDL::Vector & rhs) = 0;
};

/**
 * @ingroup ScrewTheoryLib
 * @brief ...
 */
class ScrewTheoryIkProblem
{
public:

    ~ScrewTheoryIkProblem();

    bool solve(const KDL::Frame & H_S_T, std::vector<KDL::JntArray> & solutions);

    static ScrewTheoryIkProblem * create(const std::vector<ScrewTheoryIkSubproblem *> & steps);

private:

    // disable instantiation, force users to call builder class
    ScrewTheoryIkProblem();

    // disable these too, avoid issues related to dynamic alloc
    ScrewTheoryIkProblem(const ScrewTheoryIkProblem &);
    ScrewTheoryIkProblem & operator=(const ScrewTheoryIkProblem &);

    // we own these, resources freed in destructor
    std::vector<ScrewTheoryIkSubproblem *> steps;
};

/**
 * @ingroup ScrewTheoryLib
 * @brief ...
 */
class ScrewTheoryIkProblemBuilder
{
public:

    ScrewTheoryIkProblemBuilder(const PoeExpression & poe);

    ScrewTheoryIkProblem * build();

private:

    enum poe_term
    {
        EXP_KNOWN,
        EXP_SIMPLIFIED,
        EXP_UNKNOWN
    };

    void searchPoints();

    ScrewTheoryIkSubproblem * trySolve();

    void resetSimplificationState();

    void simplify(int depth);

    PoeExpression poe;

    std::vector<KDL::Vector> points;
    std::vector<KDL::Vector> testPoints;
    std::vector<poe_term> poeTerms;

    static const int MAX_SIMPLIFICATION_DEPTH = 2;

    // TODO: addStep()
};

}  // namespace roboticslab

#endif  // __SCREW_THEORY_IK_PROBLEM_HPP__

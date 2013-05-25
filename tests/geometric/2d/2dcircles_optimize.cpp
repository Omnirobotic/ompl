/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2013, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Ioan Sucan */

#define BOOST_TEST_MODULE "GeometricPlanningOpt"
#include <boost/test/unit_test.hpp>

#include "2DcirclesSetup.h"
#include <iostream>

#include "ompl/geometric/planners/rrt/TRRT.h"
#include "ompl/geometric/planners/prm/PRMstar.h"
#include "ompl/contrib/rrt_star/RRTstar.h"

#include "../../BoostTestTeamCityReporter.h"

using namespace ompl;

static const double SOLUTION_TIME = 1.0;
static const double DT_SOLUTION_TIME = 0.1;
static const bool VERBOSE = true;

/** \brief A base class for testing planners */
class TestPlanner
{
public:
    TestPlanner(void)
    {
    }

    virtual ~TestPlanner(void)
    {
    }

    /* test a planner in a planar environment with circular obstacles */
    void test2DCircles(const Circles2D &circles)
    {
        /* instantiate space information */
        base::SpaceInformationPtr si = geometric::spaceInformation2DCircles(circles);

        /* instantiate problem definition */
        base::ProblemDefinitionPtr pdef(new base::ProblemDefinition(si));	

        // define an objective that is met the moment the solution is found
        base::PathLengthOptimizationObjective *opt = new base::PathLengthOptimizationObjective(si, std::numeric_limits<double>::infinity());
        pdef->setOptimizationObjective(base::OptimizationObjectivePtr(opt));
	
        /* instantiate motion planner */
        base::PlannerPtr planner = newPlanner(si);
        planner->setProblemDefinition(pdef);
        planner->setup();

        base::ScopedState<> start(si);
        base::ScopedState<> goal(si);
        unsigned int good = 0;
        std::size_t nt = std::min<std::size_t>(5, circles.getQueryCount());
        for (std::size_t i = 0 ; i < nt ; ++i)
        {
            const Circles2D::Query &q = circles.getQuery(i);
            start[0] = q.startX_;
            start[1] = q.startY_;
            goal[0] = q.goalX_;
            goal[1] = q.goalY_;
            pdef->setStartAndGoalStates(start, goal, 1e-3);
            planner->clear();
            pdef->clearSolutionPaths();

            // we change the optimization objective so the planner runs until the first solution
            opt->setMaximumUpperBound(std::numeric_limits<double>::infinity());

            time::point start = time::now();
            bool solved = planner->solve(SOLUTION_TIME);
            if (solved)
            {
              // we change the optimization objective so the planner runs until timeout
              opt->setMaximumUpperBound(std::numeric_limits<double>::epsilon());
              
              geometric::PathGeometric *path = static_cast<geometric::PathGeometric*>(pdef->getSolutionPath().get());
              double ini_length = path->length();
              double prev_length = ini_length;
              std::vector<double> lengths;
              double time_spent = time::seconds(time::now() - start);
              
              while (time_spent + DT_SOLUTION_TIME < SOLUTION_TIME)
              {
                pdef->clearSolutionPaths();
                solved = planner->solve(DT_SOLUTION_TIME);
                BOOST_CHECK(solved);
                geometric::PathGeometric *path = static_cast<geometric::PathGeometric*>(pdef->getSolutionPath().get());
                double new_length = path->length();                
                BOOST_CHECK(new_length <= prev_length);
                prev_length = new_length;
                time_spent = time::seconds(time::now() - start);
              }
              BOOST_CHECK(ini_length > prev_length);
            }
        }
    }
    
    virtual base::PlannerPtr newPlanner(const base::SpaceInformationPtr &si) = 0;

};

class RRTstarTest : public TestPlanner
{
protected:

    base::PlannerPtr newPlanner(const base::SpaceInformationPtr &si)
    {
        geometric::RRTstar *rrt = new geometric::RRTstar(si);
        return base::PlannerPtr(rrt);
    }
};

class PRMstarTest : public TestPlanner
{
protected:

    base::PlannerPtr newPlanner(const base::SpaceInformationPtr &si)
    {
        geometric::PRMstar *prm = new geometric::PRMstar(si);
        return base::PlannerPtr(prm);
    }
};

class PlanTest
{
public:

    void run2DCirclesTest(TestPlanner *p)
    {
	p->test2DCircles(circles_);
    }
    
    template<typename T>
    void runAllTests(void)
    {
	TestPlanner *p = new T();
	run2DCirclesTest(p);
	delete p;
    }
    
protected:

    PlanTest(void)
    {
        verbose_ = VERBOSE;
        boost::filesystem::path path(TEST_RESOURCES_DIR);
        circles_.loadCircles((path / "circle_obstacles.txt").string());
        circles_.loadQueries((path / "circle_queries.txt").string());
    }

    Circles2D     circles_;
    bool          verbose_;
};

BOOST_FIXTURE_TEST_SUITE(MyPlanTestFixture, PlanTest)

// define boost tests for a planner assuming the naming convention is followed 
#define OMPL_PLANNER_TEST(Name)						\
    BOOST_AUTO_TEST_CASE(geometric_##Name)				\
    {									\
	if (VERBOSE)							\
	    printf("\n\n\n*****************************\nTesting %s ...\n", #Name); \
	runAllTests<Name##Test>();					\
	if (VERBOSE)							\
	    printf("Done with %s.\n", #Name);				\
    }

OMPL_PLANNER_TEST(PRMstar)
OMPL_PLANNER_TEST(RRTstar)

BOOST_AUTO_TEST_SUITE_END()

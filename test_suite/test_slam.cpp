/**
 * \file test_slam.cpp
 *
 * ## Add brief description here ##
 *
 * \author jsola@laas.fr
 * \date 28/04/2010
 *
 *  ## Add a description here ##
 *
 * \ingroup rtslam
 */

// boost unit test includes
#include <boost/test/auto_unit_test.hpp>

// jafar debug include
#include "kernel/jafarDebug.hpp"
#include "kernel/jafarTestMacro.hpp"
#include "jmath/random.hpp"
#include "jmath/matlab.hpp"

#include <iostream>
#include <boost/shared_ptr.hpp>

#include "rtslam/rtSlam.hpp"
#include "rtslam/robotOdometry.hpp"
#include "rtslam/robotConstantVelocity.hpp"
#include "rtslam/robotInertial.hpp"
#include "rtslam/sensorPinHole.hpp"
#include "rtslam/landmarkAnchoredHomogeneousPoint.hpp"
#include "rtslam/landmarkEuclideanPoint.hpp"
#include "rtslam/observationPinHoleAnchoredHomogeneous.hpp"
#include "rtslam/activeSearch.hpp"
#include "rtslam/observationPinHolePoint.hpp"
#include "rtslam/featureAbstract.hpp"
#include "rtslam/rawImage.hpp"

//#include "rtslam/display_qt.hpp"
//#include "image/Image.hpp"

//#include <map>

using namespace jblas;
using namespace jafar::jmath;
using namespace jafar::jmath::ublasExtra;
using namespace jafar::rtslam;
using namespace boost;


void test_slam01() {
	ActiveSearchGrid asGrid(640, 480, 5, 5, 0);
	vec2 imSz;
	imSz(0) = 640; imSz(1) = 480;
	vec4 k;
	vec d(0), c(0);
	k(0) = 320; k(1) = 320; k(2) = 320; k(3) = 320;
	// INIT : 1 map, 2 robs, 3 sens
	world_ptr_t worldPtr(new WorldAbstract());
	map_ptr_t mapPtr(new MapAbstract(100));
	worldPtr->addMap(mapPtr);
	mapPtr->clear();
	robconstvel_ptr_t robPtr1(new RobotConstantVelocity(mapPtr));
	robPtr1->id(robPtr1->robotIds.getId());
	robPtr1->linkToParentMap(mapPtr);
	vec v(13);
	fillVector(v, 0.0);
	robPtr1->state.x(v);
	robPtr1->pose.x(quaternion::originFrame());
	robPtr1->dt_or_dx = 0.1;
	pinhole_ptr_t senPtr11 (new SensorPinHole(robPtr1, MapObject::FILTERED));
	senPtr11->id(senPtr11->sensorIds.getId());
	senPtr11->linkToParentRobot(robPtr1);
	senPtr11->state.clear();
	senPtr11->pose.x(quaternion::originFrame());
	senPtr11->set_parameters(imSz, k, d, c);
//	pinhole_ptr_t senPtr12 (new SensorPinHole(robPtr1, MapObject::FILTERED));
//	senPtr12->id(senPtr12->sensorIds.getId());
//	senPtr12->linkToParentRobot(robPtr1);
//	senPtr12->state.clear();
//	senPtr12->pose.x(quaternion::originFrame());
//	senPtr12->set_parameters(imSz, k, d, c);
//	robodo_ptr_t robPtr2(new RobotOdometry(mapPtr));
//	robPtr2->id(robPtr2->robotIds.getId());
//	robPtr2->linkToParentMap(mapPtr);
//	robPtr2->state.clear();
//	robPtr2->pose.x(quaternion::originFrame());
//	v.resize(6);
//	fillVector(v, 0.1);
//	robPtr2->control = v;
//	pinhole_ptr_t senPtr21 (new SensorPinHole(robPtr2, MapObject::FILTERED));
//	senPtr21->id(senPtr21->sensorIds.getId());
//	senPtr21->linkToParentRobot(robPtr2);
//	senPtr21->state.clear();
//	senPtr21->pose.x(quaternion::originFrame());
//	senPtr21->set_parameters(imSz, k, d, c);

	// Show empty map
	cout << *mapPtr << endl;



	// INIT : complete observations set
	// loop all sensors
	// loop all lmks
	// create sen--lmk observation
	// Temporal loop

	// display::ViewerQt viewerQt;

	for (int t = 1; t <= 2; t++) {

		cout << "\nTIME : " << t << endl;

		// foreach robot
		for (MapAbstract::RobotList::iterator robIter = mapPtr->robotList().begin(); robIter != mapPtr->robotList().end(); robIter++)
		{
			robot_ptr_t robPtr = *robIter;
			cout << "\nROBOT: " << robPtr->id() << endl;

			vec u(robPtr->mySize_control()); // TODO put some real values in u.
			fillVector(u, 0.0);
			robPtr->set_control(u);
			robPtr->move();

			cout << *robPtr << endl;

			// foreach sensor
			for (RobotAbstract::SensorList::iterator senIter = robPtr->sensorList().begin(); senIter != robPtr->sensorList().end(); senIter++)
			{
				sensor_ptr_t senPtr = *senIter;
				cout << "\nSENSOR: " << senPtr->id() << endl;
				cout << *senPtr << endl;

				// get raw-data
				senPtr->acquireRaw() ;

				asGrid.renew();
				// 1. Observe known landmarks
				// foreach observation
				for (SensorAbstract::ObservationList::iterator obsIter = senPtr->observationList().begin(); obsIter != senPtr->observationList().end(); obsIter++)
				{
					observation_ptr_t obsPtr = *obsIter;

					obsPtr->clearEvents();

					// 1a. project
					obsPtr->project();

					// 1b. check visibility
					obsPtr->predictVisibility();
					if (obsPtr->isVisible()){

						// update counter
						obsPtr->counters.nSearch++;

						// Add to tesselation grid for active search
						asGrid.addPixel(obsPtr->expectation.x());

						// 1c. predict appearance
						obsPtr->predictAppearance();

						// 1d. search appearence in raw
						obsPtr->matchFeature(senPtr->getRaw()) ;

						// 1e. if feature is find
						if (obsPtr->getMatchScore()>0.80) {
							obsPtr->counters.nMatch++;
							obsPtr->events.matched = true;
							obsPtr->computeInnovation() ;

							// 1f. if feature is inlier
							if (obsPtr->compatibilityTest(3.0)) { // use 3.0 for 3-sigma or the 5% proba from the chi-square tables.
								obsPtr->counters.nInlier++;
								JFR_DEBUG("P_rsl: " << ublas::project(obsPtr->landmarkPtr()->mapPtr()->filterPtr->P(), obsPtr->ia_rsl, obsPtr->ia_rsl))
								obsPtr->update() ;
								JFR_DEBUG("P_rsl: " << ublas::project(obsPtr->landmarkPtr()->mapPtr()->filterPtr->P(), obsPtr->ia_rsl, obsPtr->ia_rsl))
								obsPtr->events.updated = true;
							} // obsPtr->compatibilityTest(3.0)
						} // obsPtr->getScoreMatchInPercent()>80
					} // obsPtr->isVisible()

					cout << *obsPtr << endl;

				} // foreach observation




				// 2. init new landmarks
				if (mapPtr->unusedStates(LandmarkAnchoredHomogeneousPoint::size())) {

					ROI roi;
					if (asGrid.getROI(roi)){

						feature_ptr_t featPtr(new FeatureAbstract(2));
//						if (ObservationPinHolePoint::detectInRoi(senPtr->getRaw(), roi, featPtr)){
						if (senPtr->getRaw()->detect(RawAbstract::HARRIS, featPtr, &roi)) {
							cout << "Detected pixel: " << featPtr->state.x() << endl;
							cout << "Initializing lmk..." << endl;

							// 2a. create lmk object
							ahp_ptr_t lmkPtr(new LandmarkAnchoredHomogeneousPoint(mapPtr));
							lmkPtr->id(lmkPtr->landmarkIds.getId());
							lmkPtr->linkToParentMap(mapPtr);

							// 2b. create obs object
							// todo make lmk creation dynamic with factories or switch or other.
							obs_ph_ahp_ptr_t obsPtr(new ObservationPinHoleAnchoredHomogeneousPoint(senPtr,lmkPtr));
							obsPtr->linkToParentPinHole(senPtr);
							obsPtr->linkToParentAHP(lmkPtr);

							// 2c. fill data for this obs
							obsPtr->events.visible = true;
							obsPtr->events.measured = true;
							vec measNoiseStd(2);
							fillVector(measNoiseStd, 1.0);
							obsPtr->setup(measNoiseStd, ObservationPinHoleAnchoredHomogeneousPoint::getPrior());
							obsPtr->measurement.x(featPtr->state.x());

							// 2d. comute and fill data for the landmark
							obsPtr->backProject();

							// 2e. Create descriptor
							vec7 globalSensorPose = senPtr->globalPose();
							lmkPtr->createDescriptor(featPtr->appearancePtr, globalSensorPose);

							// Complete SLAM graph with all other obs
							mapPtr->completeObservationsInGraph(senPtr, lmkPtr);

							cout << *lmkPtr << endl;
						}
					}
				}

			}
		}

		//viewerQt.bufferize(worldPtr);
		//viewerQt.render();
	}


}


BOOST_AUTO_TEST_CASE( test_slam )
{
	test_slam01();
}

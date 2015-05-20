/*
 * ElevatorLogic.h
 *
 *  Created on: 20.06.2014
 *      Author: STSJR
 */

#ifndef ELEVATORLOGIC_H_
#define ELEVATORLOGIC_H_

#include "EventHandler.h"

#include <list>
#include <map>
#include <set>

class Elevator;
class Floor;
class Interface;

class ElevatorLogic: public EventHandler
{

public:
	ElevatorLogic();
	virtual ~ElevatorLogic();

	void Initialize(Environment &env);

private:

	void HandleNotify(Environment &env, const Event &e);
	void HandleStopped(Environment &env, const Event &e);
	void HandleOpened(Environment &env, const Event &e);
	void HandleClosed(Environment &env, const Event &e);
	
	// Get and process status info on elevator
	void HandleMoving(Environment &env, const Event &e);
	
	// Send elevator to given floor
	void SendToFloor(Environment &env, Floor*,Elevator*);
	
	bool moved_;
};

#endif /* ELEVATORLOGIC_H_ */

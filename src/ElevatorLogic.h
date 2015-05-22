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

 using namespace std;

class Elevator;
class Floor;
class Interface;
class Person;

class ElevatorLogic: public EventHandler
{

public:
	// state of an elevator door
	// initially Closed
	enum DoorState
	{
		Closing,
		Closed,
		Opening,
		Open	
	};

	// state of an elevator
	// initially isMoving == false
	// initially passengers.empty() == true
	typedef struct
	{
		DoorState doorState;
		bool isMoving;
		set<Person*> passengers;
	} State;

	ElevatorLogic();
	virtual ~ElevatorLogic();

	void Initialize(Environment &env);

private:

	void HandleNotify(Environment &env, const Event &e);
	void HandleStopped(Environment &env, const Event &e);
	void HandleOpened(Environment &env, const Event &e);
	void HandleClosed(Environment &env, const Event &e);
	
	// get and process status info on elevator
	void HandleMoving(Environment &env, const Event &e);
	
	/*** internal functions ***/
	// send elevator to given floor
	void SendToFloor(Environment &env, Floor*, Elevator*);
	// open a given elevators door
	void openDoor(Environment &env, int, Elevator*);

	// states of all elevators we already handled
	// initially empty
	map<Elevator*,State> elevatorState_;

	bool moved_;

};

#endif /* ELEVATORLOGIC_H_ */

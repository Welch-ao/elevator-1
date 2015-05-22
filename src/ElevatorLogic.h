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
		Opened	
	};

	// state of an elevator
	// initially isMoving == false
	// initially passengers.empty() == true
	typedef struct
	{
		bool busy;
		DoorState doorState;
		bool isMoving;
		set<Person*> passengers;
		bool isBeeping;
	} State;

	#define DEFAULT_STATE {false,Closed,false,passengers,false}

	ElevatorLogic();
	virtual ~ElevatorLogic();

	void Initialize(Environment &env);

private:

	void HandleNotify(Environment &env, const Event &e);
	void HandleStopped(Environment &env, const Event &e);
	void HandleOpening(Environment &env, const Event &e);
	void HandleOpened(Environment &env, const Event &e);
	void HandleClosing(Environment &env, const Event &e);
	void HandleClosed(Environment &env, const Event &e);
	
	// get and process status info on elevator
	void HandleMoving(Environment &env, const Event &e);
	// TODO:
	// void HandleBeeping(Environment &env, const Event &e);
	// void HandleBeeped(Environment &env, const Event &e);



	/*** internal functions ***/
	// send elevator to given floor
	void SendToFloor(Environment &env, Floor*, Elevator*);
	// open a given elevators door
	void openDoor(Environment &env, int, Elevator*);
	// close a given elevators door
	void closeDoor(Environment &env, int, Elevator*);


	// states of all elevators we already handled
	// initially empty
	map<Elevator*,State> elevatorState_;

	bool moved_;

};

#endif /* ELEVATORLOGIC_H_ */

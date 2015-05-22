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

	// we track all the objects we interact with
	// state of an elevator door
	enum DoorState
	{
		Closing,
		Closed,
		Opening,
		Opened	
	};

	// state of an elevator
	typedef struct
	{
		bool busy;
		DoorState doorState;
		bool isMoving;
		set<Person*> passengers;
		bool isBeeping;
	} ElevatorState;

	// default state for an elevator
	#define DEFAULT_STATE {false,Closed,false,passengers,false}

	typedef struct
	{
		int timer;
	} PersonState;


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

	// react to people entering and leaving
	void HandleEntered(Environment &env, const Event &e);
	void HandleExited(Environment &env, const Event &e);

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
	typedef map<Elevator*,ElevatorState> ElevatorMap;
	ElevatorMap elevators_;

	typedef map<Person*,PersonState> PersonMap;
	PersonMap persons_;
	bool moved_;

};

#endif /* ELEVATORLOGIC_H_ */

/*
 * ElevatorLogic.h
 *
 *  Created on: 20.06.2014
 *      Author: STSJR
 */

#ifndef ELEVATORLOGIC_H_
#define ELEVATORLOGIC_H_


// debug macros
// @see http://stackoverflow.com/questions/14251038/debug-macros-in-c?lq=1

#define DEBUGGING
#ifdef DEBUGGING
	#define debugging_output true

	#define DEBUG(x) x
	#define DEBUG_S(x) do \
	 	{ \
			if (debugging_output) \
		  	{ std::cerr << "\tDEBU: " << x << std::endl; } \
		} while (0)

	// the #x puts "x" and is call stringizing operator
	// @see https://msdn.microsoft.com/en-us/library/7e3a913x.aspx
	#define DEBUG_V(x) do \
		{ \
			if (debugging_output) \
			{ std::cerr << "\tDEBU " << #x << ": " << x << std::endl; } \
		} while (0)
#else
	#define DEBUG(x)
	#define DEBUG_S(x)
	#define DEBUG_V(x)
#endif


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
	// here we track all the entities we work with
	/*** ELEVATOR ***/
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

	// default state of an elevator
	#define ELEV_DEFAULT_STATE {false,Closed,false,passengers,false}

	/*** PERSON ***/

	// state of a person
	typedef struct
	{
		int timer;
	} PersonState;

	// default state of a person
	#define PERS_DEFAULT_STATE(timer) {timer}

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

	// handle entering and exiting persons
	void HandleEntered(Environment &env, const Event &e);
	void HandleExited(Environment &env, const Event &e);

	// TODO:
	// collect persons on the way.

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
	map<Elevator*,ElevatorState> elevators_;

	// states of all persons we know
	map<Person*,PersonState> persons_;
};

#endif /* ELEVATORLOGIC_H_ */

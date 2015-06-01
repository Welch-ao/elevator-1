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
	 		ostringstream out; \
			if (debugging_output) \
		  	{ std::cout << "\tDEBUG: " << x << std::endl; \
		  	  out << "\tDEBUG: " << x << "<br>" << std::endl; } \
		  	eventlog.append(out.str()); \
		} while (0)

	#define DEBUG_E(x) do \
	 	{ \
			if (debugging_output) \
		  	{ std::cerr << "\tERROR: " << x << std::endl; } \
		} while (0)

	// the #x puts "x" and is call stringizing operator
	// @see https://msdn.microsoft.com/en-us/library/7e3a913x.aspx
	#define DEBUG_V(x) do \
		{ \
			if (debugging_output) \
			{ std::cout << "\tDEBUG " << #x << ": " << x << std::endl; } \
		} while (0)
#else
	#define DEBUG(x)
	#define DEBUG_S(x)
	#define DEBUG_E(x)
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

	typedef list<Floor*> elevatorQueue;

	// state of an elevator
	typedef struct
	{
		bool busy;
		DoorState doorState;
		bool isMoving;
		set<Person*> passengers;
		elevatorQueue queue;
		bool isBeeping;
	} ElevatorState;

	// default state of an elevator
	#define ELEV_DEFAULT_STATE {false,Closed,false,passengers,queue,false}

	ElevatorLogic();
	virtual ~ElevatorLogic();

	void Initialize(Environment &env);

private:

	/*** event handlers ***/
	void HandleNotify(Environment &env, const Event &e);
	void HandleStopped(Environment &env, const Event &e);
	void HandleOpening(Environment &env, const Event &e);
	void HandleOpened(Environment &env, const Event &e);
	void HandleClosing(Environment &env, const Event &e);
	void HandleClosed(Environment &env, const Event &e);
	// get and process status info on elevator
	void HandleMoving(Environment &env, const Event &e);
	void HandleUp(Environment &env, const Event &e);
	void HandleDown(Environment &env, const Event &e);
	// handle entering and exiting persons
	void HandleEntering(Environment &env, const Event &e);
	void HandleExiting(Environment &env, const Event &e);
	void HandleEntered(Environment &env, const Event &e);
	void HandleExited(Environment &env, const Event &e);


	/*** helper functions ***/
	// send elevator to given floor
	void SendToFloor(Environment &env, Floor*, Elevator*);
	// open a given elevators door
	void openDoor(Environment &env, Elevator*);
	// close a given elevators door
	void closeDoor(Environment &env, Elevator*);
	// add a floor and a person which wants to go there
	// to an elevators queue
	void addToQueue(Elevator*,Floor*);
	// get free space of given elevator
	int getCapacity(Elevator* const);
	// check if elevator is on the way to given floor
	bool onTheWay(Elevator*,Floor*);
	// calculate the distance from one floor's relative position to a different floor stop
	double getDistance(Floor*,double,Floor*);
	// calculate travel time for given elevator from one floor to the other
	int getTravelTime(Elevator*,Floor*,Floor*);

	// states of all elevators we already handled
	map<Elevator*,ElevatorState> elevators_;



	DEBUG
	(// debugging stuff
	set<Floor*> allFloors;
	string eventlog;
	set<Interface*> allInterfaces;
	string showFloors();
	string showPersons();
	string showElevators();
	string showInterfaces();
	int tick;
	);

	// we cant put that into DEBUG() since there are commas...
	// person with start floor and start time
	map<Person*,pair<int,int>> allPersons;
	// elevator with starting floor
	map<Elevator*,int> allElevators;
	// persons with their deadlines
	std::map<Person*,int> deadlines_;

	DEBUG
	(
		void HandleAll(Environment &env, const Event &e);
		void checkTimer(Environment&, Person *person);
		void collectInfo(Environment&, Person*);
		void logEvent(Environment&, const Event&);
	);

};

#endif /* ELEVATORLOGIC_H_ */

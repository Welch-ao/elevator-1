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
			{ cout << "\tDEBUG: " << x << endl; \
			  out << "\tDEBUG: " << x << "<br>" << endl; } \
			eventlog.append(out.str()); \
		} while (0)

	// the #x puts "x" and is call stringizing operator
	// @see https://msdn.microsoft.com/en-us/library/7e3a913x.aspx
	#define DEBUG_V(x) do \
		{ \
			if (debugging_output) \
			{ cout << "\tDEBUG " << #x << ": " << x << endl; } \
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

class Elevator;
class Floor;
class Interface;
class Person;

using namespace std;

class ElevatorLogic: public EventHandler
{
	public:
		ElevatorLogic();
		virtual ~ElevatorLogic();

		void Initialize(Environment &env);

	private:
		void HandleNotify     	(Environment &env, const Event &e);
		void HandleInteract   	(Environment &env, const Event &e);
		void HandleMoving     	(Environment &env, const Event &e);
		void HandleStopped    	(Environment &env, const Event &e);
		void HandleOpening    	(Environment &env, const Event &e);
		void HandleOpened     	(Environment &env, const Event &e);
		void HandleClosing    	(Environment &env, const Event &e);
		void HandleClosed     	(Environment &env, const Event &e);
		void HandleEntering   	(Environment &env, const Event &e);
		void HandleEntered    	(Environment &env, const Event &e);
		void HandleExiting    	(Environment &env, const Event &e);
		void HandleExited     	(Environment &env, const Event &e);
		void HandleBeeping    	(Environment &env, const Event &e);
		void HandleBeeped     	(Environment &env, const Event &e);
		void HandleMalfunction	(Environment &env, const Event &e);
		void HandleFixed      	(Environment &env, const Event &e);
		void HandleAll        	(Environment &env, const Event &e);


		// helper functions
		Elevator*	pickElevator     	(Interface*, Floor*);
		int      	getQueueLength   	(Elevator*, Floor*);
		int      	getTravelTime    	(Elevator*, Floor*, Floor*, bool direct = true);
		double   	getDistance      	(Floor*, Floor*, double pos = 0.5);
		void     	addToList        	(list<Elevator*>&, Elevator*, Floor*);
		bool     	onTheWay         	(Elevator*, Floor*);
		void     	sendToFloor      	(Environment&, Elevator*, Floor*);
		void     	continueOperation	(Environment&, Elevator*);
		bool     	hasUpQueue       	(Elevator*);
		bool     	hasDownQueue     	(Elevator*);
		bool     	inPosition       	(Elevator*);


		// elevator states
		map<Elevator*,set<Floor*>>	queueInt_;
		map<Elevator*,set<Floor*>>	queueExt_;
		set<Elevator*>            	elevators_;
		set<Elevator*>            	moving_;
		set<Elevator*>            	movingUp_;
		set<Elevator*>            	movingDown_;
		set<Elevator*>            	open_;
		set<Elevator*>            	busy_;
		set<Elevator*>            	beeping_;
		set<Elevator*>            	malfunctions_;
		map<Elevator*,int>        	loads_;
		map<Person*,int>          	deadlines_;

		int time_;	// last time we checked

		// environment information
		set<Floor*>               	allFloors;
		set<Interface*>           	allInterfaces;
		map<Person*,pair<int,int>>	allPersons;  	// persons with start floor and start time
		map<Elevator*,int>        	allElevators;	// elevator with starting floor
		list<Event>               	allEvents;   	// all malfunction and fixed events

		string eventlog;

		void collectInfo(Environment&, Person*);
		void logEvent(Environment&, const Event&);
		string showFloors();
		string showPersons();
		string showElevators();
		string showInterfaces();
		string showEvents();
		string showTestCase();
};

#endif /* ELEVATORLOGIC_H_ */

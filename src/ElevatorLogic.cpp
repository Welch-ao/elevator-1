/*
 * ElevatorLogic.cpp
 *
 *  Created on: 20.06.2014
 *      Author: STSJR
 */

#include "ElevatorLogic.h"

#include <iostream>

#include "Interface.h"
#include "Person.h"
#include "Floor.h"
#include "Elevator.h"
#include "Event.h"
#include "Environment.h"

ElevatorLogic::ElevatorLogic() : EventHandler("ElevatorLogic") {}

ElevatorLogic::~ElevatorLogic() {}

// register all the handlers
void ElevatorLogic::Initialize(Environment &env)
{
	env.RegisterEventHandler("Interface::Notify", this, &ElevatorLogic::HandleNotify);
	env.RegisterEventHandler("Elevator::Moving", this, &ElevatorLogic::HandleMoving);
	env.RegisterEventHandler("Elevator::Stopped", this, &ElevatorLogic::HandleStopped);
	env.RegisterEventHandler("Elevator::Opening", this, &ElevatorLogic::HandleOpening);
	env.RegisterEventHandler("Elevator::Opened", this, &ElevatorLogic::HandleOpened);
	env.RegisterEventHandler("Elevator::Closing", this, &ElevatorLogic::HandleClosing);
	env.RegisterEventHandler("Elevator::Closed", this, &ElevatorLogic::HandleClosed);
	env.RegisterEventHandler("Person::Entered", this, &ElevatorLogic::HandleEntered);
	env.RegisterEventHandler("Person::Exited", this, &ElevatorLogic::HandleExited);
}

// What to do after recieving notification
// TODO: clean up decision on what to do for the elevator
void ElevatorLogic::HandleNotify(Environment &env, const Event &e)
{
	// grab interface that sent notification
	// notification can only come from an interface, so this cast is reasonable
	Interface *interf = static_cast<Interface*>(e.GetSender());
	// grab the person that interacted with the interface
	// reference of a notification can only be a person
	Person *person = static_cast<Person*>(e.GetEventHandler());

	DEBUG_S
	(
		"Person " << person->GetId() <<
		" (on Floor " << person->GetCurrentFloor()->GetId() << ")" <<
		" wants to go to floor " << person->GetFinalFloor()->GetId()
	);

	// add person to tracked persons
	// does nothing if already tracked
	int timer = person->GetGiveUpTime();
	pair<Person*,PersonState> personState = {person,PERS_DEFAULT_STATE(timer)};
	persons_.insert(personState);

	// check if interface comes from inside an elevator or from a floor
	// we see this from looking at the type of the interface's first loadable
	Loadable *loadable = interf->GetLoadable(0);

	// react to an interface interaction from outside the elevator
	if (loadable->GetType() == "Elevator")
	{
		DEBUG
		(
			// Get person timer
			timer = person->GetGiveUpTime() - env.GetClock();
			if (timer > 0)
			{
				DEBUG_S("Person " << person->GetId() << " waits " << timer);
			}
			else if (timer == 0)
			{
				DEBUG_S("Person " << person->GetId() << " GIVES UP!");
			}
		);

		// get all elevators that stop at this interface
		list<Elevator*> elevs;
		for(int i = 0; i < interf->GetLoadableCount(); i++)
		{
			// cast the loadables to elevator pointers
			Elevator *ele = static_cast<Elevator*>(interf->GetLoadable(i));
			elevs.push_back(ele);

			// while we're at it, add these elevators to our global map
			// using default values for its state:
			// empty passenger list
			set<Person*> passengers;
			// closed door and not moving
			pair<Elevator*,ElevatorState> elevState = {ele,ELEV_DEFAULT_STATE};

			// put it into the global map (doesn't do anything if already exists)
			elevators_.insert(elevState);
		}

		// check if any elevator is already at the calling person
		for(list<Elevator*>::iterator i = elevs.begin(); i != elevs.end(); ++i)
		{
			// take the first elevator that is at the right floor
			if ((*i)->GetCurrentFloor() == person->GetCurrentFloor())
			{
				// let the person in
				elevators_[*i].busy = true;
				openDoor(env, 0, *i);
				return;
			}
		}

		// if there is no elevator at the calling persons floor
		for(list<Elevator*>::iterator i = elevs.begin(); i != elevs.end(); ++i)
		{
			// take the first idle elevator that goes to that floor
			if ((*i)->HasFloor(person->GetFinalFloor()) && !elevators_[*i].busy)
			{
				// go to the caller's floor
				elevators_[*i].busy = true;
				SendToFloor(env,person->GetCurrentFloor(),*i);
				return;
			}
		}

		// if none can come, try again next tick
		env.SendEvent("Interface::Notify",1,interf,person);
	}
	// react to an interface interaction from inside the elevator
	else
	{
		// get our current elevator by asking the person where it is
		Elevator *ele = person->GetCurrentElevator();
		// get target from the interface input
		Floor *target = static_cast<Floor*>(loadable);

		// send elevator to where the person wants to go
		SendToFloor(env,target,ele);
	}
}

// open doors immediately after stopping in the middle of a floor
void ElevatorLogic::HandleStopped(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	// set this elevator to moving state
	elevators_[ele].isMoving = false;

	// only open doors if we're at the middle of a floor
	if (ele->GetPosition() > 0.49 && ele->GetPosition() < 0.51)
	{
		env.SendEvent("Elevator::Open", 0, this, ele);
	}
}

// update elevator state when something happens with the door
void ElevatorLogic::HandleOpening(Environment &env, const Event &e)
{
	Elevator* ele = static_cast<Elevator*>(e.GetSender());
	elevators_[ele].doorState = Opening;
}

void ElevatorLogic::HandleOpened(Environment &env, const Event &e)
{
	Elevator* ele = static_cast<Elevator*>(e.GetSender());
	elevators_[ele].doorState = Opened;
}

void ElevatorLogic::HandleClosing(Environment &env, const Event &e)
{
	Elevator* ele = static_cast<Elevator*>(e.GetSender());
	elevators_[ele].doorState = Closing;
}

void ElevatorLogic::HandleClosed(Environment &env, const Event &e)
{
	Elevator* ele = static_cast<Elevator*>(e.GetSender());
	elevators_[ele].doorState = Closed;
}

// react to elevators movement status
void ElevatorLogic::HandleMoving(Environment &env, const Event &e)
{
	// NOTE: we assume the sender of a movement event is always an elevator
	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	// set this elevator to moving state
	elevators_[ele].isMoving = true;

	// get current floor id we're at
	int floor = ele->GetCurrentFloor()->GetId();

	// FYI
	std::string state = "State unknown";
	switch (ele->GetState())
	{
		// this should not happen
		case 0:
			state = "idle";
			break;
		case 1:
			state = "moving UP";
			break;
		case 2:
			state = "moving DOWN";
			break;
		// this should absolutely never happen
		case 3:
			state = "MALFUNCTION!!!";
			break;
		default:
			break;
	}

	DEBUG_S
	(
		"Current floor: " << floor << " (" << ele->GetPosition() << "), " << state
	);

	// NOTE: when we start moving the elevator, it sends a first notification automatically which has no data.
	if(!e.GetData().empty())
	{
		// get the floor id we want to reach
		int tgt_floor = std::stoi(e.GetData());

		// FYI
		DEBUG_S
		(
			" to floor " << tgt_floor
		);

		// stop if we're at the middle of the target floor
		if (floor == tgt_floor && ele->GetPosition() > 0.49 && ele->GetPosition() < 0.51)
		{
			env.SendEvent("Elevator::Stop",0,ele,ele);
		}
		// otherwise continue movement and report back later
		else
		{
			env.SendEvent("Elevator::Moving",1,ele,ele,std::to_string(tgt_floor));
		}
	}
}

// send elevator into general direction of given floor
// WARNING: does not check if floor is reachable.
void ElevatorLogic::SendToFloor(Environment &env, Floor *target, Elevator *ele)
{
	// find out current floor
	Floor *current = ele->GetCurrentFloor();

	DEBUG_S("Closing door");
	// close door before starting
	closeDoor(env,0,ele);

	// TODO: check again after 3 ticks if starting movement is already appropriate
	// send into right direction
	// WARNING: delay hardcoded for time to close doors!
	if (target->IsBelow(current))
	{
		env.SendEvent("Elevator::Up",3,this,ele);
	}
	else
	{
		env.SendEvent("Elevator::Down",3,this,ele);
	}

	/*
	(evil hack! we're misusing our ability to let the elevator send a message
	and pretend it does so on its own. the only reason is that (AFAIK) we can't
	do anything else to have a read-only interaction, since we can't declare
	new events for this class)
	 */

	// pass the target floor id as data string
	env.SendEvent("Elevator::Moving",4,ele,ele,std::to_string(target->GetId()));
	// @see HandleMoving
}


void ElevatorLogic::openDoor(Environment &env, int delay, Elevator* ele)
{
	// only open door if stopped in the middle of a floor
	bool stoppedProperly = !elevators_[ele].isMoving && ele->GetPosition() > 0.49 && ele->GetPosition() < 0.51;
	// only open door if it is already closed or currently closing
	bool doorClosed = elevators_[ele].doorState == Closed || elevators_[ele].doorState == Closing;

	if (stoppedProperly && doorClosed)
	{
		env.SendEvent("Elevator::Open", delay, this, ele);
	}
	// TODO: If door is closing, delay everything somehow until the door is open.
}

void ElevatorLogic::closeDoor(Environment &env, int delay, Elevator* ele)
{
	// only open door if stopped in the middle of a floor
	bool beeping = elevators_[ele].isBeeping;
	// only open door if it is already closed or currently closing
	bool doorOpen = elevators_[ele].doorState == Opened || elevators_[ele].doorState == Opening;

	if (!beeping && doorOpen)
	{
		env.SendEvent("Elevator::Close", 0, this, ele);
	}
}


void ElevatorLogic::HandleEntered(Environment &env, const Event &e)
{
	Person *person = static_cast<Person*>(e.GetSender());
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());

	elevators_[ele].passengers.insert(person);
	elevators_[ele].busy = true;
}
void ElevatorLogic::HandleExited(Environment &env, const Event &e)
{
	Person *person = static_cast<Person*>(e.GetSender());
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());

	elevators_[ele].passengers.erase(person);

	// free elevator from business after last person exited
	if (elevators_[ele].passengers.empty())
	{
		elevators_[ele].busy = false;
	}
}

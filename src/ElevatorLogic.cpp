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
	env.RegisterEventHandler("Interface::Notify",	this, &ElevatorLogic::HandleNotify);
	env.RegisterEventHandler("Elevator::Moving",	this, &ElevatorLogic::HandleMoving);
	env.RegisterEventHandler("Elevator::Up",		this, &ElevatorLogic::HandleUp);
	env.RegisterEventHandler("Elevator::Down",		this, &ElevatorLogic::HandleDown);
	env.RegisterEventHandler("Elevator::Stopped",	this, &ElevatorLogic::HandleStopped);
	env.RegisterEventHandler("Elevator::Opening",	this, &ElevatorLogic::HandleOpening);
	env.RegisterEventHandler("Elevator::Opened",	this, &ElevatorLogic::HandleOpened);
	env.RegisterEventHandler("Elevator::Closing",	this, &ElevatorLogic::HandleClosing);
	env.RegisterEventHandler("Elevator::Closed",	this, &ElevatorLogic::HandleClosed);
	env.RegisterEventHandler("Person::Entered",		this, &ElevatorLogic::HandleEntered);
	env.RegisterEventHandler("Person::Exited",		this, &ElevatorLogic::HandleExited);
	env.RegisterEventHandler("Person::Entering",	this, &ElevatorLogic::HandleEntering);
	env.RegisterEventHandler("Person::Exiting",		this, &ElevatorLogic::HandleExiting);
}

// What to do after recieving notification
// TODO: clean up decision on what to do for the elevator
void ElevatorLogic::HandleNotify(Environment &env, const Event &e)
{
	// grab interface that sent notification
	// notification can only come from an interface
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

	// check if interface comes from inside an elevator or from a floor
	// we see this from looking at the type of the interface's first loadable
	Loadable *loadable = interf->GetLoadable(0);

	// react to an interface interaction from outside the elevator
	if (loadable->GetType() == "Elevator")
	{
		DEBUG
		(
			// get person's timer
			int timer = person->GetGiveUpTime() - env.GetClock();
			if (timer > 0)
			{
				DEBUG_S("Person " << person->GetId() << " waits " << timer);
			}
			else if (timer == 0)
			{
				DEBUG_S("Person " << person->GetId() << " GIVES UP!");
			}
		);

		// get all elevators that stop at this floor
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
			// empty queue
			elevatorQueue queue;
			// closed door and not moving
			pair<Elevator*,ElevatorState> elevState = {ele,ELEV_DEFAULT_STATE};

			// put it into the global map (doesn't do anything if already exists)
			elevators_.insert(elevState);
		}

		// check if any elevator is already at the calling person
		Floor *personsFloor = person->GetCurrentFloor();
		for(list<Elevator*>::iterator i = elevs.begin(); i != elevs.end(); ++i)
		{
			// check if space left
			if (getCapacity(*i) - person->GetWeight() >= 0 && !elevators_[*i].busy)
			{
				SendToFloor(env,personsFloor,*i);
				return;
			}
			DEBUG
			(
				else if (getCapacity(*i) - person->GetWeight() < 0)
				{
					DEBUG_S("No capacity left for Elevator " << (*i)->GetId());
				}
				else if (elevators_[*i].busy)
				{
					DEBUG_S("Elevator " << (*i)->GetId() << " is busy.");
				}
			);
		// if none can come, try again next tick
		}
		env.SendEvent("Interface::Notify",1,interf,person);
	}
	// react to an interface interaction from inside the elevator
	else
	{
		// get our current elevator by asking the person where it is
		Elevator *ele = person->GetCurrentElevator();
		// get target from the interface input
		Floor *target = static_cast<Floor*>(loadable);
		elevators_[ele].busy = false;
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
	DEBUG
	(
		else
		{
			DEBUG_S("ERROR: Elevator stopped at Floor " << ele->GetCurrentFloor()->GetId() << " in illegal position! What now?");
		}
	);
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
	// remove floor from queue where we just opened the door
	elevators_[ele].queue.remove(ele->GetCurrentFloor());
	DEBUG_S("Removed floor " << ele->GetCurrentFloor()->GetId() << " from queue.");
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

void ElevatorLogic::HandleUp(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
	elevators_[ele].isMoving = true;
	elevators_[ele].busy = false;

}

void ElevatorLogic::HandleDown(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
	elevators_[ele].isMoving = true;
	elevators_[ele].busy = false;
}

// react to elevators movement status
void ElevatorLogic::HandleMoving(Environment &env, const Event &e)
{
	/*
	(evil hack! we're misusing our ability to let the elevator send a message
	and pretend it does so on its own. the only reason is that (AFAIK) we can't
	do anything else to have a read-only interaction, since we can't declare
	new events for this class)
	 */

	// NOTE: we assume the sender of a movement event is always an elevator
	Elevator *ele = static_cast<Elevator*>(e.GetSender());
	DEBUG
	(
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
			"Current floor: " << ele->GetCurrentFloor()->GetId() << " (" << ele->GetPosition() << "), " << state
		);
		if (elevators_[ele].doorState != Closed)
		{
			DEBUG_S("OMAGAHHH OPEN DOOOOR!!!");
		}
	);

	// NOTE: when we start moving the elevator, it sends a first notification automatically which has no data.

	// stop if we're at the middle of any floor in the queue
	elevatorQueue queue = elevators_[ele].queue;
	elevatorQueue::iterator i = queue.begin();
	if (queue.empty())
	{DEBUG_S("no floor in queue");}
	for(; i != queue.end(); ++i)
	{
		if (*i == ele->GetCurrentFloor() && ele->GetPosition() > 0.49 && ele->GetPosition() < 0.51)
		{
			env.SendEvent("Elevator::Stop",0,ele,ele);
			return;
		}
	}

	// otherwise continue movement and report back later
	env.SendEvent("Elevator::Moving",1,ele,ele);
}

// send elevator into general direction of given floor
// WARNING: does not check if floor is reachable.
void ElevatorLogic::SendToFloor(Environment &env, Floor *target, Elevator *ele)
{
	// find out current floor
	Floor *current = ele->GetCurrentFloor();

	addToQueue(ele,target);
	if (current == target)
	{
		openDoor(env,ele);
		elevators_[ele].busy = true;
		return;
	}
	if (elevators_[ele].isMoving)
	{
		DEBUG_S("Already moving, do nothing");
		return;
	}
	elevators_[ele].isMoving = true;

	// TODO: check again after 3 ticks if starting movement is already appropriate

	// adjust delay depending on door state
	// TODO: make it more elegant
	int delay;
	if (elevators_[ele].doorState == Closed)
	{
		DEBUG_S("Door closed, we're good to go.");
		delay = 0;
	}
	else if (elevators_[ele].doorState == Closing)
	{
		DEBUG_S("Door still closing, wait 3 ticks.");
		delay = 3;
	}
	else
	{
		DEBUG_S("Door opened, we should close it.");
		// close door before starting
		closeDoor(env,ele);
		delay = 3;
	}
	// send into right direction
	if (target->IsBelow(current))
	{
		DEBUG_S("Target Floor " << target->GetId() << " is above elevator's current floor " << ele->GetCurrentFloor()->GetId());
		env.SendEvent("Elevator::Up",delay,this,ele);
	}
	else
	{
		DEBUG_S("Target Floor " << target->GetId() << " is below elevator's current floor " << ele->GetCurrentFloor()->GetId());
		env.SendEvent("Elevator::Down",delay,this,ele);
	}
}

void ElevatorLogic::openDoor(Environment &env, Elevator* ele)
{
	// only open door if stopped in the middle of a floor
	bool stoppedProperly = !elevators_[ele].isMoving && ele->GetPosition() > 0.49 && ele->GetPosition() < 0.51;
	// only open door if it is already closed or currently closing
	bool doorClosed = elevators_[ele].doorState == Closed || elevators_[ele].doorState == Closing;

	if (stoppedProperly && doorClosed)
	{
		env.SendEvent("Elevator::Open", 0, this, ele);
	}
	// TODO: If door is closing, delay everything somehow until the door is open.
}

void ElevatorLogic::closeDoor(Environment &env, Elevator* ele)
{
	// only open door if stopped in the middle of a floor
	bool beeping = elevators_[ele].isBeeping;
	// only open door if it is already closed or currently closing
	bool doorOpen = elevators_[ele].doorState == Opened || elevators_[ele].doorState == Opening;
	// check if someone is entering or exiting
	bool busy = elevators_[ele].busy;
	if (!beeping && doorOpen && !busy)
	{
		DEBUG_S("Closing door");
		env.SendEvent("Elevator::Close", 0, this, ele);
	}
	else
	{
		DEBUG_S("Door blocked, trying again...");
		env.SendEvent("Elevator::Close", 1, this, ele);
	}
}

void ElevatorLogic::HandleEntered(Environment &env, const Event &e)
{
	Person *person = static_cast<Person*>(e.GetSender());
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());

	elevators_[ele].passengers.insert(person);
	elevators_[ele].busy = false;
}

void ElevatorLogic::HandleExited(Environment &env, const Event &e)
{
	Person *person = static_cast<Person*>(e.GetSender());
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());

	elevators_[ele].passengers.erase(person);

	elevators_[ele].busy = false;

	// after someone left, go to lowest floor in queue
	// WARNING: just a dummy, we should actually continue our path
	if (!elevators_[ele].queue.empty())
	{
		SendToFloor(env,elevators_[ele].queue.front(),ele);
	}
}

void ElevatorLogic::HandleEntering(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
	elevators_[ele].busy = true;
}

void ElevatorLogic::HandleExiting(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
	elevators_[ele].busy = true;
}


// add a floor to an elevator's queue
/**
 * NOTE: the queue could as well be implemented with a set and a custom
 * comparison function that uses `IsAbove()` and `IsBelow()`
 */
void ElevatorLogic::addToQueue(Elevator *ele, Floor *target)
{
	// get the elevator's queue
	elevatorQueue &q = elevators_[ele].queue;

	// if queue empty, just add the new floor together with person to queue
	if (q.empty())
	{
		q.push_front(target);
		DEBUG_S("Added floor " << target->GetId() << " to queue of Elevator " << ele->GetId());
		return;
	}

	// otherwise find the right position to insert
	// NOTE: queue is sorted by floors ordered from lowest to highest
	elevatorQueue::iterator i = q.begin();
	for(; i != q.end(); ++i)
	{
		// if floor is already in queue
		if ((*i) == target)
		{
			// do nothing
			DEBUG_S("Floor " << target->GetId() << " already in queue of Elevator " << ele->GetId());
			return;
		}
		// if queued floor is above given floor
		else if ((*i)->IsAbove(target))
		{
			// the insert happens after the loop anyway
			break;
		}
	}
	// if new floor is above all others, insert at the end
	q.insert(i,target);
	DEBUG_S("Added floor " << target->GetId() << " to queue of Elevator " << ele->GetId());
}

// get free space of given elevator
int ElevatorLogic::getCapacity(Elevator *ele)
{
	// start with max load
	int capacity = ele->GetMaxLoad();

	set<Person*> passengers = elevators_[ele].passengers;
	set<Person*>::iterator i = passengers.begin();
	// subtract weight of each person on board
	for(; i != passengers.end(); ++i)
	{
		capacity -= (*i)->GetWeight();
	}
	return capacity;
}

// check if elevator is on the way to given floor
bool ElevatorLogic::onTheWay(Elevator *ele,Floor *target)
{
	return
	(
		(ele->GetState() == Elevator::Up && ele->GetCurrentFloor()->IsAbove(target)) ||
		(ele->GetState() == Elevator::Down && ele->GetCurrentFloor()->IsBelow(target))
	);
}
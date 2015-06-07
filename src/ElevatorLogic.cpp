/*
 * ElevatorLogic.cpp
 *
 *  Created on: 20.06.2014
 *      Author: STSJR
 */

#include "ElevatorLogic.h"

#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>

#include "Interface.h"
#include "Person.h"
#include "Floor.h"
#include "Elevator.h"
#include "Event.h"
#include "Environment.h"

ElevatorLogic::ElevatorLogic() : EventHandler("ElevatorLogic"), tick(0) {}

ElevatorLogic::~ElevatorLogic() {}

// register all the handlers
void ElevatorLogic::Initialize(Environment &env)
{
	env.RegisterEventHandler("Interface::Notify",		this, &ElevatorLogic::HandleNotify);
	env.RegisterEventHandler("Elevator::Moving",		this, &ElevatorLogic::HandleMoving);
	env.RegisterEventHandler("Elevator::Up",			this, &ElevatorLogic::HandleUp);
	env.RegisterEventHandler("Elevator::Down",			this, &ElevatorLogic::HandleDown);
	env.RegisterEventHandler("Elevator::Stopped",		this, &ElevatorLogic::HandleStopped);
	env.RegisterEventHandler("Elevator::Opening",		this, &ElevatorLogic::HandleOpening);
	env.RegisterEventHandler("Elevator::Opened",		this, &ElevatorLogic::HandleOpened);
	env.RegisterEventHandler("Elevator::Closing",		this, &ElevatorLogic::HandleClosing);
	env.RegisterEventHandler("Elevator::Closed",		this, &ElevatorLogic::HandleClosed);
	env.RegisterEventHandler("Elevator::Beeping",		this, &ElevatorLogic::HandleBeeping);
	env.RegisterEventHandler("Elevator::Beeped",		this, &ElevatorLogic::HandleBeeped);
	env.RegisterEventHandler("Elevator::Malfunction",	this, &ElevatorLogic::HandleMalfunction);
	env.RegisterEventHandler("Elevator::Fixed",			this, &ElevatorLogic::HandleFixed);
	env.RegisterEventHandler("Person::Entered",			this, &ElevatorLogic::HandleEntered);
	env.RegisterEventHandler("Person::Exited",			this, &ElevatorLogic::HandleExited);
	env.RegisterEventHandler("Person::Entering",		this, &ElevatorLogic::HandleEntering);
	env.RegisterEventHandler("Person::Exiting",			this, &ElevatorLogic::HandleExiting);
	env.RegisterEventHandler("Interface::Interact",		this, &ElevatorLogic::HandleInteract);
	env.RegisterEventHandler("Environment::All",		this, &ElevatorLogic::HandleAll);
}

int ElevatorLogic::getQueueLength(Elevator *ele, Floor* f)
{
	// on empty queue or unmoved elevator
	if ((elevators_[ele].queueUp.empty() && elevators_[ele].queueDown.empty()) || (!movingUp_.count(ele) && !movingDown_.count(ele)))
	{
		// get direct travel time to target
		return getTravelTime(ele,ele->GetCurrentFloor(),f,true);
	}

	int result = 0;
	// straightforward path
	if (movingUp_.count(ele) && onTheWay(ele,f))
	{
		Floor *prev = ele->GetCurrentFloor();
		Floor *cur = ele->GetCurrentFloor();
		while (cur != f)
		{
			if (elevators_[ele].queueUp.count(cur))
			{
				// also consider time for opening/closing door
				result += getTravelTime(ele,prev,cur) + 6;
				prev = cur;
			}
			cur = cur->GetAbove();
		}
		result += getTravelTime(ele,prev,f);
	}
	else if (movingDown_.count(ele) && onTheWay(ele,f))
	{
		Floor *prev = ele->GetCurrentFloor();
		Floor *cur = ele->GetCurrentFloor();
		while (cur != f)
		{
			if (elevators_[ele].queueDown.count(cur))
			{
				// also consider time for opening/closing door
				result += getTravelTime(ele,prev,cur) + 6;
				prev = cur;
			}
			cur = cur->GetBelow();
		}
		result += getTravelTime(ele,prev,f);
	}
	// more complex path
	else if (movingUp_.count(ele) && !onTheWay(ele,f))
	{
		Floor *prev = ele->GetCurrentFloor();
		Floor *cur = ele->GetCurrentFloor();
		// get all the way to the top
		while (cur != nullptr)
		{
			if (elevators_[ele].queueUp.count(cur))
			{
				// also consider time for opening/closing door
				result += getTravelTime(ele,prev,cur) + 6;
				prev = cur;
			}
			cur = cur->GetAbove();
		}
		// go down the whole down queue until target
		cur = prev;
		while (cur != f)
		{
			if (elevators_[ele].queueDown.count(cur))
			{
				// also consider time for opening/closing door
				result += getTravelTime(ele,prev,cur) + 6;
				prev = cur;
			}
			cur = cur->GetBelow();
		}
		result += getTravelTime(ele,prev,f);
	}
	else if (movingDown_.count(ele) && !onTheWay(ele,f))
	{
		Floor *prev = ele->GetCurrentFloor();
		Floor *cur = ele->GetCurrentFloor();
		// get all the way down
		while (cur != nullptr)
		{
			if (elevators_[ele].queueDown.count(cur))
			{
				// also consider time for opening/closing door
				result += getTravelTime(ele,prev,cur) + 6;
				prev = cur;
			}
			cur = cur->GetBelow();
		}
		// go back up the up queue
		cur = prev;
		while (cur != f)
		{
			if (elevators_[ele].queueUp.count(cur))
			{
				// also consider time for opening/closing door
				result += getTravelTime(ele,prev,cur) + 6;
				prev = cur;
			}
			cur = cur->GetAbove();
		}
		result += getTravelTime(ele,prev,f);
	}
	else
	{
		DEBUG_S("Case not considered");
		DEBUG_V(ele->GetId());
		DEBUG_V(onTheWay(ele,f));
		DEBUG_V(elevators_[ele].queueUp.empty());
		DEBUG_V(elevators_[ele].queueDown.empty());
		DEBUG_V(moving_.count(ele));
		DEBUG_V(movingUp_.count(ele));
		DEBUG_V(movingDown_.count(ele));
	}
	return result;
}

Elevator* ElevatorLogic::pickElevator(Environment &env, const Event &e)
{
	Interface *interf = static_cast<Interface*>(e.GetSender());
	Person *person = static_cast<Person*>(e.GetEventHandler());

	Floor *personsFloor = person->GetCurrentFloor();

	// TODO: sort all elevators at this floor by shortest path to caller
	// Then pick the one that would work best.

	// get all usable elevators that stop at this floor
	list<Elevator*> elevs;
	for(int i = 0; i < interf->GetLoadableCount(); ++i)
	{
		// cast the loadables to elevator pointers
		Elevator *ele = static_cast<Elevator*>(interf->GetLoadable(i));

		// while we're at it, add these elevators to our global map
		// using default values for its state:
		// empty passenger list
		set<Person*> passengers;
		// empty queue
		set<Floor*> queue;
		set<Floor*> queueUp;
		set<Floor*> queueDown;
		// closed door and not moving
		pair<Elevator*,ElevatorState> elevState = {ele,ELEV_DEFAULT_STATE};

		// put it into the global map (doesn't do anything if already exists)
		elevators_.insert(elevState);
		// check if space left, non-blocked and either idle or on the way
		addToList(elevs,ele,personsFloor);
	}

	if (!elevs.empty())
	{
		// get idles from current floor
		for(auto const &ele : elevs)
		{
			if ((!moving_.count(ele) || onTheWay(ele,personsFloor)) && !malfunctions_.count(ele))
			{
				DEBUG
				(
					if (!moving_.count(ele))
					{
						DEBUG_S("Using elevator " << ele->GetId() << " idling at floor " << ele->GetCurrentFloor()->GetId());
					}
					else if (onTheWay(ele,personsFloor))
					{
						DEBUG_S("Using elevator " << ele->GetId() << " on the way at floor " << ele->GetCurrentFloor()->GetId());
					}
				);
				return ele;
			}
		}

		// get elevator with lowest load
		Elevator *lowest = elevs.front();
		for(auto const &ele : elevs)
		{
			if (getCapacity(ele) > getCapacity(lowest))
			{
				lowest = ele;
			}
		}
		DEBUG_S("Using elevator " << lowest->GetId() << " (smallest load of " << getCapacity(lowest) << ") at floor " << lowest->GetCurrentFloor()->GetId());
		return lowest;
	}
	else
	{
		return nullptr;
	}
}

// What to do after recieving notification
// TODO: clean up decision on what to do for the elevator
void ElevatorLogic::HandleNotify(Environment &env, const Event &e)
{
	// find out who is calling
	Entity *ref = static_cast<Entity*>(e.GetEventHandler());
	// handle elevator movement
	if (ref->GetType() == "Elevator")
	{
		/*
		(evil hack! we're misusing our ability to let the elevator send a message
		and pretend it does so on its own. the only reason is that (AFAIK) we can't
		do anything else to have a read-only interaction, since we can't declare
		new events for this class)
		 */

		// NOTE: we assume the sender of a movement event is always an elevator
		Elevator *ele = static_cast<Elevator*>(ref);
		DEBUG
		(
			std::string state = "state unknown";
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
				"[Elevator " << ele->GetId() << "] Floor " << ele->GetCurrentFloor()->GetId() << " (" << ele->GetPosition() << "), " << state
			);
		);

		// stop if we're at the middle of any floor in one of the queues
		if
		(
			(
				elevators_[ele].queueUp.count(ele->GetCurrentFloor()) ||
				elevators_[ele].queueDown.count(ele->GetCurrentFloor())
			) &&
			(ele->GetPosition() > 0.49 && ele->GetPosition() < 0.51)
		)
		{
			env.SendEvent("Elevator::Stop",0,this,ele);
			return;
		}
		// otherwise continue movement and report back later
		else
		{
			if (!malfunctions_.count(ele))
			{
				env.SendEvent("Interface::Notify",1,ele,ele);
			}

		}

	}
	// handle person's call
	else
	{
		Interface *interf = static_cast<Interface*>(e.GetSender());
		Person *person = static_cast<Person*>(e.GetEventHandler());
		Floor *personsFloor = person->GetCurrentFloor();

		// play NSA on the test cases
		DEBUG(collectInfo(env, person););

		DEBUG_S
		(
			"[Person " << person->GetId() <<
			"] Going from floor " << person->GetCurrentFloor()->GetId() <<
			" to floor " << person->GetFinalFloor()->GetId()
		);

		// check if interface comes from inside an elevator or from a floor
		// we see this from looking at the type of the interface's first loadable
		Loadable *loadable = interf->GetLoadable(0);

		// react to an interface interaction from outside the elevator
		if (loadable->GetType() == "Elevator")
		{
			DEBUG_S
			(
				"[Person " << person->GetId() <<
				"] Waiting " << (deadlines_[person] - env.GetClock())
			);

			// try to find the best fitting elevator
			Elevator *ele = pickElevator(env,e);
			// if it exists
			if (ele)
			{
				DEBUG_S
				(
					"Using elevator " << ele->GetId() << " at floor " << ele->GetCurrentFloor()->GetId() <<
					". Distance: " << getDistance(ele->GetCurrentFloor(),personsFloor,ele->GetPosition()) <<
					" ETA: " << getQueueLength(ele,personsFloor)
				);
				sendToFloor(env,personsFloor,ele);
			}
			// if none can come, try again next tick
			else
			{
				env.SendEvent("Interface::Notify",1,interf,person);
				DEBUG_S("[Person " << person->GetId() << "] No free elevator found, trying again later");
			}
		}
		// react to an interface interaction from inside the elevator
		else
		{
			// get our current elevator by asking the person where it is
			Elevator *ele = person->GetCurrentElevator();
			// get target from the interface input
			Floor *target = static_cast<Floor*>(loadable);
			sendToFloor(env,target,ele);
		}
	}
}

// open doors immediately after stopping in the middle of a floor
void ElevatorLogic::HandleStopped(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	moving_.erase(ele);

	elevators_[ele].isBusy = true;
	openDoor(env,ele);
}

// update elevator state when something happens with the door
void ElevatorLogic::HandleOpening(Environment &env, const Event &e)
{
	Elevator* ele = static_cast<Elevator*>(e.GetSender());
	if (ele->GetPosition() <= 0.49 || ele->GetPosition() >= 0.51)
		throw std::runtime_error(showTestCase() + eventlog + "An elevator opened its doors when it was not at the center of a floor");
	if (moving_.count(ele))
		throw std::runtime_error(showTestCase() + eventlog + "An elevator opened its doors while it was moving");

	open_.insert(ele);

	elevators_[ele].doorState = Opening;
	elevators_[ele].isBusy = true;
}

void ElevatorLogic::HandleOpened(Environment &env, const Event &e)
{
	Elevator* ele = static_cast<Elevator*>(e.GetSender());
	elevators_[ele].doorState = Opened;
	elevators_[ele].isBusy = false;
	// remove floor from queue where we just opened the door
	elevators_[ele].queue.erase(ele->GetCurrentFloor());
	elevators_[ele].queueUp.erase(ele->GetCurrentFloor());
	elevators_[ele].queueDown.erase(ele->GetCurrentFloor());

	closeDoor(env,ele);
	DEBUG_S("[Elevator " << ele->GetId() << "] Removed floor " << ele->GetCurrentFloor()->GetId() << " from queue");
}

void ElevatorLogic::HandleClosing(Environment &env, const Event &e)
{
	Elevator* ele = static_cast<Elevator*>(e.GetSender());

	if (beeping_.count(ele))
		throw std::runtime_error(showTestCase() + eventlog + "An elevator closed the doors while it was beeping");

	elevators_[ele].doorState = Closing;
	elevators_[ele].isBusy = true;

}

void ElevatorLogic::HandleClosed(Environment &env, const Event &e)
{
	Elevator* ele = static_cast<Elevator*>(e.GetSender());
	open_.erase(ele);

	elevators_[ele].doorState = Closed;
	elevators_[ele].isBusy = false;

	if (loads_[ele] > ele->GetMaxLoad())
	{
		openDoor(env,ele);
	}
	else
	{
		// continue operation if possible
		continueOperation(env,ele);
	}
}

void ElevatorLogic::HandleUp(Environment &env, const Event &e)
{
}

void ElevatorLogic::HandleDown(Environment &env, const Event &e)
{
}

// react to elevators movement status
void ElevatorLogic::HandleMoving(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());
	auto iter = loads_.find(ele);
	if (iter != loads_.end())
		if (iter->second > ele->GetMaxLoad())
			throw std::runtime_error(showTestCase() + eventlog + "An elevator started moving while exceeding its maximum load");
	if (malfunctions_.count(ele))
		throw std::runtime_error(showTestCase() + eventlog + "An elevator started moving while it was malfunctioning");
	if (open_.count(ele))
		throw std::runtime_error(showTestCase() + eventlog + "An elevator started moving while its doors were open");
	moving_.insert(ele);

	env.SendEvent("Interface::Notify",0,ele,ele);

}

void ElevatorLogic::HandleBeeping(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	if (!open_.count(ele))
		throw std::runtime_error(showTestCase() + eventlog + "An elevator started beeping while its doors were closed");
	auto iter = loads_.find(ele);
	if (iter == loads_.end())
		throw std::runtime_error(showTestCase() + eventlog + "An elevator started beeping although it was not overloaded");
	if (iter->second <= ele->GetMaxLoad())
		throw std::runtime_error(showTestCase() + eventlog + "An elevator started beeping although it was not overloaded");
	beeping_.insert(ele);
}

void ElevatorLogic::HandleBeeped(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	elevators_[ele].isBusy = false;
	beeping_.erase(ele);

	// continue normal operation
	continueOperation(env,ele);
}

void ElevatorLogic::HandleMalfunction(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());
	allEvents.push_back(e);
	DEBUG_S("[NSA]: Tracking malfunction");

	malfunctions_.insert(ele);
	// stop immediately if moving
	if (moving_.count(ele))
	{
		env.SendEvent("Elevator::Stop",0,this,ele);
	}
}

void ElevatorLogic::HandleFixed(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	allEvents.push_back(e);
	DEBUG_S("[NSA]: Tracking fixed");

	elevators_[ele].isBusy = false;
	malfunctions_.erase(ele);
	// continue normal operation
	continueOperation(env,ele);
}

void ElevatorLogic::HandleEntered(Environment &env, const Event &e)
{
	Person *person = static_cast<Person*>(e.GetSender());
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());

	auto iter = loads_.insert(std::make_pair(ele, person->GetWeight()));
	if (!iter.second)
		iter.first->second += person->GetWeight();


	elevators_[ele].passengers.insert(person);
	elevators_[ele].isBusy = false;
	if (loads_[ele] > ele->GetMaxLoad())
	{
		openDoor(env,ele);
		if (!beeping_.count(ele))
		{
			beeping_.insert(ele);
			env.SendEvent("Elevator::Beep",3,this,ele);
		}
	}
	else
	{
		closeDoor(env,ele);
	}
	if (ele->HasFloor(person->GetFinalFloor()))
	{
		addToQueue(env,ele,person->GetFinalFloor());
	}
}

void ElevatorLogic::HandleExited(Environment &env, const Event &e)
{
	Person *person = static_cast<Person*>(e.GetSender());
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());

	auto iter = loads_.find(ele);
	iter->second -= person->GetWeight();

	elevators_[ele].passengers.erase(person);
	elevators_[ele].isBusy = false;
	if (beeping_.count(ele) && loads_[ele] <= ele->GetMaxLoad())
	{
		env.SendEvent("Elevator::StopBeep",0,this,ele);
	}
	else
	{
		closeDoor(env,ele);
	}
}

void ElevatorLogic::HandleEntering(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
	Person *person = static_cast<Person*>(e.GetSender());

	elevators_[ele].isBusy = true;

	DEBUG_S("[Person " << person->GetId() << "] Had " << (deadlines_[person]-e.GetTime()) << " ticks left");
	if (doorEvents_.count(ele))
	{
		if (!env.CancelEvent(doorEvents_[ele]))
		{
			DEBUG_S("Cancelling event failed, do nothing");
			return;
		}
	}
	closeDoor(env,ele);

	deadlines_.erase(deadlines_.find(person));
}

void ElevatorLogic::HandleExiting(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
	elevators_[ele].isBusy = true;

	if (!env.CancelEvent(doorEvents_[ele]))
	{
		DEBUG_S("Cancelling event failed, do nothing");
		return;
	}

	closeDoor(env,ele);
}

// send elevator into general direction of given floor
// WARNING: does not check if floor is reachable.
void ElevatorLogic::sendToFloor(Environment &env, Floor *target, Elevator *ele)
{
	DEBUG_S("Sending elevator " << ele->GetId() << " to floor " << target->GetId());
	// find out current floor
	Floor *current = ele->GetCurrentFloor();

	// add target floor to queue in any case
	addToQueue(env,ele,target);
	// if moving already, just add to queue (see below)
	if (current == target && moving_.count(ele) && ele->GetPosition() > 0.49 && ele->GetPosition() < 0.51)
	{
		env.SendEvent("Elevator::Stop",0,this,ele);
		return;
	}
	else if (moving_.count(ele))
	{
		DEBUG_S("Already moving, do nothing");
	}
	else if (malfunctions_.count(ele))
	{
		DEBUG_S("Malfunctioning, do nothing");
	}
	else if (elevators_[ele].isBusy)
	{
		DEBUG_S("Busy, do nothing");
	}
	else if (loads_[ele] > ele->GetMaxLoad())
	{
		DEBUG_S("Overloaded, do nothing");
	}
	// if idling somewhere else, send into right direction
	else
	{
		// TODO: check again after 3 ticks if starting movement is already appropriate

		// adjust delay depending on door state
		// TODO: make it more elegant
		int delay;
		if (elevators_[ele].doorState == Closed)
		{
			DEBUG_S("Door closed, we're good to go");
			delay = 0;
		}
		else if (elevators_[ele].doorState == Closing)
		{
			DEBUG_S("Door still closing, wait 3 ticks");
			delay = 3;
		}
		else
		{
			DEBUG_S("Door opened, we should close it");
			// close door before starting
			closeDoor(env,ele);
			delay = 3;
		}

		// send into right direction
		moving_.insert(ele);
		if
		(
			(movingUp_.count(ele) && !elevators_[ele].queueUp.empty()) ||
			(elevators_[ele].queueDown.empty() && current->IsAbove(target))
		)
		{
			DEBUG_S("Target floor " << target->GetId() << " is above elevator's current floor " << current->GetId());
			env.SendEvent("Elevator::Up",delay,this,ele);
			movingDown_.erase(ele);
			movingUp_.insert(ele);
		}
		else
		{
			DEBUG_S("Target floor " << target->GetId() << " is below elevator's current floor " << current->GetId());
			env.SendEvent("Elevator::Down",delay,this,ele);
			movingUp_.erase(ele);
			movingDown_.insert(ele);
		}
	}
	DEBUG(
		DEBUG_S("Up queue:");

		for (auto const &f : elevators_[ele].queueUp)
		{
			DEBUG_S(f->GetId());
		});
	DEBUG(
		DEBUG_S("Down queue:");
		for (auto const &f : elevators_[ele].queueDown)
		{
			DEBUG_S(f->GetId());
		});
}

void ElevatorLogic::openDoor(Environment &env, Elevator* ele, int delay)
{
	// only open door if stopped in the middle of a floor
	bool stoppedProperly = !moving_.count(ele) && ele->GetPosition() > 0.49 && ele->GetPosition() < 0.51;
	// only open door if it is already closed or currently closing
	bool doorClosed = elevators_[ele].doorState == Closed || elevators_[ele].doorState == Closing;

	if (stoppedProperly && doorClosed)
	{
		// store next door event
		// if one exists already, cancel it
		if (doorEvents_.count(ele))
		{
			env.CancelEvent(doorEvents_[ele]);
			doorEvents_.erase(ele);
		}
		doorEvents_.insert(make_pair(ele,env.SendEvent("Elevator::Open", delay, this, ele)));
	}
	DEBUG
	(
		else if (malfunctions_.count(ele))
		{
			DEBUG_S("[Elevator " << ele-> GetId() << "] Malfunctioning. Not opening doors.");
		}
		else
		{
			DEBUG_S("[Elevator " << ele-> GetId() << "] Stopped at floor " << ele->GetCurrentFloor()->GetId() << " in illegal position! Not opening doors.");
		}
	);
	// TODO: If door is closing, delay everything somehow until the door is open.
}

void ElevatorLogic::closeDoor(Environment &env, Elevator* ele, int delay)
{
	// only if not beeping
	// only open door if stopped in the middle of a floor
	// only open door if it is already closed or currently closing
	bool doorOpen = elevators_[ele].doorState == Opened || elevators_[ele].doorState == Opening;
	// check if someone is entering or exiting
	if (!beeping_.count(ele) && doorOpen)
	{
		if (doorEvents_.count(ele))
		{
			env.CancelEvent(doorEvents_[ele]);
			doorEvents_.erase(ele);
		}
		doorEvents_.insert(make_pair(ele,env.SendEvent("Elevator::Close", delay, this, ele)));
	}
}

// add a floor to an elevator's queue
/**
 * NOTE: the queue could as well be implemented with a set and a custom
 * comparison function that uses `IsAbove()` and `IsBelow()`
 */
void ElevatorLogic::addToQueue(Environment &env, Elevator *ele, Floor *target)
{
	Floor *current = ele->GetCurrentFloor();
	bool added = false;
	// insert into appropriate queue
	// if same floor, add to both queues
	// WARNING: this only works because handleOpened deletes the floor from both
	if (current == target)
	{
		added = elevators_[ele].queueDown.insert(target).second;
		DEBUG
		(
			if (added)
			{
				DEBUG_S("[Elevator " << ele->GetId() << "] adding floor " << target->GetId() << " to down queue");
			}
		);
		added = elevators_[ele].queueUp.insert(target).second;
		DEBUG
		(
			if (added)
			{
				DEBUG_S("[Elevator " << ele->GetId() << "] adding floor " << target->GetId() << " to up queue");
			}
		);
		//env.SendEvent("Interface::Notify",0,ele,ele);
	}
	else if (current->IsAbove(target))
	{
		added = elevators_[ele].queueUp.insert(target).second;
		DEBUG
		(
			if (added)
			{
				DEBUG_S("[Elevator " << ele->GetId() << "] adding floor " << target->GetId() << " to up queue");
			}
		);

	}
	else if (current->IsBelow(target))
	{
		added = elevators_[ele].queueDown.insert(target).second;
		DEBUG
		(
			if (added)
			{
				DEBUG_S("[Elevator " << ele->GetId() << "] adding floor " << target->GetId() << " to down queue");
			}
		);
	}

	else
	{
		ostringstream msg;
		msg << "Elevator " << ele->GetId() << " did something it was not supposed to do.";
		throw std::runtime_error(showTestCase() + eventlog + msg.str() );
	}

}

// get free space of given elevator
int ElevatorLogic::getCapacity(Elevator *ele)
{
	// start with max load
	int capacity = ele->GetMaxLoad();

	set<Person*> passengers = elevators_[ele].passengers;
	// subtract weight of each person on board
	for(auto const &i : passengers)
	{
		capacity -= i->GetWeight();
	}
	return capacity;
}

// check if elevator is on the way to given floor
bool ElevatorLogic::onTheWay(Elevator *ele,Floor *target)
{
	return
	(
		(movingUp_.count(ele) && ele->GetCurrentFloor()->IsAbove(target)) ||
		(movingDown_.count(ele) && ele->GetCurrentFloor()->IsBelow(target)) ||
		// if on the same floor
		// otherwise same floor is recognized as lower
		(ele->GetCurrentFloor() == target &&
			(
			(ele->GetPosition() > 0.51 && movingDown_.count(ele)) ||
			(ele->GetPosition() < 0.49 && movingUp_.count(ele))
			// || (ele->GetPosition() > 0.49 && ele->GetPosition() < 0.51)
			)
		)
	);
}

double ElevatorLogic::getDistance(Floor *a, Floor *b, double pos)
{
	// if relative to one floor, return distance from the middle
	if (a == b)
	{
		return abs(a->GetHeight()/2 - a->GetHeight()*pos);
	}
	// walk through all floors until we reach destination
	else if (a->IsBelow(b))
	{
		double distance = a->GetHeight()*pos;
		while (a->GetBelow() != b)
		{
			a = a->GetBelow();
			distance += a->GetHeight();
		}
		distance += b->GetHeight()/2;
		return distance;
	}
	else
	{
		double distance = a->GetHeight() - a->GetHeight()*pos;
		while (a->GetAbove() != b)
		{
			a = a->GetAbove();
			distance += a->GetHeight();
		}
		distance += b->GetHeight()/2;
		return distance;
	}
}

int ElevatorLogic::getTravelTime(Elevator *ele, Floor *a, Floor *b, bool direct)
{
	if (direct)
	{
		return ceil(getDistance(a,b,ele->GetPosition())/ele->GetSpeed());
	}
	else
	{
		return ceil(getDistance(a,b)/ele->GetSpeed());
	}
}

void ElevatorLogic::addToList(list<Elevator*> &elevs, Elevator* ele, Floor* target)
{
	DEBUG_S("Considering elevator " << ele->GetId() <<
	". Distance: " << getDistance(ele->GetCurrentFloor(),target,ele->GetPosition()) << " ETA: " << getQueueLength(ele,target));

	// if list empty, just add
	if (elevs.empty())
	{
		elevs.push_front(ele);

		return;
	}

	// otherwise find the right position to insert
	// NOTE: list is sorted by travel time to target
	auto i = elevs.begin();
	for(; i != elevs.end(); ++i)
	{
		// insert before
		if (getQueueLength((*i),target) > getQueueLength(ele,target))
		{
			// the insert happens after the loop anyway
			break;
		}
	}
	// if new floor is above all others, insert at the end
	elevs.insert(i,ele);
}

void ElevatorLogic::continueOperation(Environment &env, Elevator *ele)
{
	if (elevators_[ele].queueUp.empty() && elevators_[ele].queueDown.empty())
	{
		DEBUG_S("[Elevator " << ele->GetId() << "] Both queues empty, nothing to do.");
	}
	// if there are upwards targets and we were either going there already or have nowhere to go down
	else if
	(
		!elevators_[ele].queueUp.empty() &&
		(movingUp_.count(ele) || (elevators_[ele].queueDown.empty() && movingDown_.count(ele)))
	)
	{
		DEBUG_S("Go UP next");
		sendToFloor(env,*elevators_[ele].queueUp.begin(),ele);
	}
	// if there are downwards targets and we were either going there already or have nowhere to go up
	// NOTE: also if we never started but have something in both queues, which should not happen
	else
	{
		DEBUG_S("Go DOWN next");
		sendToFloor(env,*elevators_[ele].queueDown.begin(),ele);
	}
}

DEBUG
(
	void ElevatorLogic::HandleAll(Environment &env, const Event &e)
	{
		logEvent(env, e);
		if (env.GetClock() != tick)
		{
			for (auto pair : deadlines_)
			{
				DEBUG_S("[Person " << pair.first->GetId() << "] Waiting " << (pair.second - e.GetTime()));
				if (e.GetTime() > pair.second)
					throw std::runtime_error(showTestCase() + eventlog + "A person gave up waiting for an elevator");
			}
			tick = env.GetClock();
		}
		// check if all tracked persons reached their target
		// if (!allPersons.empty())
		// {
		// 	for (auto i : allPersons)
		// 	{
		// 		DEBUG_V(i.first->GetId());
		// 		DEBUG_V((i.first->GetCurrentFloor() == i.first->GetFinalFloor()));
		// 		if (i.first->GetCurrentFloor() != i.first->GetFinalFloor())
		// 		{
		// 			return;
		// 		}
		// 	}
		// 	exit(0);
		// }
		// abort after fixed time
		// if (env.GetClock() == 118)
		// {
		// 		throw std::runtime_error(showTestCase() + eventlog + "You must be kidding me");
		// }
	}

	void ElevatorLogic::HandleInteract(Environment &env, const Event &e)
	{
		Person *person = static_cast<Person*>(e.GetSender());
		Interface *interf = static_cast<Interface*>(e.GetEventHandler());
		int deadline = e.GetTime() + person->GetGiveUpTime();
		if (interf->GetLoadable(0)->GetType() == "Elevator")
		{
			auto iter = deadlines_.insert(std::make_pair(person, deadline));
			if (!iter.second)
				iter.first->second = deadline;
		}
	}

	string ElevatorLogic::showFloors()
	{
		ostringstream oss;
		for (Floor *f : allFloors)
		{
			oss << "Floor { " <<
			// ID
			f->GetId() << " " <<
			// floor below
			(f->GetBelow() == nullptr ? 0 : f->GetBelow()->GetId()) << " " <<
			// floor above
			(f->GetAbove() == nullptr ? 0 : f->GetAbove()->GetId()) << " " <<
			// number of interfaces
			f->GetHeight() << " " << f->GetInterfaceCount();
			// list of interfaces
			for (int i = 0; i < f->GetInterfaceCount(); i++)
			{
				oss << " " << f->GetInterface(i)->GetId();
			}
			oss << " }<br>" << endl;
		}
		return oss.str();
	}

	string ElevatorLogic::showPersons()
	{
		ostringstream oss;
		for (auto const &p : allPersons)
		{
			oss << "Person { " <<
			// ID
			p.first->GetId() << " " <<
			// start floor
			p.second.first << " " <<
			// target floor
			p.first->GetFinalFloor()->GetId() << " " <<
			// giveup time
			p.first->GetGiveUpTime() << " " <<
			// weight
			p.first->GetWeight() << " " <<
			// start time
			p.second.second <<
			" }<br>" << endl;
		}
		return oss.str();
	}

	string ElevatorLogic::showElevators()
	{
		ostringstream oss;
		for (auto const &ele : allElevators)
		{
			oss << "Elevator { " <<
			// ID
			ele.first->GetId() << " " <<
			// speed
			ele.first->GetSpeed() << " " <<
			// max load
			ele.first->GetMaxLoad() << " " <<
			// starting floor
			ele.second << " " <<
			// number of interfaces
			ele.first->GetInterfaceCount();
			// list of interfaces
			for (int i = 0; i < ele.first->GetInterfaceCount(); i++)
			{
				oss << " " << ele.first->GetInterface(i)->GetId();
			}
			oss << " }<br>" << endl;
		}
		return oss.str();
	}

	string ElevatorLogic::showInterfaces()
	{
		ostringstream oss;
		for (auto const &intf : allInterfaces)
		{
			oss << "Interface { " <<
			// ID
			intf->GetId() << " " <<
			// number loadables
			intf->GetLoadableCount();
			// list of interfaces
			for (int i = 0; i < intf->GetLoadableCount(); i++)
			{
				oss << " " << intf->GetLoadable(i)->GetId();
			}
			oss << " }<br>" << endl;
		}
		return oss.str();
	}


	string ElevatorLogic::showEvents()
	{
		ostringstream oss;
		for (auto const &e : allEvents)
		{
			Entity *snd = static_cast<Entity*>(e.GetSender());
			Entity *ref = static_cast<Entity*>(e.GetEventHandler());
			oss << "Event { " <<
			// Event type
			e.GetEvent() << " " <<
			// time
			e.GetTime() << " " <<
			// sender
			snd->GetId() << " " <<
			// reference
			ref->GetId() << " " <<
			oss << " }<br>" << endl;
		}
		return oss.str();
	}
	void ElevatorLogic::collectInfo(Environment &env, Person *person)
	{
		// track person with start floor and time
		if (allPersons.insert(make_pair(person,make_pair(person->GetCurrentFloor()->GetId(),env.GetClock()))).second)
		{
			// if it was new, also track deadline
			//deadlines_.insert(make_pair(person,person->GetGiveUpTime()));
			DEBUG_S("[NSA] Tracking person " << person->GetId());
		}
		// start from this person's floor
		Floor* f = person->GetCurrentFloor();
		// find lowest floor
		while (f->GetBelow() != nullptr)
		{
			f = f->GetBelow();
		}
		// check all floors up to the top
		do
		{
			// insert new floor
			if (allFloors.insert(f).second)
			{
				DEBUG_S("[NSA] Tracking floor "<< f->GetId());
			}
			// check all interfaces
			for(int i = 0; i < f->GetInterfaceCount(); i++)
			{
				// insert all interfaces
				if (allInterfaces.insert(f->GetInterface(i)).second)
				{
					DEBUG_S("[NSA] Tracking interface "<< f->GetInterface(i)->GetId());
				}
				// insert all elevators
				for(int k = 0; k < f->GetInterface(i)->GetLoadableCount(); k++)
				{
					Elevator *ele = static_cast<Elevator*>(f->GetInterface(i)->GetLoadable(k));
					if (allElevators.insert(make_pair(ele,ele->GetCurrentFloor()->GetId())).second)
					{
						DEBUG_S("[NSA] Tracking elevator "<< ele->GetId());
					}
					// insert all interfaces of the elevator
					for(int j = 0; j < ele->GetInterfaceCount(); j++)
					{
						if (allInterfaces.insert(ele->GetInterface(j)).second)
						{
							DEBUG_S("[NSA] Tracking interface "<< ele->GetInterface(j)->GetId());
						}
					}
				}
			}
			f = f->GetAbove();
		}
		while (f != nullptr);
	}

	void ElevatorLogic::logEvent(Environment &env, const Event &e)
	{
		ostringstream event;
		event << "[" << env.GetClock() << "] " <<
		e.GetSender()->GetName() << " sends " <<
		e.GetEvent() <<
		(e.GetEventHandler() == nullptr ? "" : " referencing ") <<
		(e.GetEventHandler() == nullptr ? "" : e.GetEventHandler()->GetName()) << "<br>" << endl;
		eventlog.append(event.str());
	}


	string ElevatorLogic::showTestCase()
	{
		return (showFloors() + showElevators() + showPersons() + showInterfaces() + showEvents());
	}
);

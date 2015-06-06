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
				"[Elevator " << ele->GetId() << "] Floor " << ele->GetCurrentFloor()->GetId() << " (" << ele->GetPosition() << "), " << state
			);
			if ((ele->GetState() == Elevator::Up || ele->GetState() == Elevator::Down) && elevators_[ele].doorState != Closed)
			{

				ostringstream result;
				result << showTestCase() << eventlog << "[" << env.GetClock() << "] Elevator " << ele->GetId() << " moved with open door!<br>\n" ;
				throw std::runtime_error(result.str());
			}
			// else if (elevators_[ele].isMalfunction == true)
			// {
			// 	ostringstream result;
			// 	result << showFloors() << showElevators() << showPersons() << showInterfaces() << eventlog << "[" << env.GetClock() << "] Elevator " << ele->GetId() << " moved while malfunctioning!<br>\n" ;
			// 	throw std::runtime_error(result.str());
			// }
		);

		// stop if we're at the middle of any floor in the queue
		elevatorQueue queue = elevators_[ele].queue;
		DEBUG
		(
			// this should absolutely not happen
			if (queue.empty())
				{
					DEBUG_S("no floor in queue");
				}
		);
		elevatorQueue::iterator i = queue.begin();
		for(; i != queue.end(); ++i)
		{
			if (*i == ele->GetCurrentFloor() && ele->GetPosition() > 0.49 && ele->GetPosition() < 0.51)
			{
				env.SendEvent("Elevator::Stop",0,this,ele);
				return;
			}
		}

		// otherwise continue movement and report back later
		env.SendEvent("Interface::Notify",1,ele,ele);
	}
	// handle person's call
	else
	{
		// grab interface that sent notification
		// notification can only come from an interface
		Interface *interf = static_cast<Interface*>(e.GetSender());
		// grab the person that interacted with the interface
		// reference of a notification can only be a person
		Person *person = static_cast<Person*>(e.GetEventHandler());

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

			Floor *personsFloor = person->GetCurrentFloor();

			// TODO: sort all elevators at this floor by shortest path to caller
			// Then pick the one that would work best.

			// get all usable elevators that stop at this floor
			list<Elevator*> elevs;
			for(int i = 0; i < interf->GetLoadableCount(); i++)
			{
				// cast the loadables to elevator pointers
				Elevator *ele = static_cast<Elevator*>(interf->GetLoadable(i));

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
				// check if space left, non-blocked and either idle or on the way
				if (getCapacity(ele) - person->GetWeight() >= 0 && (ele->GetState() == Elevator::Idle || onTheWay(ele,personsFloor)))
				{
					addToList(elevs,ele,personsFloor);
				}
			}

			if (!elevs.empty())
			{
				DEBUG_S("Using elevator " << elevs.front()->GetId() << " at floor " << elevs.front()->GetCurrentFloor()->GetId() <<
				". Distance: " << getDistance(elevs.front()->GetCurrentFloor(),personsFloor,elevs.front()->GetPosition()) << " ETA: " << getTravelTime(elevs.front(),elevs.front()->GetCurrentFloor(),personsFloor));
				if (!elevators_[elevs.front()].isBusy)
				{
					SendToFloor(env,personsFloor,elevs.front());
				}
				else
				{
					addToQueue(elevs.front(),personsFloor);
				}

				return;
			}

			// if none can come, try again next tick
			env.SendEvent("Interface::Notify",1,interf,person);
			DEBUG_S("No free elevator found, trying again later");

		}
		// react to an interface interaction from inside the elevator
		else
		{
			// get our current elevator by asking the person where it is
			Elevator *ele = person->GetCurrentElevator();
			// get target from the interface input
			Floor *target = static_cast<Floor*>(loadable);
			if (!elevators_[ele].isBusy)
			{
				SendToFloor(env,target,ele);
			}
			else
			{
				addToQueue(ele,target);
			}
		}
	}
}

// open doors immediately after stopping in the middle of a floor
void ElevatorLogic::HandleStopped(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	// original test suite
	moving_.erase(ele);

	// set this elevator to moving state
	elevators_[ele].isMoving = false;
	elevators_[ele].isBusy = true;

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
	if (ele->GetPosition() <= 0.49 || ele->GetPosition() >= 0.51)
	    throw std::runtime_error("An elevator opened its doors when it was not at the center of a floor");
	if (moving_.count(ele))
	    throw std::runtime_error("An elevator opened its doors while it was moving");
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
	elevators_[ele].queue.remove(ele->GetCurrentFloor());
	DEBUG_S("[Elevator " << ele->GetId() << "] Removed floor " << ele->GetCurrentFloor()->GetId() << " from queue");
}

void ElevatorLogic::HandleClosing(Environment &env, const Event &e)
{
	Elevator* ele = static_cast<Elevator*>(e.GetSender());

	// original test suite
    if (beeping_.count(ele))
        throw std::runtime_error("An elevator closed the doors while it was beeping");

	// elevators_[ele].doorState = Closing;
	elevators_[ele].isBusy = true;

}

void ElevatorLogic::HandleClosed(Environment &env, const Event &e)
{
	Elevator* ele = static_cast<Elevator*>(e.GetSender());
	// original test suite
	open_.erase(ele);

	elevators_[ele].doorState = Closed;
	elevators_[ele].isBusy = false;
	// get to the next floor in queue as quickly as possible
	if (!elevators_[ele].queue.empty())
	{
		SendToFloor(env,elevators_[ele].queue.front(),ele);
	}

}

void ElevatorLogic::HandleUp(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
	elevators_[ele].isMoving = true;
	elevators_[ele].isBusy = false;

}

void ElevatorLogic::HandleDown(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
	elevators_[ele].isMoving = true;
	elevators_[ele].isBusy = false;
}

// react to elevators movement status
void ElevatorLogic::HandleMoving(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());
	auto iter = loads_.find(ele);
	if (iter != loads_.end())
	    if (iter->second > ele->GetMaxLoad())
	        throw std::runtime_error("An elevator started moving while exceeding its maximum load");
	if (malfunctions_.count(ele))
	    throw std::runtime_error("An elevator started moving while it was malfunctioning");
	if (open_.count(ele))
	    throw std::runtime_error("An elevator started moving while its doors were open");
	moving_.insert(ele);
}

void ElevatorLogic::HandleBeeping(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());

	// original test suite
	if (!open_.count(ele))
	    throw std::runtime_error("An elevator started beeping while its doors were closed");
	auto iter = loads_.find(ele);
	if (iter == loads_.end())
	    throw std::runtime_error("An elevator started elevator although it was not overloaded");
	if (iter->second <= ele->GetMaxLoad())
	    throw std::runtime_error("An elevator started elevator although it was not overloaded");
	beeping_.insert(ele);

	// elevators_[ele].isBeeping = true;
	// DEBUG
	// (
	// 	if (elevators_[ele].doorState == Closed)
	// 	{
	// 		ostringstream result;
	// 		result << showFloors() << showElevators() << showPersons() << showInterfaces() << eventlog << "[" << env.GetClock() << "] Elevator " << ele->GetCurrentFloor()->GetId() << " started beeping with door closed!<br>\n" ;
	// 		throw std::runtime_error(result.str());
	// 	}
	// );
}
void ElevatorLogic::HandleBeeped(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());

	// original test suite
	beeping_.erase(ele);
	// elevators_[ele].isBeeping = false;
	// continue normal operation
	if (!elevators_[ele].queue.empty())
	{
		SendToFloor(env,elevators_[ele].queue.front(),ele);
	}
}

void ElevatorLogic::HandleMalfunction(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
	// original test suite
	malfunctions_.insert(ele);

	// elevators_[ele].isMalfunction = true;
	// stop immediately
	env.SendEvent("Elevator::Stop",0,this,ele);
}

void ElevatorLogic::HandleFixed(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
	// original test suite
	malfunctions_.erase(ele);

	//elevators_[ele].isMalfunction = false;

	// continue normal operation
	if (!elevators_[ele].queue.empty())
	{
		SendToFloor(env,elevators_[ele].queue.front(),ele);
	}
}

void ElevatorLogic::HandleEntered(Environment &env, const Event &e)
{
	Person *person = static_cast<Person*>(e.GetSender());
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());

	// original test suite
	auto iter = loads_.insert(std::make_pair(ele, person->GetWeight()));
	if (!iter.second)
	    iter.first->second += person->GetWeight();

	elevators_[ele].passengers.insert(person);
	elevators_[ele].isBusy = false;
}

void ElevatorLogic::HandleExited(Environment &env, const Event &e)
{
	Person *person = static_cast<Person*>(e.GetSender());
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());

	// original test suite
	auto iter = loads_.find(ele);
	iter->second -= person->GetWeight();

	elevators_[ele].passengers.erase(person);
	elevators_[ele].isBusy = false;

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
	Person *person = static_cast<Person*>(e.GetSender());



	elevators_[ele].isBusy = true;

	// do not track deadlines anymore
	DEBUG_S("[Person " << person->GetId() << "] Had " << (deadlines_[person] - e.GetTime()) << " ticks left");
	if (getCapacity(ele) - person->GetWeight() < 0)
	{
		DEBUG_S("[Elevator " << ele->GetId() << "] Will be overloaded if person enters!");
		env.SendEvent("Elevator::Beep",0,this,ele);
	}
	else
	{
		closeDoor(env,ele);
	}
	// original test suite
	deadlines_.erase(deadlines_.find(person));
}

void ElevatorLogic::HandleExiting(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
	elevators_[ele].isBusy = true;

	closeDoor(env,ele);
}

// send elevator into general direction of given floor
// WARNING: does not check if floor is reachable.
void ElevatorLogic::SendToFloor(Environment &env, Floor *target, Elevator *ele)
{
	DEBUG_S("Sending elevator " << ele->GetId() << " to floor " << target->GetId());
	// find out current floor
	Floor *current = ele->GetCurrentFloor();

	addToQueue(ele,target);
	if (current == target)
	{
		openDoor(env,ele);
		elevators_[ele].isBusy = true;
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
	if (elevators_[ele].queue.front()->IsBelow(current))
	{
		DEBUG_S("Target floor " << target->GetId() << " is above elevator's current floor " << ele->GetCurrentFloor()->GetId());
		env.SendEvent("Elevator::Up",delay,this,ele);
	}
	else
	{
		DEBUG_S("Target floor " << target->GetId() << " is below elevator's current floor " << ele->GetCurrentFloor()->GetId());
		env.SendEvent("Elevator::Down",delay,this,ele);
	}
	env.SendEvent("Interface::Notify",delay+1,ele,ele);
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
	if (!beeping && doorOpen)
	{
		//DEBUG_S("[Elevator " << ele->GetId() << "] Closing door");
		env.SendEvent("Elevator::Close", 0, this, ele);
		elevators_[ele].doorState = Closing;
	}
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

	// try just queueing floors FIFO style
	if (std::find(q.begin(),q.end(),target) == q.end())
	{
		q.push_back(target);
		DEBUG_S("[Elevator " << ele->GetId() << "] Added floor " << target->GetId() << " to queue");
	}
	else
	{
		DEBUG_S("Floor " << target->GetId() << " already in queue of elevator " << ele->GetId());
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
		(ele->GetState() == Elevator::Up && ele->GetCurrentFloor()->IsAbove(target)) ||
		(ele->GetState() == Elevator::Down && ele->GetCurrentFloor()->IsBelow(target)) ||
		// if on the same floor
		// otherwise same floor is recognized as lower
		(ele->GetCurrentFloor() == target &&
			(
			(ele->GetPosition() > 0.5 && ele->GetState() == Elevator::Down) ||
			(ele->GetPosition() < 0.5 && ele->GetState() == Elevator::Up)
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
	". Distance: " << getDistance(ele->GetCurrentFloor(),target,ele->GetPosition()) << " ETA: " << getTravelTime(ele,ele->GetCurrentFloor(),target));

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
		if (getTravelTime((*i),(*i)->GetCurrentFloor(),target) > getTravelTime(ele,ele->GetCurrentFloor(),target))
		{
			// the insert happens after the loop anyway
			break;
		}
	}
	// if new floor is above all others, insert at the end
	elevs.insert(i,ele);
}

DEBUG
(
	void ElevatorLogic::HandleAll(Environment &env, const Event &e)
	{
		logEvent(env, e);
		if (env.GetClock() != tick)
		{
			// original test suite
			for (auto pair : deadlines_)
			{
				DEBUG_S("[Person " << pair.first->GetId() << "] Waiting " << (pair.second - e.GetTime()));
			    if (e.GetTime() > pair.second)
			        throw std::runtime_error(eventlog + "A person gave up waiting for an elevator");
			}
			tick = env.GetClock();
		}
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
			// interfaces only have one target each
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
		e.GetEvent() << " referencing " <<
		(e.GetEventHandler() == nullptr ? "" : e.GetEventHandler()->GetName()) << "<br>" << endl;
		eventlog.append(event.str());
	}

	string ElevatorLogic::showTestCase()
	{
		return (showFloors() + showElevators() + showPersons() + showInterfaces());
	}
);

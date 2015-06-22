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


ElevatorLogic::ElevatorLogic() : EventHandler("ElevatorLogic"), time_(0) {}

ElevatorLogic::~ElevatorLogic() {}

void ElevatorLogic::Initialize(Environment &env)
{
	env.RegisterEventHandler("Interface::Notify", this, &ElevatorLogic::HandleNotify);
	env.RegisterEventHandler("Interface::Interact", this, &ElevatorLogic::HandleInteract);
	env.RegisterEventHandler("Elevator::Moving", this, &ElevatorLogic::HandleMoving);
	env.RegisterEventHandler("Elevator::Stopped", this, &ElevatorLogic::HandleStopped);
	env.RegisterEventHandler("Elevator::Opened", this, &ElevatorLogic::HandleOpened);
	env.RegisterEventHandler("Elevator::Closed", this, &ElevatorLogic::HandleClosed);
	env.RegisterEventHandler("Elevator::Stopped", this, &ElevatorLogic::HandleStopped);
	env.RegisterEventHandler("Elevator::Opening", this, &ElevatorLogic::HandleOpening);
	env.RegisterEventHandler("Person::Entering", this, &ElevatorLogic::HandleEntering);
	env.RegisterEventHandler("Person::Entered", this, &ElevatorLogic::HandleEntered);
	env.RegisterEventHandler("Person::Exiting", this, &ElevatorLogic::HandleExiting);
	env.RegisterEventHandler("Person::Exited", this, &ElevatorLogic::HandleExited);
	env.RegisterEventHandler("Elevator::Beeping", this, &ElevatorLogic::HandleBeeping);
	env.RegisterEventHandler("Elevator::Beeped", this, &ElevatorLogic::HandleBeeped);
	env.RegisterEventHandler("Elevator::Closing", this, &ElevatorLogic::HandleClosing);
	env.RegisterEventHandler("Elevator::Malfunction", this, &ElevatorLogic::HandleMalfunction);
	env.RegisterEventHandler("Elevator::Fixed", this, &ElevatorLogic::HandleFixed);
	env.RegisterEventHandler("Environment::All", this, &ElevatorLogic::HandleAll);
}

/**
 * handle incoming events
 */

void ElevatorLogic::HandleNotify(Environment &env, const Event &e)
{
		Entity *ref = static_cast<Entity*>(e.GetEventHandler());
		// handle elevator movement
		if (ref->GetType() == "Elevator")
		{
			Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
			Floor *current = ele->GetCurrentFloor();

			// catch possible null pointer
			if (queue_.find(ele) != queue_.end())
				throw runtime_error(showTestCase() + "An elevator was moving without having a queue.");

			// stop if we're at the middle of any floor in the queue or at the upper/lower limit
			if ((queue_[ele].count(current)
				|| ele->IsHighestFloor(current) || ele->IsLowestFloor(current))
				&&	(ele->GetPosition() > 0.49 && ele->GetPosition() < 0.51)
				)
			{
				env.SendEvent("Elevator::Stop",0,this,ele);
			}
			// otherwise continue movement and report back later
			else
			{
				// but only if not malfunctioning
				if (!malfunctions_.count(ele))
					env.SendEvent("Interface::Notify",1,ele,ele);
			}
		}
		// handle person's call
		else
		{
			Interface *interf = static_cast<Interface*>(e.GetSender());
			Person *person = static_cast<Person*>(e.GetEventHandler());
			Floor *target = person->GetCurrentFloor();

			// play NSA on the test cases
			DEBUG(collectInfo(env, person););

			DEBUG_S
			(
				"[Person " << person->GetId() <<
				"] Going from floor " << person->GetCurrentFloor()->GetId() <<
				" to floor " << person->GetFinalFloor()->GetId()
			);

			// NOTE: interfaces have exactly one loadable
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
					sendToFloor(env,target,ele);
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
				sendToFloor(env,ele,target);
			}
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

void ElevatorLogic::HandleMoving(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());
	auto iter = loads_.find(ele);

	if (iter != loads_.end())
		if (iter->second > ele->GetMaxLoad())
			throw runtime_error(showTestCase() + "An elevator started moving while exceeding its maximum load.");

	if (malfunctions_.count(ele))
		throw runtime_error(showTestCase() + "An elevator started moving while it was malfunctioning.");
	if (open_.count(ele))
		throw runtime_error(showTestCase() + "An elevator started moving while its doors were open.");
	moving_.insert(ele);
	elevators_.insert(ele);
}

void ElevatorLogic::HandleStopped(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());
	moving_.erase(ele);
}

void ElevatorLogic::HandleOpening(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());
	if (ele->GetPosition() <= 0.49 || ele->GetPosition() >= 0.51)
		throw runtime_error(showTestCase() + "An elevator opened its doors when it was not at the center of a floor.");
	if (moving_.count(ele))
		throw runtime_error(showTestCase() + "An elevator opened its doors while it was moving.");
	open_.insert(ele);
}

void ElevatorLogic::HandleOpened(Environment &env, const Event &e)
{}

void ElevatorLogic::HandleClosing(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());
	if (beeping_.count(ele))
		throw runtime_error(showTestCase() + "An elevator closed the doors while it was beeping.");
}

void ElevatorLogic::HandleClosed(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());
	open_.erase(ele);
}

void ElevatorLogic::HandleEntering(Environment &env, const Event &e)
{
	Person *person = static_cast<Person*>(e.GetSender());
	deadlines_.erase(deadlines_.find(person));
}

void ElevatorLogic::HandleEntered(Environment &env, const Event &e)
{
	Person *person = static_cast<Person*>(e.GetSender());
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
	auto iter = loads_.insert(make_pair(ele, person->GetWeight()));
	if (!iter.second)
		iter.first->second += person->GetWeight();
}

void ElevatorLogic::HandleExiting(Environment &env, const Event &e)
{
}

void ElevatorLogic::HandleExited(Environment &env, const Event &e)
{
	Person *person = static_cast<Person*>(e.GetSender());
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
	auto iter = loads_.find(ele);
	iter->second -= person->GetWeight();
}

void ElevatorLogic::HandleBeeping(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());
	if (!open_.count(ele))
		throw runtime_error(showTestCase() + "An elevator started beeping while its doors were closed.");
	auto iter = loads_.find(ele);
	if (iter == loads_.end())
		throw runtime_error(showTestCase() + "An elevator started elevator although it was not overloaded.");
	if (iter->second <= ele->GetMaxLoad())
		throw runtime_error(showTestCase() + "An elevator started elevator although it was not overloaded.");
	beeping_.insert(ele);
}

void ElevatorLogic::HandleBeeped(Environment &env, const Event &e)
{
	 Elevator *ele = static_cast<Elevator*>(e.GetSender());
	 beeping_.erase(ele);
}


void ElevatorLogic::HandleMalfunction(Environment &env, const Event &e)
{
	 Elevator *ele = static_cast<Elevator*>(e.GetSender());
	 malfunctions_.insert(ele);
}

void ElevatorLogic::HandleFixed(Environment &env, const Event &e)
{
	 Elevator *ele = static_cast<Elevator*>(e.GetSender());
	 malfunctions_.erase(ele);
}

void ElevatorLogic::HandleAll(Environment &env, const Event &e)
{
	for (auto pair : deadlines_)
	{
		if (e.GetTime() > pair.second)
			throw runtime_error(showTestCase() + "A person gave up waiting for an elevator.");
	}

	for (Elevator *ele : elevators_)
	{
		Floor *current = ele->GetCurrentFloor();
		if (ele->GetPosition() > 0.51)
			if (ele->IsHighestFloor(current))
				throw runtime_error(showTestCase() + "An elevator crashed into the ceiling of its elevator shaft.");
		if (ele->GetPosition() < 0.49)
			if (ele->IsLowestFloor(current))
				throw runtime_error(showTestCase() + "An elevator crashed into the floor of its elevator shaft.");
	}
}


/**
 * collect and pretty print environment information
 */

string ElevatorLogic::showFloors()
{
	ostringstream oss;
	for (auto const &f : allFloors)
	{
		oss << "Floor { " <<
		// id
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
		// id
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
	for (auto const &e : allElevators)
	{
		oss << "Elevator { " <<
		// id
		e.first->GetId() << " " <<
		// speed
		e.first->GetSpeed() << " " <<
		// max load
		e.first->GetMaxLoad() << " " <<
		// starting floor
		e.second << " " <<
		// number of interfaces
		e.first->GetInterfaceCount();
		// list of interfaces
		for (int i = 0; i < e.first->GetInterfaceCount(); i++)
		{
			oss << " " << e.first->GetInterface(i)->GetId();
		}
		oss << " }<br>" << endl;
	}
	return oss.str();
}

string ElevatorLogic::showInterfaces()
{
	ostringstream oss;
	for (auto const &i : allInterfaces)
	{
		oss << "Interface { " <<
		// id
		i->GetId() << " " <<
		// number of loadables
		i->GetLoadableCount();
		// list of interfaces
		for (int j = 0; j < i->GetLoadableCount(); j++)
		{
			oss << " " << i->GetLoadable(j)->GetId();
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
		// event type
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
		DEBUG_S("[NSA] Tracking person " << person->GetId());

	// start from this person's floor
	Floor* f = person->GetCurrentFloor();
	// find lowest floor
	while (f->GetBelow() != nullptr)
	{
		f = f->GetBelow();
	}
	// insert all floors up to the top
	while (f != nullptr)
	{
		if (allFloors.insert(f).second)
			DEBUG_S("[NSA] Tracking floor " << f->GetId());
		// insert all interfaces
		for(int i = 0; i < f->GetInterfaceCount(); i++)
		{
			if (allInterfaces.insert(f->GetInterface(i)).second)
				DEBUG_S("[NSA] Tracking interface " << f->GetInterface(i)->GetId());
			// insert all elevators
			for(int k = 0; k < f->GetInterface(i)->GetLoadableCount(); k++)
			{
				Elevator *ele = static_cast<Elevator*>(f->GetInterface(i)->GetLoadable(k));
				if (allElevators.insert(make_pair(ele,ele->GetCurrentFloor()->GetId())).second)
					DEBUG_S("[NSA] Tracking elevator " << ele->GetId());
				// insert all interfaces of the elevator
				for(int j = 0; j < ele->GetInterfaceCount(); j++)
				{
					if (allInterfaces.insert(ele->GetInterface(j)).second)
						DEBUG_S("[NSA] Tracking interface " << ele->GetInterface(j)->GetId());
				}
			}
		}
		f = f->GetAbove();
	}
}

void ElevatorLogic::logEvent(Environment &env, const Event &e)
{
	ostringstream event;
	event << "[" << env.GetClock() << "] " <<
	e.GetSender()->GetName() << " sends " << e.GetEvent() <<
	(e.GetEventHandler() == nullptr ? "" : " referencing " + e.GetEventHandler()->GetName()) <<
	"<br>" << endl;
	eventlog.append(event.str());
}

string ElevatorLogic::showTestCase()
{
	return (showFloors() + showElevators() + showPersons() + showInterfaces() + showEvents());
}

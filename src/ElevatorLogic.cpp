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
	env.RegisterEventHandler("Elevator::Opening", this, &ElevatorLogic::HandleOpening);
	env.RegisterEventHandler("Elevator::Opened", this, &ElevatorLogic::HandleOpened);
	env.RegisterEventHandler("Elevator::Closing", this, &ElevatorLogic::HandleClosing);
	env.RegisterEventHandler("Elevator::Closed", this, &ElevatorLogic::HandleClosed);
	env.RegisterEventHandler("Person::Entering", this, &ElevatorLogic::HandleEntering);
	env.RegisterEventHandler("Person::Entered", this, &ElevatorLogic::HandleEntered);
	env.RegisterEventHandler("Person::Exiting", this, &ElevatorLogic::HandleExiting);
	env.RegisterEventHandler("Person::Exited", this, &ElevatorLogic::HandleExited);
	env.RegisterEventHandler("Elevator::Beeping", this, &ElevatorLogic::HandleBeeping);
	env.RegisterEventHandler("Elevator::Beeped", this, &ElevatorLogic::HandleBeeped);
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

			// catch possible null pointer
			if (queueInt_.find(ele) == queueInt_.end() || queueExt_.find(ele) == queueExt_.end())
				throw runtime_error(showTestCase() + "An elevator was moving without having a queue.");

			// stop if we're at the middle of any floor in the queue or at the upper/lower limit
			if ((queueInt_[ele].count(current) || queueExt_[ele].count(current) || (movingUp_.count(ele) && ele->IsHighestFloor(current)) || (movingDown_.count(ele) && ele->IsLowestFloor(current)))
				&& (inPosition(ele)))
			{
				if (moving_.count(ele))
					env.SendEvent("Elevator::Stop",0,this,ele);
				else
				{
					env.SendEvent("Elevator::Open",0,this,ele);
					opening_.insert(ele);
				}
				if (queueInt_[ele].erase(current))
					DEBUG_S("[Elevator " << ele->GetId() << "] Removed floor " << current->GetId() << " from internal queue");
				if  (queueExt_[ele].erase(current))
					DEBUG_S("[Elevator " << ele->GetId() << "] Removed floor " << current->GetId() << " from external queue");
				if (queueInt_[ele].empty() && queueExt_[ele].empty())
				{
					DEBUG_S("[Elevator " << ele->GetId() << "] Queues now empty, removing movement direction");
					movingUp_.erase(ele);
					movingDown_.erase(ele);
				}
			}
			// otherwise continue movement and report back later
			else
			{
				// but only if not malfunctioning
				if (!malfunctions_.count(ele))
				{
					nextNotification_.erase(ele);
					nextNotification_.insert(make_pair(ele,env.SendEvent("Interface::Notify",1,ele,ele)));
				}
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
			Elevator *ele;
			// react to an interface interaction from outside the elevator
			if (loadable->GetType() == "Elevator")
			{
				DEBUG_S
				(
					"[Person " << person->GetId() <<
					"] Waiting " << (deadlines_[person] - env.GetClock())
				);

				// try to find the best fitting elevator
				ele = pickElevator(interf,target);
				// if it exists
				if (ele)
				{
					// add target floor to external queue
					if (!(ele->GetCurrentFloor() == target && opening_.count(ele)))
						if (queueExt_[ele].insert(target).second)
							DEBUG_S("[Elevator " << ele->GetId() << "] Added floor " << target->GetId() << " to external queue");
					continueOperation(env,ele);
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
				ele = person->GetCurrentElevator();
				// get target from the interface input
				Floor *target = static_cast<Floor*>(loadable);
				// add target floor to internal queue
				if (queueInt_[ele].insert(target).second)
					DEBUG_S("[Elevator " << ele->GetId() << "] Added floor " << target->GetId() << " to internal queue");
				continueOperation(env,ele);
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
	env.SendEvent("Interface::Notify",0,ele,ele);
}

void ElevatorLogic::HandleStopped(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());
	moving_.erase(ele);

	if (!moving_.count(ele) && inPosition(ele))
	{
		env.SendEvent("Elevator::Open",0,this,ele);
		opening_.insert(ele);
	}
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
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	opening_.erase(ele);

	bool overloaded = false;
	auto iter = loads_.find(ele);
	if (iter != loads_.end())
		if (iter->second > ele->GetMaxLoad())
			overloaded = true;

	if (!malfunctions_.count(ele) && !beeping_.count(ele) && !overloaded)
	{
		env.SendEvent("Elevator::Close",0,this,ele);
		closing_.insert(ele);
	}
	if (overloaded)
		env.SendEvent("Elevator::Beep",0,this,ele);
}

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
	closing_.erase(ele);

	// exclude overloaded
	bool overloaded = false;
	auto iter = loads_.find(ele);
	if (iter != loads_.end())
		if (iter->second > ele->GetMaxLoad())
			overloaded = true;

	if (!malfunctions_.count(ele) && !overloaded)
		continueOperation(env,ele);
	else if (overloaded)
	{
		env.SendEvent("Elevator::Open",0,this,ele);
		opening_.insert(ele);
	}
}

void ElevatorLogic::HandleEntering(Environment &env, const Event &e)
{
	Person *person = static_cast<Person*>(e.GetSender());
	DEBUG_S("[Person " << person->GetId() << "] Had " << (deadlines_[person]-e.GetTime()) << " ticks left");
	deadlines_.erase(deadlines_.find(person));
}

void ElevatorLogic::HandleEntered(Environment &env, const Event &e)
{
	Person *person = static_cast<Person*>(e.GetSender());
	Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
	auto iter = loads_.insert(make_pair(ele, person->GetWeight()));
	if (!iter.second)
		iter.first->second += person->GetWeight();

	if (!closing_.count(ele) && open_.count(ele) && loads_[ele] > ele->GetMaxLoad() && !beeping_.count(ele))
	{
		env.SendEvent("Elevator::Beep",0,this,ele);
	}

	// read passenger's mind
	if (ele->HasFloor(person->GetFinalFloor()))
	{
		queueInt_[ele].insert(person->GetFinalFloor());
		DEBUG_S("[Elevator " << ele->GetId() << "] Added floor " << person->GetFinalFloor()->GetId() << " to internal queue");
	}

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

	if (beeping_.count(ele) && loads_[ele] <= ele->GetMaxLoad())
	{
		env.SendEvent("Elevator::StopBeep",0,this,ele);
	}
}

void ElevatorLogic::HandleBeeping(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());
	if (!open_.count(ele))
		throw runtime_error(showTestCase() + "An elevator started beeping while its doors were closed.");
	auto iter = loads_.find(ele);
	if (iter == loads_.end())
		throw runtime_error(showTestCase() + "An elevator started beeping although it was not overloaded.");
	if (iter->second <= ele->GetMaxLoad())
		throw runtime_error(showTestCase() + "An elevator started beeping although it was not overloaded.");
	beeping_.insert(ele);
}

void ElevatorLogic::HandleBeeped(Environment &env, const Event &e)
{
	 Elevator *ele = static_cast<Elevator*>(e.GetSender());
	 beeping_.erase(ele);
	 if (!malfunctions_.count(ele))
	 {
		env.SendEvent("Elevator::Close",0,this,ele);
		closing_.insert(ele);
	 }
}


void ElevatorLogic::HandleMalfunction(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());
	malfunctions_.insert(ele);

	if (moving_.count(ele))
		env.SendEvent("Elevator::Stop",0,this,ele);

	allEvents.push_back(e);
	DEBUG_S("[NSA]: Tracking malfunction");

	if (queueInt_.find(ele) != queueInt_.end() && queueExt_.find(ele) != queueExt_.end())
	{
		// reassign external queue
		for (auto const &f : queueExt_[ele])
		{
			for (auto const &el : queueExt_)
			{
				if (el.first->HasFloor(f) && !malfunctions_.count(el.first))
				{
					DEBUG_S("Elevator " << el.first->GetId() << " is good, reassigning.");
					// add to new elevator's queue
					queueExt_[el.first].insert(f);
					continueOperation(env,el.first);
					// remove from malfunctioning elevator's queue
					queueExt_[ele].erase(f);
					break;
				}
			}
		}
		// if in position, clear internal queue (and optionally call new elevators for passengers)
		if (inPosition(ele))
		{
			queueInt_[ele].clear();
			DEBUG_S("[Elevator " << ele->GetId() << "] Cleared internal queue");
		}
	}

}

void ElevatorLogic::HandleFixed(Environment &env, const Event &e)
{
	Elevator *ele = static_cast<Elevator*>(e.GetSender());
	malfunctions_.erase(ele);

	allEvents.push_back(e);
	DEBUG_S("[NSA]: Tracking fixed");

	if (open_.count(ele))
	{
		env.SendEvent("Elevator::Close",0,this,ele);
		closing_.insert(ele);
	}

	set<Floor*> q;
	if (queueInt_.insert(make_pair(ele,q)).second)
		DEBUG_S("[Elevator " << ele->GetId() << "] Had no internal queue! Internal queue created");
	if (queueExt_.insert(make_pair(ele,q)).second)
		DEBUG_S("[Elevator " << ele->GetId() << "] Had no external queue! External queue created");

	continueOperation(env,ele);
}

void ElevatorLogic::HandleAll(Environment &env, const Event &e)
{
	logEvent(env, e);

	if (env.GetClock() != time_)
	{
		time_ = env.GetClock();
		for (auto pair : deadlines_)
		{
			DEBUG_S("[Person " << pair.first->GetId() << "] Waiting " << (pair.second - time_));
		}
	}

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
 * helper functions
 */

// check if elevator can really stop at a given floor
bool ElevatorLogic::canReachFloor(Elevator *ele, Floor* target)
{
	if (!ele || !target)
		throw runtime_error(showTestCase() + "Trying to dereference nullptr in canReachFloor().");

	// distance from elevator's current position to middle of target floor
	double distance	= getDistance(ele->GetCurrentFloor(),target,ele->GetPosition());
	int speed      	= ele->GetSpeed();
	// exact travel time to target
	double travelTime	= distance/speed;
	int height       	= target->GetHeight();
	// mantissa of travel time
	double m	= travelTime - floor(travelTime);

	// this tells us if we still hit the middle if we round travel time up or down
	bool p1	= height*0.5 - speed*m    	>= height*0.49;
	bool p2	= height*0.5 + speed*(1-m)	<= height*0.51;

	return (p1 || p2);
}

bool ElevatorLogic::inPosition(Elevator *ele)
{
	if (!ele)
		throw runtime_error(showTestCase() + "Trying to dereference nullptr in inPosition().");

	return (ele->GetPosition() >= 0.49 && ele->GetPosition() <= 0.51);
}

void ElevatorLogic::continueOperation(Environment &env, Elevator *ele)
{
	if (!ele)
		throw runtime_error(showTestCase() + "Trying to dereference nullptr in continueOperation().");

	if (queueInt_.find(ele) == queueInt_.end())
		throw runtime_error(showTestCase() + "An elevator was trying to continue operation without an internal queue.");
	if (queueExt_.find(ele) == queueExt_.end())
		throw runtime_error(showTestCase() + "An elevator was trying to continue operation without an external queue.");

	// create merged queue
	set<Floor*> q;
	q.insert(queueInt_[ele].begin(),queueInt_[ele].end());
	q.insert(queueExt_[ele].begin(),queueExt_[ele].end());

	if (q.empty())
	{
		DEBUG_S("[Elevator " << ele->GetId() << "] Queue empty, nothing to do.");
		return;
	}
	// catch bad behavior
	if (inPosition(ele) && q.count(ele->GetCurrentFloor()))
	{
		if (nextNotification_.find(ele) != nextNotification_.end())
		{
			if (env.CancelEvent(nextNotification_[ele]))
			{
				env.SendEvent("Interface::Notify",0,ele,ele);
				return;
			}
		}
	}
	if (moving_.count(ele))
	{
		DEBUG_S("[Elevator " << ele->GetId() << "] Already moving, do nothing");
		return;
	}
	if (open_.count(ele) || opening_.count(ele))
	{
		DEBUG_S("[Elevator " << ele->GetId() << "] Door open, do nothing");
		return;
	}
	if (busy_.count(ele))
	{
		DEBUG_S("[Elevator " << ele->GetId() << "] Busy, do nothing");
		return;
	}
	if (malfunctions_.count(ele))
	{
		DEBUG_S("[Elevator " << ele->GetId() << "] Malfunctioning, do nothing.");
		return;
	}

	if ((movingUp_.count(ele) && hasUpQueue(ele)) || !hasDownQueue(ele))
	{
		env.SendEvent("Elevator::Up",0,this,ele);
		movingDown_.erase(ele);
		movingUp_.insert(ele);
		moving_.insert(ele);
	}
	else if ((movingDown_.count(ele) && hasDownQueue(ele)) || !hasUpQueue(ele))
	{
		env.SendEvent("Elevator::Down",0,this,ele);
		movingUp_.erase(ele);
		movingDown_.insert(ele);
		moving_.insert(ele);
	}
	// if no movement direction given but queue not empty
	// decide by counting waiting people above and below
	else
	{
		int up = 0;
		int down = 0;

		for (auto const &f : queueExt_[ele])
		{
			if (ele->GetCurrentFloor() != f)
			{
				if (ele->GetCurrentFloor()->IsAbove(f))
					up++;
				else if (ele->GetCurrentFloor()->IsBelow(f))
					down++;
			}
		}

		// NOTE: will still go up first if number of people is the same
		if (up >= down)
		{
			env.SendEvent("Elevator::Up",0,this,ele);
			movingUp_.insert(ele);
			moving_.insert(ele);
		}
		else
		{
			env.SendEvent("Elevator::Down",0,this,ele);
			movingDown_.insert(ele);
			moving_.insert(ele);
		}
	}

	DEBUG_S("[Elevator " << ele->GetId() << "] Complete queue:");
	for (auto const &f : q)
	{
		DEBUG_S(f->GetId());
	}
	DEBUG_S("[Elevator " << ele->GetId() << "] Internal Queue:");
	for (auto const &f : queueInt_[ele])
	{
		DEBUG_S(f->GetId());
	}
	DEBUG_S("[Elevator " << ele->GetId() << "] External Queue:");
	for (auto const &f : queueExt_[ele])
	{
		DEBUG_S(f->GetId());
	}
}

bool ElevatorLogic::hasUpQueue(Elevator *ele)
{
	if (!ele)
		throw runtime_error(showTestCase() + "Trying to dereference nullptr in hasUpQueue().");

	if (queueInt_.find(ele) != queueInt_.end() && queueExt_.find(ele) != queueExt_.end())
	{
		set<Floor*> q;
		q.insert(queueInt_[ele].begin(),queueInt_[ele].end());
		q.insert(queueExt_[ele].begin(),queueExt_[ele].end());

		for (auto const& f : q)
		{
			if (ele->GetCurrentFloor()->IsAbove(f))
				return true;
		}
	}
	return false;
}

bool ElevatorLogic::hasDownQueue(Elevator *ele)
{
	if (!ele)
		throw runtime_error(showTestCase() + "Trying to dereference nullptr in hasDownQueue().");

	if (queueInt_.find(ele) != queueInt_.end() && queueExt_.find(ele) != queueExt_.end())
	{
		set<Floor*> q;
		q.insert(queueInt_[ele].begin(),queueInt_[ele].end());
		q.insert(queueExt_[ele].begin(),queueExt_[ele].end());

		for (auto const& f : q)
		{
			if (ele->GetCurrentFloor()->IsBelow(f))
				return true;
		}
	}
	return false;
}

// check if elevator is on the way to given floor
bool ElevatorLogic::onTheWay(Elevator *ele, Floor *target)
{
	if (!ele || !target)
		throw runtime_error(showTestCase() + "Trying to dereference nullptr in onTheWay().");

	return
	(
		(movingUp_.count(ele) && ele->GetCurrentFloor()->IsAbove(target)) ||
		(movingDown_.count(ele) && ele->GetCurrentFloor()->IsBelow(target)) ||
		(ele->GetCurrentFloor() == target &&
			(
			(ele->GetPosition() > 0.51 && movingDown_.count(ele)) ||
			(ele->GetPosition() < 0.49 && movingUp_.count(ele)) ||
			// this is important so that getQueueLength() works as expected
			(inPosition(ele))
			)
		)
	);
}

// get distance from one floor (including position) to another
double ElevatorLogic::getDistance(Floor *a, Floor *b, double pos)
{
	if (!a || !b)
		throw runtime_error(showTestCase() + "Trying to dereference nullptr in getDistance().");

	// if relative to one floor, return distance from the middle
	if (a == b)
		return abs(a->GetHeight()/2.0 - a->GetHeight()*pos);
	// walk through all floors until we reach destination
	else if (a->IsBelow(b))
	{
		double distance = a->GetHeight()*pos;
		while (a->GetBelow() != b)
		{
			if (!a->GetBelow())
				throw runtime_error(showTestCase() + "Trying to dereference nullptr in getDistance() loop.");

			a = a->GetBelow();
			distance += a->GetHeight();
		}
		distance += b->GetHeight()/2.0;
		return distance;
	}
	else
	{
		double distance = a->GetHeight() - a->GetHeight()*pos;
		while (a->GetAbove() != b)
		{
			if (!a->GetAbove())
				throw runtime_error(showTestCase() + "Trying to dereference nullptr in getDistance() loop.");

			a = a->GetAbove();
			distance += a->GetHeight();
		}
		distance += b->GetHeight()/2.0;
		return distance;
	}
}

// get travel time of an elevator from one floor to the other
int ElevatorLogic::getTravelTime(Elevator *ele, Floor *a, Floor *b, bool direct)
{
	if (!ele || !a || !b)
		throw runtime_error(showTestCase() + "Trying to dereference nullptr in getTravelTime().");

	if (direct)
		return ceil(getDistance(a,b,ele->GetPosition())/ele->GetSpeed());
	else
		return ceil(getDistance(a,b)/ele->GetSpeed());
}

// get travel time to a floor considering an existing queue
int ElevatorLogic::getQueueLength(Elevator *ele, Floor* target)
{
	if (!ele || !target)
		throw runtime_error(showTestCase() + "Trying to dereference nullptr in getQueueLength().");

	// catch possible null pointer
	if (queueInt_.find(ele) == queueInt_.end() || queueExt_.find(ele) == queueExt_.end())
		throw runtime_error(showTestCase() + "An elevator was trying to calculate travel time without having a queue.");

	// create merged queue
	set<Floor*> q;
	q.insert(queueInt_[ele].begin(),queueInt_[ele].end());
	q.insert(queueExt_[ele].begin(),queueExt_[ele].end());

	// on empty queue or unmoved elevator
	if (q.empty() || (!movingUp_.count(ele) && !movingDown_.count(ele)))
		// get direct travel time to target
		return getTravelTime(ele,ele->GetCurrentFloor(),target,true);

	int result = 0;
	Floor *prev = ele->GetCurrentFloor();
	Floor *cur = ele->GetCurrentFloor();


	// straightforward path
	if (movingUp_.count(ele) && onTheWay(ele,target))
	{
		while (cur->GetAbove() && cur != target)
		{
			if (q.count(cur))
			{
				// also consider time for opening/closing door
				result += getTravelTime(ele,prev,cur) + 6;
				prev = cur;
			}
			cur = cur->GetAbove();
		}
	}
	else if (movingDown_.count(ele) && onTheWay(ele,target))
	{
		while (cur->GetAbove() && cur != target)
		{
			if (q.count(cur))
			{
				result += getTravelTime(ele,prev,cur) + 6;
				prev = cur;
			}
			cur = cur->GetBelow();
		}
	}
	// more complex path
	else if (movingUp_.count(ele) && !onTheWay(ele,target))
	{
		// get all the way to the top
		while (cur != nullptr)
		{
			if (q.count(cur))
			{
				// also consider time for opening/closing door
				result += getTravelTime(ele,prev,cur) + 6;
				prev = cur;
			}
			cur = cur->GetAbove();
		}
		// go down the whole down queue until target
		cur = ele->GetCurrentFloor();
		prev = cur;
		while (cur->GetAbove() && cur != target)
		{
			if (q.count(cur))
			{
				result += getTravelTime(ele,prev,cur) + 6;
				prev = cur;
			}
			cur = cur->GetBelow();
		}
	}
	else if (movingDown_.count(ele) && !onTheWay(ele,target))
	{
		// get all the way to the top
		while (cur != nullptr)
		{
			if (q.count(cur))
			{
				// also consider time for opening/closing door
				result += getTravelTime(ele,prev,cur) + 6;
				prev = cur;
			}
			cur = cur->GetBelow();
		}
		// go down the whole down queue until target
		cur = ele->GetCurrentFloor();
		prev = cur;
		while (cur->GetAbove() && cur != target)
		{
			if (q.count(cur))
			{
				result += getTravelTime(ele,prev,cur) + 6;
				prev = cur;
			}
			cur = cur->GetAbove();
		}
	}
	else
	{
		// this should absolutely not happen, but who knows
		DEBUG_S("Case not considered");
		DEBUG_V(ele->GetId());
		DEBUG_V(onTheWay(ele,target));
		DEBUG_V(q.empty());
		DEBUG_V(moving_.count(ele));
		DEBUG_V(movingUp_.count(ele));
		DEBUG_V(movingDown_.count(ele));
		throw runtime_error(showTestCase() + "Could not find proper queue length for an elevator.");
	}
	return (result + getTravelTime(ele,prev,target));
}

// add elevator to a list sorted by time to travel to a target floor
void ElevatorLogic::addToList(list<Elevator*> &elevs, Elevator* ele, Floor* target)
{
	if (!ele || !target)
		throw runtime_error(showTestCase() + "Trying to dereference nullptr in addToList().");

	DEBUG_S("Considering elevator " << ele->GetId() << " at floor " << ele->GetCurrentFloor()->GetId() <<
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

// pick the elevator with shortest travel time to target
Elevator* ElevatorLogic::pickElevator(Interface *interf, Floor *target)
{
	if (!interf || !target)
		throw runtime_error(showTestCase() + "Trying to dereference nullptr in pickElevator().");

	// get all elevators that stop at this floor
	list<Elevator*> elevs;
	for(int i = 0; i < interf->GetLoadableCount(); ++i)
	{
		Elevator *ele = static_cast<Elevator*>(interf->GetLoadable(i));

		// add new elevator with empty queue global map
		if (queueInt_.find(ele) == queueInt_.end() || queueExt_.find(ele) == queueExt_.end())
		{
			set<Floor*> q;
			// WARNING: we rely on the hope that this COPIES the created pair
			if (queueInt_.insert(make_pair(ele,q)).second)
				DEBUG_S("[Elevator " << ele->GetId() << "] Internal queue created");
			if (queueExt_.insert(make_pair(ele,q)).second)
				DEBUG_S("[Elevator " << ele->GetId() << "] External queue created");
		}

		// exclude overloaded
		bool overloaded = false;
		auto iter = loads_.find(ele);
		if (iter != loads_.end())
			if (iter->second > ele->GetMaxLoad())
				overloaded = true;

		// exclude malfunctioning
		if (!malfunctions_.count(ele) && !overloaded && canReachFloor(ele,target))
			// sort them by estimated travel time
			addToList(elevs,ele,target);
	}

	// get idling at current floor
	for (auto const& ele : elevs)
	{
		if (!moving_.count(ele) && ele->GetCurrentFloor() == target)
			return ele;
	}

	// get on the way
	for (auto const& ele : elevs)
	{
		if (onTheWay(ele,target))
			return ele;
	}

	// pick the one which would be there first
	if (!elevs.empty())
		return elevs.front();
	else
		return nullptr;
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
		"}<br>" << endl;
	}
	return oss.str();
}

void ElevatorLogic::collectInfo(Environment &env, Person *person)
{
	if (!person)
		throw runtime_error(showTestCase() + "Trying to dereference nullptr in collectInfo().");

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
	return (showFloors() + showElevators() + showPersons() + showInterfaces() + showEvents() + eventlog);
}

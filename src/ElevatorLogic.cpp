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

ElevatorLogic::ElevatorLogic() : EventHandler("ElevatorLogic"), moved_(false) {}

ElevatorLogic::~ElevatorLogic() {}

// register all the handlers
void ElevatorLogic::Initialize(Environment &env)
{
	env.RegisterEventHandler("Interface::Notify", this, &ElevatorLogic::HandleNotify);
	env.RegisterEventHandler("Elevator::Moving", this, &ElevatorLogic::HandleMoving);
	env.RegisterEventHandler("Elevator::Stopped", this, &ElevatorLogic::HandleStopped);
	env.RegisterEventHandler("Elevator::Opened", this, &ElevatorLogic::HandleOpened);
	env.RegisterEventHandler("Elevator::Closed", this, &ElevatorLogic::HandleClosed);
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

	// FYI
	std::cout << "Person " << person->GetId() << " wants to go to floor " << person->GetFinalFloor()->GetId() << std::endl;
	
	// check if interface comes from inside an elevator or from a floor
	// we see this from looking at the type of the interface's first loadable
	Loadable *loadable = interf->GetLoadable(0);

	// react to an interface interaction from outside the elevator
	if (loadable->GetType() == "Elevator")
	{
		// get all elevators that stop at this interface
		list<Elevator*> elevators;
		for(int i = 0; i < interf->GetLoadableCount(); i++)
		{
			// cast the loadables to elevator pointers
			Elevator *ele = static_cast<Elevator*>(interf->GetLoadable(i));
			std::cout << interf->GetLoadable(i)->GetType() << std::endl;

			elevators.push_back(ele);

			// while we're at it, add these elevators to our global map
			// using default values for its state:
			// empty passenger list
			set<Person*> passengers;
			// closed door and not moving
			pair<Elevator*,State> state = {ele,DEFAULT_STATE};

			// put it into the global map (doesn't do anything if already exists)
			elevatorState_.insert(state);
		}

		// check if any elevator is already at the calling person
		for(list<Elevator*>::iterator i = elevators.begin(); i != elevators.end(); ++i)
		{
			// take the first elevator that is at the right floor
			if ((*i)->GetCurrentFloor() == person->GetCurrentFloor())
			{
				// let the person in
				openDoor(env, 0, *i);
				return;
			}
		}

		// if there is no elevator at the calling persons floor
		for(list<Elevator*>::iterator i = elevators.begin(); i != elevators.end(); ++i)
		{
			// take the first idle elevator that goes to that floor
			if ((*i)->HasFloor(person->GetFinalFloor()))
			{
				// go to the caller's floor
				SendToFloor(env,person->GetCurrentFloor(),*i);
				return;
			}
		}

		// //FYI
		// std::cout << "Elevator currently at Floor " << ele->GetCurrentFloor()->GetId() << std::endl;

		// // let the appropriate elevator open doors
		// env.SendEvent("Elevator::Open", 0, this, ele);
	}
	// react to an interface interaction from inside the elevator
	else
	{
		// get our current elevator by asking the person where it is
		Elevator *ele = person->GetCurrentElevator();
		// get target from the interface input
		Floor *target = static_cast<Floor*>(loadable);
		
		//FYI
		std::cout << "Elevator currently at Floor " << ele->GetCurrentFloor()->GetId() << std::endl;
		
		// close doors
		env.SendEvent("Elevator::Close", 0, this, ele);
		
		// send elevator to where the person wants to go
		// WARNING: Assumes person is inside...
		SendToFloor(env,target,ele);
	}
}

// open doors immediately after stopping
void ElevatorLogic::HandleStopped(Environment &env, const Event &e)
{

	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	// only open doors if we're at the middle of a floor
	if (ele->GetPosition() > 0.49 && ele->GetPosition() < 0.51)
	{
		env.SendEvent("Elevator::Open", 0, this, ele);
	}
}


// close doors after 4 seconds
// WARNING: Hardcoded time
void ElevatorLogic::HandleOpened(Environment &env, const Event &e)
{

// 	Elevator *ele = static_cast<Elevator*>(e.GetSender());
// 
// 	env.SendEvent("Elevator::Close", 4, this, ele);
}

// after closing doors, go up for 4 seconds
// WARNING: Hardcoded!!!
void ElevatorLogic::HandleClosed(Environment &env, const Event &e)
{

// 	Elevator *ele = static_cast<Elevator*>(e.GetSender());
// 
// 	// only start moving if we're not moving yet
// 	// and if we're not already on the highest floor
// 	if (!moved_ && !ele->IsHighestFloor(ele->GetCurrentFloor()))
// 	{
// 		env.SendEvent("Elevator::Up", 0, this, ele);
// 		env.SendEvent("Elevator::Stop", 4, this, ele);
// 
// 		moved_ = true;
// 	}
// 	else
// 	{
// 		std::cout << "Already on highest floor!" << std::endl;
// 	}
}

// react to elevators movement status
void ElevatorLogic::HandleMoving(Environment &env, const Event &e)
{
	// NOTE: we assume the sender of a movement event is always an elevator
	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	// FYI
	// get current floor id we're at
	int floor = ele->GetCurrentFloor()->GetId();
	
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
		default:
			break;
	}
	std::cout << "Current floor: " << floor << " (" << ele->GetPosition() << "), " << state;
	
	// NOTE: when we start moving the elevator, it sends a first notification automatically which has no data.
	if(!e.GetData().empty())
	{
		// get the floor id we want to reach
		int tgt_floor = std::stoi(e.GetData());
		
		// FYI
		std::cout << " to floor " << tgt_floor << std::endl;
		
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
	else
	{
		std::cout << std::endl;
	}
	
}

// send elevator into general direction of given floor
// WARNING: does not check if floor is reachable.
void ElevatorLogic::SendToFloor(Environment &env, Floor *target, Elevator *ele)
{
	// find out current floor
	Floor *current = ele->GetCurrentFloor();
	
	// don't do anything if we're already there
	if (target == current)
	{
		std::cout << "Target floor already reached" << std::endl;
		return;
	}
	
	// close door before starting
	closeDoor(env,0,ele);

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
	
	// afterwards, let the elevator report after every tick
	// (evil hack! we're misusing our ability to let the elevator send a message and pretend it does so on its own. the only reason is that (AFAIK) we can't do anything else to have a read-only interaction, since we can't declare new events for this class)
	// pass the target floor id as data string
	env.SendEvent("Elevator::Moving",4,ele,ele,std::to_string(target->GetId()));
	// @see HandleMoving
}


void ElevatorLogic::openDoor(Environment &env, int delay, Elevator* ele)
{
	// only open door if stopped in the middle of a floor
	bool stoppedProperly = !elevatorState_[ele].isMoving && ele->GetPosition() > 0.49 && ele->GetPosition() < 0.51;
	// only open door if it is already closed or currently closing
	bool doorClosed = elevatorState_[ele].doorState == Closed || elevatorState_[ele].doorState == Closing;

	if (stoppedProperly && doorClosed)
	{
		env.SendEvent("Elevator::Open", delay, this, ele);
	}
}

void ElevatorLogic::closeDoor(Environment &env, int delay, Elevator* ele)
{
	// only open door if stopped in the middle of a floor
	bool beeping = elevatorState_[ele].isBeeping;
	// only open door if it is already closed or currently closing
	bool doorOpen = elevatorState_[ele].doorState == Open || elevatorState_[ele].doorState == Opening;

	if (!beeping && doorOpen)
	{
		env.SendEvent("Elevator::Close", delay, this, ele);
	}
}


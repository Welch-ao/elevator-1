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

ElevatorLogic::ElevatorLogic() : EventHandler("ElevatorLogic"), moved_(false) {
}

ElevatorLogic::~ElevatorLogic() {
}

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
	Interface *interf = static_cast<Interface*>(e.GetSender());
	Person *person = static_cast<Person*>(e.GetEventHandler());
	
	// FYI
	std::cout << "Person " << person->GetId() << " wants to go to floor " << person->GetFinalFloor()->GetId() << std::endl;
	
	// check if message really goes to elevator
	// WARNING:Hardcoded to first entry, maybe should search for the right elevator here
	Loadable *loadable = interf->GetLoadable(0);

	if (loadable->GetType() == "Elevator")
	{
		// do cast magic to pull out a pointer to an elevator object
		Elevator *ele = static_cast<Elevator*>(loadable);
		//FYI
		std::cout << "Elevator currently at Floor " << ele->GetCurrentFloor()->GetId() << std::endl;

		// let the appropriate elevator open doors
		env.SendEvent("Elevator::Open", 0, this, ele);
	}
	// WARNING: In the basic test case this other loadable is the floor, but may be anything else later
	// here we react to an interface interaction from inside the elevator
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
void ElevatorLogic::SendToFloor(Environment &env,Floor *target,Elevator *ele)
{
	// find out current floor
	Floor *current = ele->GetCurrentFloor();
	
	// don't do anything if we're already there
	if (target == current)
	{
		std::cout << "Target floor already reached" << std::endl;
		return;
	}
	
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
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
	env.RegisterEventHandler("Elevator::Stopped", this, &ElevatorLogic::HandleStopped);
	env.RegisterEventHandler("Elevator::Opened", this, &ElevatorLogic::HandleOpened);
	env.RegisterEventHandler("Elevator::Closed", this, &ElevatorLogic::HandleClosed);
}

// 
void ElevatorLogic::HandleNotify(Environment &env, const Event &e)
{
	// grab interface that sent notification
	Interface *interf = static_cast<Interface*>(e.GetSender());
	Person *person = static_cast<Person*>(e.GetEventHandler());
	std::cout << "Person " << person->GetId() << " wants to go to floor " << person->GetFinalFloor()->GetId() << std::endl;

	// check if message really goes to elevator
	// WARNING:Hardcoded to first entry, maybe should search for the right elevator here
	Loadable *loadable = interf->GetLoadable(0);

	if (loadable->GetType() == "Elevator")
	{
		// do cast magic to pull out a pointer to an elevator object
		Elevator *ele = static_cast<Elevator*>(loadable);

		// let the appropriate elevator open doors
		env.SendEvent("Elevator::Open", 0, this, ele);
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

	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	env.SendEvent("Elevator::Close", 4, this, ele);
}

// after closing doors, go up for 4 seconds
// WARNING: Hardcoded!!!
void ElevatorLogic::HandleClosed(Environment &env, const Event &e)
{

	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	// only start moving if we're not moving yet
	// and if we're not already on the highest floor
	if (!moved_ && !ele->IsHighestFloor(ele->GetCurrentFloor()))
	{
		env.SendEvent("Elevator::Up", 0, this, ele);
		env.SendEvent("Elevator::Stop", 4, this, ele);

		moved_ = true;
	}
	else
	{
		std::cout << "Already on highest floor!" << std::endl;
	}
}

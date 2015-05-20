/*
 * Person.cpp
 *
 *  Created on: 18.06.2014
 *      Author: STSJR
 */

#include "Person.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "Floor.h"
#include "Interface.h"
#include "Elevator.h"
#include "Event.h"
#include "Environment.h"

Person::Person() :
		Entity("Person"), current_(0), final_(0), giveup_(0), weight_(0), start_(0), requested_(nullptr), elevator_(nullptr), beeping_(
				false), action_(0) {
}

Person::~Person() {
}

Floor *Person::GetCurrentFloor() const {
	return current_;
}

Floor *Person::GetFinalFloor() const {
	return final_;
}

int Person::GetGiveUpTime() const {
	return giveup_;
}

int Person::GetWeight() const {
	return weight_;
}

Elevator *Person::GetCurrentElevator() const {
	return elevator_;
}

void Person::DeclareEvents(Environment &env) {
	env.DeclareEvent("Person::Entering");
	env.DeclareEvent("Person::Entered");
	env.DeclareEvent("Person::Exiting");
	env.DeclareEvent("Person::Exited");
	env.DeclareEvent("Person::Canceled");
}

void Person::Initialize(Environment &env) {
	env.RegisterEventHandler("UpDownButton::Decide", this, &Person::HandleDecide);
	env.RegisterEventHandler("Elevator::Opened", this, &Person::HandleOpened);
	env.RegisterEventHandler("Elevator::Beeping", this, &Person::HandleBeeping);
	env.RegisterEventHandler("Elevator::Beeped", this, &Person::HandleBeeped);
	env.RegisterEventHandler("Person::Entered", this, &Person::HandleEntered);
	env.RegisterEventHandler("Person::Exiting", this, &Person::HandleExiting);
	env.RegisterEventHandler("Person::Exited", this, &Person::HandleExited);
	env.RegisterEventHandler("Elevator::Moving", this, &Person::HandleMoving);
	env.RegisterEventHandler("Elevator::Fixed", this, &Person::HandleFixed);

	std::list<Floor*> init;
	init.push_front(current_);

	std::list<std::list<Floor*>> paths;
	paths.push_front(init);

	do {

		std::list<Floor*> path = paths.front();
		Floor *current = path.back();

		paths.pop_front();

		if (current == final_) {
			path.pop_front();
			path_ = path;
			break;
		}

		for (int i = 0; i < current->GetInterfaceCount(); ++i) {

			Interface *interf = current->GetInterface(i);
			Elevator *ele = static_cast<Elevator*>(interf->GetLoadable(0));

			for (int j = 0; j < ele->GetInterfaceCount(); ++j) {

				Interface *binterf = ele->GetInterface(j);
				Floor *floor = static_cast<Floor*>(binterf->GetLoadable(0));

				if (floor != current) {
					path.push_back(floor);
					paths.push_back(path);
					path.pop_back();
				}
			}
		}

	} while (true);

	std::cout << "Path for " << GetName() << " is ";
	for (std::list<Floor*>::iterator it = path_.begin(); it != path_.end(); ++it) {
		Floor *floor = *it;
		std::cout << "Floor " << floor->GetId() << " ";
	}
	std::cout << std::endl;

	RequestElevator(env, start_);
}

void Person::Load(Environment &env, std::istream &is) {

	Entity::Load(env, is);

	env.LoadReference(&current_, is);
	env.LoadReference(&final_, is);
	env.LoadInteger(&giveup_, is);
	env.LoadInteger(&weight_, is);
	env.LoadInteger(&start_, is);
}

void Person::Validate() {

	Entity::Validate();

	if (!current_) {
		std::ostringstream oss("Invalid test: starting floor of ", std::ostringstream::ate);
		oss << GetName() << " is null.";
		throw std::runtime_error(oss.str());
	}

	if (current_->GetType() != "Floor") {
		std::ostringstream oss("Invalid test: starting floor of ", std::ostringstream::ate);
		oss << GetName() << " does not reference a floor object.";
		throw std::runtime_error(oss.str());
	}

	if (!final_) {
		std::ostringstream oss("Invalid test: final floor of ", std::ostringstream::ate);
		oss << GetName() << " is null.";
		throw std::runtime_error(oss.str());
	}

	if (final_->GetType() != "Floor") {
		std::ostringstream oss("Invalid test: final floor of ", std::ostringstream::ate);
		oss << GetName() << " does not reference a floor object.";
		throw std::runtime_error(oss.str());
	}

	if (giveup_ <= 0) {
		std::ostringstream oss("Invalid test: give up time of ", std::ostringstream::ate);
		oss << GetName() << " must be positive.";
		throw std::runtime_error(oss.str());
	}

	if (weight_ <= 0) {
		std::ostringstream oss("Invalid test: weight of ", std::ostringstream::ate);
		oss << GetName() << " must be positive.";
		throw std::runtime_error(oss.str());
	}

	if (start_ < 0) {
		std::ostringstream oss("Invalid test: start time of ", std::ostringstream::ate);
		oss << GetName() << " must be non-negative.";
		throw std::runtime_error(oss.str());
	}
}


void Person::HandleDecide(Environment &env, const Event &e) {

	if (ForMe(e)) {
		if (current_->IsAbove(path_.front()))
			env.SendEvent("UpDownButton::Up", 0, this, e.GetSender());

		if (current_->IsBelow(path_.front()))
			env.SendEvent("UpDownButton::Down", 0, this, e.GetSender());
	}
}

void Person::HandleOpened(Environment &env, const Event &e) {

	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	if (elevator_) {

		if (elevator_ == ele)
			if (ele->GetPosition() > 0.49 && ele->GetPosition() < 0.51)
				if (ele->GetCurrentFloor() == path_.front())
					Exit(env);

	} else if (requested_->HasLoadable(ele) && ele->GetCurrentFloor() == current_ && ele->GetPosition() > 0.49
			&& ele->GetPosition() < 0.51 && e.GetTime() > start_) {

		elevator_ = ele;
		Enter(env);
	}
}

void Person::HandleEntered(Environment &env, const Event &e) {

	if (FromMe(e))
		RequestFloor(env);
}

void Person::HandleExiting(Environment &env, const Event &e) {

	Person *person = static_cast<Person*>(e.GetSender());
	Elevator *ele = person->GetCurrentElevator();

	if (beeping_ && elevator_ == ele && !FromMe(e))
		exiting_.insert(person);
}

void Person::HandleExited(Environment &env, const Event &e) {

	Person *person = static_cast<Person*>(e.GetSender());

	if (FromMe(e)) {

		exiting_.clear();
		current_ = elevator_->GetCurrentFloor();
		elevator_ = nullptr;

		if (!beeping_) {
			path_.pop_front();
			RequestElevator(env, e.GetTime() + 3);
		}

	} else if (beeping_ && exiting_.count(person)) {
		exiting_.erase(person);
		Cancel(env);
		Exit(env);
	}
}

void Person::HandleBeeping(Environment &env, const Event &e) {

	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	if (elevator_ == ele) {

		beeping_ = true;
		env.CancelEvent(action_); // Cancel Interface::Interact
		Exit(env);
	}
}

void Person::HandleBeeped(Environment &env, const Event &e) {

	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	if (beeping_ && elevator_ == ele) {

		beeping_ = false;
		Cancel(env);
		RequestFloor(env);
	}
}

void Person::HandleMoving(Environment &env, const Event &e) {

	if (beeping_) {

		beeping_ = false;
		RequestElevator(env, e.GetTime() + 3);
	}
}

void Person::HandleFixed(Environment &env, const Event &e) {

	Elevator *ele = static_cast<Elevator*>(e.GetSender());

	if (ele == elevator_) {
		RequestFloor(env);
	}
}

void Person::Enter(Environment &env) {

	if (current_ == final_)
		return;

	env.SendEvent("Person::Entering", 0, this, elevator_);
	action_ = env.SendEvent("Person::Entered", 3, this, elevator_);
}

void Person::Exit(Environment &env) {
	env.SendEvent("Person::Exiting", 0, this, elevator_);
	action_ = env.SendEvent("Person::Exited", 3, this, elevator_);
}

void Person::Cancel(Environment &env) {

	if (action_) {

		env.CancelEvent(action_);
		env.SendEvent("Person::Canceled", 0, this);

		action_ = 0;
	}
}

void Person::RequestFloor(Environment &env) {

	Floor *next = path_.front();

	for (int i = 0; i < elevator_->GetInterfaceCount(); ++i) {

		Interface *interf = elevator_->GetInterface(i);
		Floor *floor = static_cast<Floor*>(interf->GetLoadable(0));

		if (floor == next) {
			action_ = env.SendEvent("Interface::Interact", 3, this, elevator_->GetInterface(i));

			return;
		}
	}

	std::ostringstream oss("Invalid operation: ", std::ostringstream::ate);
	oss << elevator_->GetName() << " can not go to Floor " << next->GetId();
	oss << ", this error should never occur.";
	throw std::runtime_error(oss.str());
}

void Person::RequestElevator(Environment &env, int time) {

	if (current_ == final_)
		return;

	Floor *next = path_.front();

	for (int i = 0; i < current_->GetInterfaceCount(); ++i) {

		Interface *interf = current_->GetInterface(i);
		Elevator *ele = static_cast<Elevator*>(interf->GetLoadable(0));

		if (ele->HasFloor(next)) {

			requested_ = current_->GetInterface(i);
			env.SendEvent("Interface::Interact", time - env.GetClock(), this, requested_);

			return;
		}
	}

	std::ostringstream oss("Invalid operation: no elevator available to go from Floor ", std::ostringstream::ate);
	oss << current_->GetId() << " to Floor " << next->GetId();
	oss << ", this error should never occur.";
	throw std::runtime_error(oss.str());
}


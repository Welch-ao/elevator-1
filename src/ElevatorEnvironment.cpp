/*
 * ElevatorEnvironment.cpp
 *
 *  Created on: 17.06.2014
 *      Author: STSJR
 */

#include "ElevatorEnvironment.h"

#include <set>
#include <iostream>

#include "Elevator.h"
#include "Floor.h"
#include "Interface.h"
#include "UpDownButton.h"
#include "Person.h"

ElevatorEnvironment::ElevatorEnvironment() {

	UseEntity<Interface>();
	UseEntity<UpDownButton>();
	UseEntity<Elevator>(&ElevatorEnvironment::NewElevator);
	UseEntity<Person>(&ElevatorEnvironment::NewPerson);
	UseLoadable<Floor>(&ElevatorEnvironment::NewFloor);

	RegisterEventHandler(&logic_);
}

ElevatorEnvironment::~ElevatorEnvironment() {
}

void ElevatorEnvironment::Evaluate() {

	for (std::vector<Person*>::iterator it = persons_.begin(); it != persons_.end(); ++it) {
		Person *p = *it;
		if (p->GetCurrentFloor() != p->GetFinalFloor()) {
			throw std::runtime_error("A person did not reach its desired destination floor.");
		}
	}
}

void ElevatorEnvironment::Validate() {

	if (elevators_.size() == 0)
		throw std::runtime_error("Invalid test: no elevator object was specified.");

	if (persons_.size() == 0)
		throw std::runtime_error("Invalid test: no person object was specified.");

	Floor *top = 0;
	Floor *bottom = 0;

	for (std::vector<Floor*>::iterator it = floors_.begin(); it != floors_.end(); ++it) {
		Floor *floor = *it;
		if (!floor->GetAbove()) {
			if (!top)
				top = floor;
			else
				throw std::runtime_error("Invalid test: invalid floor configuration: multiple top floors exist.");
		}
		if (!floor->GetBelow()) {
			if (!bottom)
				bottom = floor;
			else
				throw std::runtime_error("Invalid test: invalid floor configuration: multiple bottom floors exist.");
		}
	}

	if (!top)
		throw std::runtime_error("Invalid test: invalid floor configuration: floor arrangement is cyclic.");

	unsigned int c = 0;
	while (top) {
		++c;
		top = top->GetBelow();
	}

	if (c != floors_.size())
		throw std::runtime_error("Invalid test: invalid floor configuration: an additional cyclic floor arrangement exists.");

	for (std::vector<Person*>::iterator it = persons_.begin(); it != persons_.end(); ++it) {
		Person *person = *it;
		std::set<Floor*> floors;
		std::set<Elevator*> elevators;

		floors.insert(person->GetCurrentFloor());

		bool added;
		do {
			added = false;

			for (std::set<Floor*>::iterator it = floors.begin(); it != floors.end(); ++it) {
				Floor *floor = *it;
				for (int i = 0; i < floor->GetInterfaceCount(); ++i) {

					Interface *interf = floor->GetInterface(i);

					for (int j = 0; j < interf->GetLoadableCount(); ++j) {

						Elevator *ele = static_cast<Elevator*>(interf->GetLoadable(j));

						added |= elevators.insert(ele).second;
					}
				}
			}

			for (std::set<Elevator*>::iterator it = elevators.begin(); it != elevators.end(); ++it) {
				Elevator *ele = *it;
				for (int i = 0; i < ele->GetInterfaceCount(); ++i) {

					Interface *interf = ele->GetInterface(i);

					for (int j = 0; j < interf->GetLoadableCount(); ++j) {

						Floor *floor = static_cast<Floor*>(interf->GetLoadable(j));

						added |= floors.insert(floor).second;
					}
				}
			}

		} while (added);

		if (!floors.count(person->GetFinalFloor())) {
			std::ostringstream oss("Invalid test: ", std::ostringstream::ate);
			oss << person->GetName() << " has unreachable final floor: Floor " << person->GetFinalFloor()->GetId() << ".";
			throw std::runtime_error(oss.str());
		}
	}
}

void ElevatorEnvironment::NewElevator(Elevator *elevator) {
	elevators_.push_back(elevator);
}

void ElevatorEnvironment::NewFloor(Floor *floor) {
	floors_.push_back(floor);
}

void ElevatorEnvironment::NewPerson(Person *person) {
	persons_.push_back(person);
}

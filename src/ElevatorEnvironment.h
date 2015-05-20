/*
 * ElevatorEnvironment.h
 *
 *  Created on: 17.06.2014
 *      Author: STSJR
 */

#ifndef ELEVATORENVIRONMENT_H_
#define ELEVATORENVIRONMENT_H_

#include "Environment.h"

#include "ElevatorLogic.h"

class ElevatorLogic;
class Floor;
class Elevator;
class Person;

class ElevatorEnvironment: public Environment {

public:
	ElevatorEnvironment();
	virtual ~ElevatorEnvironment();

	void Evaluate();
	void Validate();

private:

	void NewElevator(Elevator *elevator);
	void NewFloor(Floor *floor);
	void NewPerson(Person *person);

	ElevatorLogic logic_;
	std::vector<Floor*> floors_;
	std::vector<Elevator*> elevators_;
	std::vector<Person*> persons_;
};

#endif /* ELEVATORENVIRONMENT_H_ */

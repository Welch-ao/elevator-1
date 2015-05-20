/*
 * Elevator.h
 *
 *  Created on: 18.06.2014
 *      Author: STSJR
 */

#ifndef ELEVATOR_H_
#define ELEVATOR_H_

#include "Entity.h"

class Floor;
class Interface;

class Elevator: public Entity {
public:

	enum Movement {
		Idle,
		Up,
		Down,
		Malfunction
	};

	Elevator();
	virtual ~Elevator();

	bool HasFloor(Floor *floor) const;
	bool IsLowestFloor(Floor *floor) const;
	bool IsHighestFloor(Floor *floor) const;

	int GetSpeed() const;
	int GetMaxLoad() const;
	Floor *GetCurrentFloor() const;

	int GetInterfaceCount() const;
	Interface *GetInterface(int n) const;

	double GetPosition() const;
	Movement GetState() const;

	void DeclareEvents(Environment &env);
	void Initialize(Environment &env);

	void Load(Environment &env, std::istream &is);
	void Validate();

private:

	void HandleUpdate(Environment &env, const Event &e);
	void HandleUp(Environment &env, const Event &e);
	void HandleDown(Environment &env, const Event &e);
	void HandleStop(Environment &env, const Event &e);
	void HandleOpen(Environment &env, const Event &e);
	void HandleClose(Environment &env, const Event &e);
	void HandleBeep(Environment &env, const Event &e);
	void HandleBeeping(Environment &env, const Event &e);
	void HandleStopBeep(Environment &env, const Event &e);
	void HandleMalfunction(Environment &env, const Event &e);
	void HandleFixed(Environment &env, const Event &e);

	int speed_;
	int load_;
	Floor *current_;
	int count_;
	Interface** interfs_;

	double position_;
	int lasttime_;
	Movement state_;
	int beeping_;
};

#endif /* ELEVATOR_H_ */

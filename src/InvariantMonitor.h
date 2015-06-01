/* * InvariantMonitor.h
 * * Created on: 20.06.2014
 * * Author: STSJR
*/
#ifndef INVARIANTMONITOR_H_
#define INVARIANTMONITOR_H_
#include "EventHandler.h"

#include <map>
#include <set>
#include <chrono>

class Elevator;
class Person;

class InvariantMonitor: public EventHandler
{
    public: InvariantMonitor();
    virtual ~InvariantMonitor();
    void Initialize(Environment &env);
    private: void HandleMoving(Environment &env, const Event &e);
    void HandleStopped(Environment &env, const Event &e);
    void HandleOpening(Environment &env, const Event &e);
    void HandleClosed(Environment &env, const Event &e);
    void HandleEntering(Environment &env, const Event &e);
    void HandleEntered(Environment &env, const Event &e);
    void HandleExited(Environment &env, const Event &e);
    void HandleBeeping(Environment &env, const Event &e);
    void HandleBeeped(Environment &env, const Event &e);
    void HandleClosing(Environment &env, const Event &e);
    void HandleMalfunction(Environment &env, const Event &e);
    void HandleFixed(Environment &env, const Event &e);
    void HandleInteract(Environment &env, const Event &e);
    void HandleAll(Environment &env, const Event &e);
    std::set<Elevator*> elevators_;
    std::set<Elevator*> moving_;
    std::set<Elevator*> open_;
    std::set<Elevator*> beeping_;
    std::set<Elevator*> malfunctions_;
    std::map<Elevator*,int> loads_;
    std::map<Person*,int> deadlines_;
    std::chrono::steady_clock::time_point start;
};

#endif /* INVARIANTMONITOR_H_ */
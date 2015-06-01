
/* * InvariantMonitor.cpp
 * * Created on: 20.06.2014
 * * Author: STSJR */
 #include "InvariantMonitor.h"
#include "Elevator.h"
#include "Person.h"
#include "Event.h"
#include "Environment.h"
#include "Interface.h"

InvariantMonitor::InvariantMonitor() : EventHandler("InvariantMonitor") {}
InvariantMonitor::~InvariantMonitor() {}

void InvariantMonitor::Initialize(Environment &env)
{
    env.RegisterEventHandler("Elevator::Moving", this, &InvariantMonitor::HandleMoving);
    env.RegisterEventHandler("Elevator::Stopped", this, &InvariantMonitor::HandleStopped);
    env.RegisterEventHandler("Elevator::Opening", this, &InvariantMonitor::HandleOpening);
    env.RegisterEventHandler("Elevator::Closed", this, &InvariantMonitor::HandleClosed);
    env.RegisterEventHandler("Person::Entered", this, &InvariantMonitor::HandleEntered);
    env.RegisterEventHandler("Person::Entering", this, &InvariantMonitor::HandleEntering);
    env.RegisterEventHandler("Person::Exited", this, &InvariantMonitor::HandleExited);
    env.RegisterEventHandler("Elevator::Beeping", this, &InvariantMonitor::HandleBeeping);
    env.RegisterEventHandler("Elevator::Beeped", this, &InvariantMonitor::HandleBeeped);
    env.RegisterEventHandler("Elevator::Closing", this, &InvariantMonitor::HandleClosing);
    env.RegisterEventHandler("Elevator::Malfunction", this, &InvariantMonitor::HandleMalfunction);
    env.RegisterEventHandler("Elevator::Fixed", this, &InvariantMonitor::HandleFixed);
    env.RegisterEventHandler("Interface::Interact", this, &InvariantMonitor::HandleInteract);
    env.RegisterEventHandler("Environment::All", this, &InvariantMonitor::HandleAll);
    start = std::chrono::steady_clock::now();
}

void InvariantMonitor::HandleMoving(Environment &env, const Event &e)
{
    Elevator *ele = static_cast<Elevator*>(e.GetSender());
    auto iter = loads_.find(ele);
    if (iter != loads_.end())
        if (iter->second > ele->GetMaxLoad())
            throw std::runtime_error("An elevator started moving while exceeding its maximum load.");
    if (malfunctions_.count(ele))
        throw std::runtime_error("An elevator started moving while it was malfunctioning.");
    if (open_.count(ele))
        throw std::runtime_error("An elevator started moving while its doors were open.");
    moving_.insert(ele);
    elevators_.insert(ele);
}

void InvariantMonitor::HandleStopped(Environment &env, const Event &e)
{
    Elevator *ele = static_cast<Elevator*>(e.GetSender());
    moving_.erase(ele);
}

void InvariantMonitor::HandleOpening(Environment &env, const Event &e)
{
    Elevator *ele = static_cast<Elevator*>(e.GetSender());
    if (ele->GetPosition() <= 0.49 || ele->GetPosition() >= 0.51)
        throw std::runtime_error("An elevator opened its doors when it was not at the center of a floor.");
    if (moving_.count(ele))
        throw std::runtime_error("An elevator opened its doors while it was moving.");
    open_.insert(ele);
}

void InvariantMonitor::HandleClosed(Environment &env, const Event &e)
{
    Elevator *ele = static_cast<Elevator*>(e.GetSender());
    open_.erase(ele);
}

void InvariantMonitor::HandleEntering(Environment &env, const Event &e)
{
    Person *person = static_cast<Person*>(e.GetSender());
    deadlines_.erase(deadlines_.find(person));
}

void InvariantMonitor::HandleEntered(Environment &env, const Event &e)
{
    Person *person = static_cast<Person*>(e.GetSender());
    Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
    auto iter = loads_.insert(std::make_pair(ele, person->GetWeight()));
    if (!iter.second)
        iter.first->second += person->GetWeight();
}

void InvariantMonitor::HandleExited(Environment &env, const Event &e)
{
    Person *person = static_cast<Person*>(e.GetSender());
    Elevator *ele = static_cast<Elevator*>(e.GetEventHandler());
    auto iter = loads_.find(ele);
    iter->second -= person->GetWeight();
}

void InvariantMonitor::HandleBeeping(Environment &env, const Event &e)
{
    Elevator *ele = static_cast<Elevator*>(e.GetSender());
    if (!open_.count(ele))
        throw std::runtime_error("An elevator started beeping while its doors were closed.");
    auto iter = loads_.find(ele);
    if (iter == loads_.end())
        throw std::runtime_error("An elevator started elevator although it was not overloaded.");
    if (iter->second <= ele->GetMaxLoad())
        throw std::runtime_error("An elevator started elevator although it was not overloaded.");
    beeping_.insert(ele);
}

void InvariantMonitor::HandleBeeped(Environment &env, const Event &e)
{
    Elevator *ele = static_cast<Elevator*>(e.GetSender());
    beeping_.erase(ele);
}

void InvariantMonitor::HandleClosing(Environment &env, const Event &e)
{
    Elevator *ele = static_cast<Elevator*>(e.GetSender());
    if (beeping_.count(ele))
        throw std::runtime_error("An elevator closed the doors while it was beeping.");
}

void InvariantMonitor::HandleMalfunction(Environment &env, const Event &e)
{
    Elevator *ele = static_cast<Elevator*>(e.GetSender());
    malfunctions_.insert(ele);
}

void InvariantMonitor::HandleFixed(Environment &env, const Event &e)
{
    Elevator *ele = static_cast<Elevator*>(e.GetSender());
    malfunctions_.erase(ele);
}

void InvariantMonitor::HandleInteract(Environment &env, const Event &e)
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

void InvariantMonitor::HandleAll(Environment &env, const Event &e)
{
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::chrono::duration<long int, std::ratio<1, 100000000>> elapsed = std::chrono::duration_cast<std::chrono::duration<long int,std::ratio<1, 100000000>>>(now - start);
    if (elapsed.count() > 120)
        throw std::runtime_error("Test run exceeded its maximum run time of 120 seconds.");

    for (auto pair : deadlines_)
    {
        if (e.GetTime() > pair.second)
            throw std::runtime_error("A person gave up waiting for an elevator.");
    }

    for (Elevator *ele : elevators_)
    {
        Floor *current = ele->GetCurrentFloor();
        if (ele->GetPosition() > 0.51)
            if (ele->IsHighestFloor(current))
                throw std::runtime_error("An elevator crashed into the ceiling of its elevator shaft.");
        if (ele->GetPosition() < 0.49)
            if (ele->IsLowestFloor(current))
                throw std::runtime_error("An elevator crashed into the floor of its elevator shaft.");
    }
}
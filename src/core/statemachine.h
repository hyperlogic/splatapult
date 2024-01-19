/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <functional>
#include <map>
#include <vector>

#include "log.h"

template <typename State>
class StateMachine
{
public:
    using ProcessCallback = std::function<void(float)>;
    using VoidCallback = std::function<void(void)>;
    using BoolCallback = std::function<bool(void)>;

protected:
    struct TransitionStruct
    {
        TransitionStruct(const BoolCallback& cbIn, State stateIn, std::string nameIn) : cb(cbIn), state(stateIn), name(nameIn) {}
        BoolCallback cb;
        State state;
        std::string name;
    };
    struct StateStruct
    {
        StateStruct(const VoidCallback& enterIn, const VoidCallback& exitIn, const ProcessCallback& processIn) : enter(enterIn), exit(exitIn), process(processIn) {}
        VoidCallback enter;
        VoidCallback exit;
        ProcessCallback process;
        std::vector<TransitionStruct> transitionVec;
    };
    using StatePair = std::pair<State, StateStruct>;

public:
    StateMachine(State defaultState) : state(defaultState), debug(false)
    {
        ;
    }

    void AddState(State state, const std::string& name, const VoidCallback& enter, const VoidCallback& exit, const ProcessCallback& process)
    {
        StatePair sp(state, StateStruct(enter, exit, process));
        stateStructMap.insert(sp);
        stateNameMap.insert(std::pair<State, std::string>(state, name));
    }

    void AddTransition(State state, State newState, const std::string& name, const BoolCallback& transitionCb)
    {
        stateStructMap.at(state).transitionVec.push_back(TransitionStruct(transitionCb, newState, name));
    }

    void Process(float dt)
    {
        for (auto&& trans : stateStructMap.at(state).transitionVec)
        {
            if (trans.cb())
            {
                ChangeState(trans.state, trans.name);
            }
        }
        stateStructMap.at(state).process(dt);
    }

    void ChangeState(State newState, const std::string& reason)
    {
        if (debug)
        {
            Log::D("StateChange from %s -> %s, (%s)\n", stateNameMap.at(state).c_str(), stateNameMap.at(newState).c_str(), reason.c_str());
        }
        stateStructMap.at(state).exit();
        stateStructMap.at(newState).enter();
        state = newState;
    }

    void SetDebug(bool debugIn) { debug = debugIn; }

protected:

    State state;
    std::map<State, StateStruct> stateStructMap;
    std::map<State, std::string> stateNameMap;
    bool debug;
};

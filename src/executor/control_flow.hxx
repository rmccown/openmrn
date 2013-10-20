/** \copyright
 * Copyright (c) 2013, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file control_flow.hxx
 *
 * Class running processes with asynchronous I/O.
 *
 * @author Balazs Racz
 * @date 5 Aug 2013
 */

#ifndef _EXECUTOR_CONTROL_FLOW_HXX_
#define _EXECUTOR_CONTROL_FLOW_HXX_

#include "executor/executor.hxx"

class ControlFlow : public Executable {
public:
  // =============== Interface to the outside world ===============
  ControlFlow(Executor* executor)
    : state_(&ControlFlow::NotStarted), executor_(executor) {}
  
  //! Callback from the executor. This method will be run every time the
  //! control flow is scheduled on the executor.
  virtual void Run() {
    HASSERT(state_);
    do {
      ControlFlowAction action = (this->*state_)();
      // We got a WaitForNotification.
      if (!action.next_state_) return;
      state_ = action.next_state_;
    } while (1);
  }

  //! Wakes up this control flow and puts it onto the executor's scheduling
  //! queue.
  void Notify() {
    LockHolder h(executor_);
    if (!executor_->IsMaybePending(this)) {
      executor_->Add(this);
    }
  }

  //! Returns true if this control flow has reached the terminated state.
  bool IsDone() {
    return state_ == &ControlFlow::Terminated;
  }

  Executor* executor() { return executor_; }

  // ============ Interface to children (actual flows) ==============
protected:
  struct ControlFlowAction;

public:
  //! The prototype of the member functions of all control flow stages.
  typedef ControlFlowAction (ControlFlow::*MemberFunction)();

protected:
  struct ControlFlowAction {
    ControlFlowAction(MemberFunction s) : next_state_(s) {}
    MemberFunction next_state_;
  };

protected:
  //! This function should be used to start the flow. The caller is responsible
  //! to ensure that the flow is not running at this moment. The flow will be
  //! scheduled onto its own executor, so it is safe to call this from a
  //! different thread.
  void StartFlowAt(MemberFunction f) {
    YieldAndCall(f);
  }

  // ==========  ACTION COMMANDS =============

  //! Suspends the execution of the current control flow until an external
  //! notification arrives. After the notification the current state will be
  //! re-tried.
  ControlFlowAction WaitForNotification() {
    return ControlFlowAction(nullptr);
  }

  //! Transition to a new state, and calls the new state handler immediately
  //! following the current handler.
  ControlFlowAction CallImmediately(MemberFunction f) {
    return ControlFlowAction(f);
  }

  //! Transitions to a new state, but allows all other pending callbacks of the
  //! executor to run before proceeding to the next state.
  ControlFlowAction YieldAndCall(MemberFunction f) {
    state_ = f;
    Notify();
    return WaitForNotification();
  }

  //! Transitions to a new state, but waits for a notification first.
  ControlFlowAction WaitAndCall(MemberFunction f) {
    state_ = f;
    return WaitForNotification();
  }

  //! Yields to other callbacks in the current executor, and re-tries the
  //! current state again.
  ControlFlowAction yield() {
    Notify();
    return WaitForNotification();
  }

  ControlFlowAction Exit() {
    return CallImmediately(&ControlFlow::Terminated);
  }

  struct SleepData {
    SleepData()
      : callback_count(0), timer_handle(NULL) {};
    ~SleepData();
    int callback_count;
    os_timer_t timer_handle;
  };

  ControlFlowAction Sleep(SleepData* data, long long delay_nsec,
                          MemberFunction next_state);

  //! Sets up a repeated timer call. The individual waits have to be
  //! instantiated using the WaitForTimerWakeUpAndCall call. After this has
  //! been called, the SleepData must not be used for a regular Sleep call.
  void WakeUpRepeatedly(SleepData* data, long long period_nsec);

  //! Suspends the current control flow until a repeated timer event has come
  //! in. Precondition: WakeUpRepeatedly has been called previously.
  ControlFlowAction WaitForTimerWakeUpAndCall(SleepData* data,
                                              MemberFunction next_state);

  //! Calls a child control flow, and when that flow is completed, transitions
  //! to next_state. Will send a notification to the dst flow.
  ControlFlowAction CallFlow(ControlFlow* dst, MemberFunction next_state);


private:
  //! Implementation state for a not-yet-started control flow.
  ControlFlowAction NotStarted();
  //! Implementation state for an exited control flow.
  ControlFlowAction Terminated();
  //! Implementation state that is waiting for a timer callback.
  ControlFlowAction WaitForTimer();
  //! Implementation state that is waiting for another flow to finish.
  ControlFlowAction WaitForControlFlow();

  //! Helper function for timer implementation.
  void NotifyControlFlowTimer(SleepData* entry);
  static long long control_flow_single_timer(void* arg_flow, void* arg_entry);
  static long long control_flow_repeated_timer(void* arg_flow, void* arg_entry);


  union {
    SleepData* sleep;
    ControlFlow* called_flow;

  } sub_flow_;

  //! The current state that needs to be tried.
  MemberFunction state_;

  //! If a sub flow is running, this state will be transitioned to when the
  //! subflow terminates. This is used by the sleep subflow.
  MemberFunction next_state_;

  //! Executor controlling this control flow. Also hosts the lock for the
  //! control flow.
  Executor* executor_;
};




#endif // _EXECUTOR_CONTROL_FLOW_HXX_
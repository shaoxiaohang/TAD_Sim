#pragma once
#include "CoreMinimal.h"

#include "txsim_module_service.h"
#include "DisplayGameInstance.h"

class DISPLAY_API NetworkModule : public tx_sim::SimModule {
 public:
  NetworkModule();
  ~NetworkModule();

  virtual void Init(tx_sim::InitHelper& helper);
  virtual void Reset(tx_sim::ResetHelper& helper);
  virtual void Step(tx_sim::StepHelper& helper);
  virtual void Stop(tx_sim::StopHelper& helper);

  FCriticalSection mutex_Input;
  FCriticalSection mutex_Output;
  FEvent* getThreadSuspendedEvent();

  class UDisplayGameInstance* myGameInstance = nullptr;

 private:
  FEvent* threadSuspendedEvent = nullptr;
  FSimUpdateIn simUpdateIn;

  bool asynchronousMode = false;

  double time0 = 0;
  double realstep = 0;

  FString TrafficTopic = TEXT("TRAFFIC");
  FString LocationTopic = TEXT("LOCATION");
  FString UnionPrefixStr = TEXT("EgoUnion/");
};
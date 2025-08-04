// EventAction.cc (final corrected version with ETA cap)

#include "EventAction.hh"
#include "RunAction.hh"
#include "G4RunManager.hh"
#include "G4Event.hh"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <ctime>
#include <chrono>
#include <deque>
#include <cstring> // Needed for memcpy


//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// Constructor: Initializes the EventAction with a pointer to the RunAction
EventAction::EventAction(RunAction* runAction) 
: fRunAction(runAction),fGlobalEventCounter(runAction->GetEventsProcessedCounter())
 { }

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
void EventAction::BeginOfEventAction(const G4Event* anEvent)
{
  nSecondaries = 0;
  
  // Use the atomic counter to get the true, global event ID
//    G4int eventID = anEvent->GetEventID();

  G4int eventID = fGlobalEventCounter.fetch_add(1, std::memory_order_relaxed) + 1;
      //std::cout << "=> Event " << eventID  << std::endl;

  G4int nOfEvents = G4RunManager::GetRunManager()->GetNumberOfEventsToBeProcessed();
  G4double dispPercentage = 1.;

  auto currentTimePoint = std::chrono::high_resolution_clock::now();
  fEventTimestamps.push_back(currentTimePoint);

  if (fEventTimestamps.size() > ETA_WINDOW) {
      fEventTimestamps.pop_front();
  }

  if (nOfEvents > 0 && fmod(eventID, double(nOfEvents * dispPercentage * 0.01)) == 0)
  {
    G4double status = (100.0 * eventID) / nOfEvents; 

    double etaSeconds = 0.0;
    
    if (fEventTimestamps.size() == ETA_WINDOW) 
    {
      auto time_span_duration = currentTimePoint - fEventTimestamps.front();
      double time_span = std::chrono::duration_cast<std::chrono::duration<double>>(time_span_duration).count();
      
      if (time_span > 0.0) 
      {
          double eventsPerSecond = (double)ETA_WINDOW / time_span;
          double remainingEvents = nOfEvents - eventID;
          etaSeconds = remainingEvents / eventsPerSecond;
      }
    } 
    else 
    {
        auto elapsedTime_duration = currentTimePoint - fRunAction->fStartTime;
        double elapsedTime = std::chrono::duration_cast<std::chrono::duration<double>>(elapsedTime_duration).count();

        if(elapsedTime > 0.0) {
            double eventsPerSecond = (double)eventID / elapsedTime;
            double remainingEvents = nOfEvents - eventID;
            etaSeconds = remainingEvents / eventsPerSecond;
        }
    }
    
    time_t currentTime_t = std::chrono::high_resolution_clock::to_time_t(currentTimePoint);
    time_t runStartTime_t = std::chrono::high_resolution_clock::to_time_t(fRunAction->fStartTime);

    tm localRunStartTime;
    memcpy(&localRunStartTime, localtime(&runStartTime_t), sizeof(tm));

    
    tm localStartTime;
    memcpy(&localStartTime, localtime(&currentTime_t), sizeof(tm));
    
    const long maxEtaSeconds = 86400L * 365L;
    if (etaSeconds > maxEtaSeconds) {
        etaSeconds = maxEtaSeconds;
    }

    time_t estimatedCompletionTime_t = currentTime_t + (long)etaSeconds;
    
    tm localEndTime;
    tm* tempEndTime = localtime(&estimatedCompletionTime_t);
    if (tempEndTime) {
        memcpy(&localEndTime, tempEndTime, sizeof(tm));
    } else {
        memset(&localEndTime, 0, sizeof(tm));
    }
    
    char runStartTimeStr[30];
    char startTimeStr[30];
    char endTimeStr[30];
    
    strftime(startTimeStr, sizeof(startTimeStr), "%Y-%m-%d %H:%M:%S", &localStartTime);
    strftime(runStartTimeStr, sizeof(runStartTimeStr), "%Y-%m-%d %H:%M:%S", &localRunStartTime);
    
    if (tempEndTime) {
        strftime(endTimeStr, sizeof(endTimeStr), "%Y-%m-%d %H:%M:%S", &localEndTime);
    } else {
        strcpy(endTimeStr, "Date Overflow");
    }

    int d = (int)(etaSeconds / 86400);
    int h = (int)(fmod(etaSeconds, 86400) / 3600);
    int m = (int)(fmod(etaSeconds, 3600) / 60);
    int s = (int)(fmod(etaSeconds, 60));

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "=> Event " << eventID << " started at " << startTimeStr << " runStart: " << runStartTimeStr<< std::endl;
    std::cout << "   Progress: " << status << "% complete. " << "Remaining: " << d << "d " << h << "h " << m << "m " << s << "s" << std::endl;
    std::cout << "   ETA: " << endTimeStr << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void EventAction::EndOfEventAction(const G4Event*)
{
  //...
}

void EventAction::IncrementSecondaryParticleCounter()
{
  nSecondaries++;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
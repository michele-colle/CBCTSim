
#ifndef B1EventAction_h
#define B1EventAction_h 1

#include "G4UserEventAction.hh"
#include "globals.hh"

#include <deque> // Include for std::deque
#include <ctime>   // Include for time_t
#include <chrono> // Include for std::chrono
#include <atomic> // Include for std::atomic

class G4Event;

class RunAction;

/// Event action class

class EventAction : public G4UserEventAction
{
  public:
    EventAction(RunAction* runAction);
    ~EventAction() override = default;

    void BeginOfEventAction(const G4Event* event) override;
    void EndOfEventAction(const G4Event* event) override;
    void IncrementSecondaryParticleCounter();


  private:
    RunAction* fRunAction = nullptr;
    G4int nSecondaries;

    /// @brief counter globale per calcolo ETA (estimated time arrival)
    std::atomic<long>& fGlobalEventCounter;
     // New member variables for rolling average ETA
    static const int ETA_WINDOW = 500; // Window size for rolling average
    std::deque<std::chrono::high_resolution_clock::time_point> fEventTimestamps;
};


//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif
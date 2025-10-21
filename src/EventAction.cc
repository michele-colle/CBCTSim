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
#include <G4SystemOfUnits.hh>
#include <G4AnalysisManager.hh>
#include <G4SDManager.hh>
#include <MyHit.hh>
#include <G4EventManager.hh>
#include <CBCTParams.hh>
#include <TxtWithHeaderReader.hh>

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// Constructor: Initializes the EventAction with a pointer to the RunAction
EventAction::EventAction(RunAction *runAction)
    : fRunAction(runAction), fGlobalEventCounter(runAction->GetEventsProcessedCounter())
{
  TxtWithHeaderReader reader;
  CBCTParams *params = CBCTParams::Instance();
  if (reader.loadFromFile(params->GetDetectorMaterial() + ".txt"))
  {
    scintillatorDetectorEfficiency = new G4PhysicsOrderedFreeVector();
    auto enflt = reader.getColumn("keV");
    auto att = reader.getColumn("att");
    for (size_t i = 0; i < enflt.size(); ++i)
    {
      scintillatorDetectorEfficiency->InsertValues(enflt[i] * keV, 1 - exp(-params->GetDetectorThickness() * att[i] / cm));
    }
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
void EventAction::BeginOfEventAction(const G4Event *anEvent)
{
  nSecondaries = 0;

  // Use the atomic counter to get the true, global event ID
  //    G4int eventID = anEvent->GetEventID();

  G4int eventID = fGlobalEventCounter.fetch_add(1, std::memory_order_relaxed) + 1;
  // std::cout << "=> Event " << eventID  << std::endl;

  G4int nOfEvents = G4RunManager::GetRunManager()->GetNumberOfEventsToBeProcessed();
  G4double dispPercentage = 1.;

  auto currentTimePoint = std::chrono::high_resolution_clock::now();
  fEventTimestamps.push_back(currentTimePoint);

  if (fEventTimestamps.size() > ETA_WINDOW)
  {
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

      if (elapsedTime > 0.0)
      {
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
    if (etaSeconds > maxEtaSeconds)
    {
      etaSeconds = maxEtaSeconds;
    }

    time_t estimatedCompletionTime_t = currentTime_t + (long)etaSeconds;

    tm localEndTime;
    tm *tempEndTime = localtime(&estimatedCompletionTime_t);
    if (tempEndTime)
    {
      memcpy(&localEndTime, tempEndTime, sizeof(tm));
    }
    else
    {
      memset(&localEndTime, 0, sizeof(tm));
    }

    char runStartTimeStr[30];
    char startTimeStr[30];
    char endTimeStr[30];

    strftime(startTimeStr, sizeof(startTimeStr), "%Y-%m-%d %H:%M:%S", &localStartTime);
    strftime(runStartTimeStr, sizeof(runStartTimeStr), "%Y-%m-%d %H:%M:%S", &localRunStartTime);

    if (tempEndTime)
    {
      strftime(endTimeStr, sizeof(endTimeStr), "%Y-%m-%d %H:%M:%S", &localEndTime);
    }
    else
    {
      strcpy(endTimeStr, "Date Overflow");
    }

    int d = (int)(etaSeconds / 86400);
    int h = (int)(fmod(etaSeconds, 86400) / 3600);
    int m = (int)(fmod(etaSeconds, 3600) / 60);
    int s = (int)(fmod(etaSeconds, 60));

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "=> Event " << eventID << " started at " << startTimeStr << " runStart: " << runStartTimeStr << std::endl;
    std::cout << "   Progress: " << status << "% complete. " << "Remaining: " << d << "d " << h << "h " << m << "m " << s << "s" << std::endl;
    std::cout << "   ETA: " << endTimeStr << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void EventAction::EndOfEventAction(const G4Event *anEvent)
{
  //G4cout<<"EndOfEventAction"<<G4endl;
  // Get the unique ID for our hits collection
  G4int hcID = G4SDManager::GetSDMpointer()->GetCollectionID("MyHitsCollection");

  // Get the Hits Collection of This Event
  G4HCofThisEvent *hce = anEvent->GetHCofThisEvent();
  if (!hce)
    return;
  // Get our specific collection from the HCE
  MyHitsCollection *hitsCollection = static_cast<MyHitsCollection *>(hce->GetHC(hcID));

  if (!hitsCollection)
    return;

  // Now, loop through the collection and fill histograms
  G4AnalysisManager *analysis = G4AnalysisManager::Instance();
  auto par = CBCTParams::Instance();
  auto sourcePos = G4ThreeVector(0, -par->GetDSO(), 0);

  for (const auto &hit : *hitsCollection->GetVector())
  {
    // Get the data from the hit object
    //G4double primaryEnergy = hit->GetPrimaryEnergy();
    //G4ThreeVector primaryMomentum = hit->GetPrimaryMomentum();
    G4double en = hit->GetEnergy();
    G4ThreeVector posPhoton = hit->GetPosition();
    G4ThreeVector momPhotonDirection = hit->GetMomentum().unit();
    if (scintillatorDetectorEfficiency && G4UniformRand() > scintillatorDetectorEfficiency->Value(en))
    {
      // //G4cout<<"photon not detected "<<en<<G4endl;
      continue; // non viene visto
    }

    G4ThreeVector p = (posPhoton - sourcePos).unit();
    G4double dot = momPhotonDirection.dot(p);
    //G4double E = primaryEnergy;
    const bool isCollinear = (dot >= 1.0 - DBL_EPSILON);
    //G4double deltaEnergy = E - en;
    // const bool isCollinear = (diffSquared < DBL_EPSILON);
    // G4cout<<"mom angle diff "<<diffSquared<<G4endl;
    // G4cout<<"parent id "<<track->GetParentID()<<G4endl;
    // G4cout<<"primary id "<<info->primaryID<<G4endl;
    // G4cout<<"actual id "<<track->GetTrackID() <<G4endl;
    // G4cout<<"primary momdir "<<p/p.mag()<<G4endl;
    // G4cout<<"actual momdir "<<momPhoton/momPhoton.mag()<<G4endl;

    // primary
    if (isCollinear)
    {
      analysis->FillH1(1, en);
      analysis->FillH2(1, posPhoton.x(), posPhoton.z(), en / keV);
    }
    else // scatter
    {
      analysis->FillH1(2, en);
      analysis->FillH2(2, posPhoton.x(), posPhoton.z(), en / keV);
    }
    // G4cout<<"actual pos "<<posPhoton<<G4endl;
  }

  //G4cout<<"Received Particles: "<<hitsCollection->GetSize()<<G4endl;

// const G4VProcess *process = aStep->GetPostStepPoint()->GetProcessDefinedStep();
//       G4String processName = " UserLimit";
//       if (process)
//       {
//         processName = process->GetProcessName();
//         if(processName!="Transportation"){
//           G4cout << "*Gamma - Process:  " << processName  <<G4endl;
//           const bool isCollinear = (diffSquared < DBL_EPSILON);
//           G4cout<<"mom angle diff "<<diffSquared<<G4endl;
//           G4cout<<"parent id "<<track->GetParentID()<<G4endl;
//           G4cout<<"primary id "<<info->primaryID<<G4endl;
//           G4cout<<"actual id "<<track->GetTrackID() <<G4endl;
//           G4cout<<"primary momdir "<<p/p.mag()<<G4endl;
//           G4cout<<"actual momdir "<<momPhoton/momPhoton.mag()<<G4endl<<G4endl<<G4endl;
//         }
//       }
}



void EventAction::IncrementSecondaryParticleCounter()
{
  nSecondaries++;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

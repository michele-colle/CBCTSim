#ifndef MyHit_h
#define MyHit_h 1

#include "G4VHit.hh"
#include "G4THitsCollection.hh"
#include "G4ThreeVector.hh"
#include "G4Allocator.hh" // Required for efficient memory management

// This is our custom Hit class. It inherits from the Geant4 base class.
class MyHit : public G4VHit
{
public:
  // Default constructor and destructor
  MyHit() = default;
  virtual ~MyHit() = default;

  // We need to implement these two methods for G4Allocator
  inline void* operator new(size_t);
  inline void  operator delete(void*);

  // You can add your "setter" and "getter" methods here for clean access
  // void SetPrimaryEnergy(G4double val)    { fPrimaryEnergy = val; }
  // G4double GetPrimaryEnergy() const      { return fPrimaryEnergy; }

  // void SetPrimaryMomentum(G4ThreeVector val) { fPrimaryMomentum = val; }
  // const G4ThreeVector& GetPrimaryMomentum() const { return fPrimaryMomentum; }

  void SetEnergy(G4double val)           { fEnergy = val; }
  G4double GetEnergy() const             { return fEnergy; }
  
  void SetPosition(G4ThreeVector val)    { fPosition = val; }
  const G4ThreeVector& GetPosition() const { return fPosition; }
  
  void SetMomentum(G4ThreeVector val)    { fMomentum = val; }
  const G4ThreeVector& GetMomentum() const { return fMomentum; }

private:
  // The data members that will store our information
  //G4double      fPrimaryEnergy;
  //G4ThreeVector fPrimaryMomentum;
  G4double      fEnergy;
  G4ThreeVector fPosition;
  G4ThreeVector fMomentum;
};

// This is a special Geant4 template that creates a "vector" of MyHit objects.
using MyHitsCollection = G4THitsCollection<MyHit>;

// These two lines are required by Geant4 for efficient memory allocation of hits.
// It uses a memory pool to avoid calling new/delete millions of times.
extern G4ThreadLocal G4Allocator<MyHit>* MyHitAllocator;

inline void* MyHit::operator new(size_t)
{
  if (!MyHitAllocator) MyHitAllocator = new G4Allocator<MyHit>;
  return (void*)MyHitAllocator->MallocSingle();
}

inline void MyHit::operator delete(void* hit)
{
  MyHitAllocator->FreeSingle((MyHit*) hit);
}

#endif
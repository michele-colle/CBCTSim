#include "MyHit.hh"

// Define the thread-local allocator for MyHit
G4ThreadLocal G4Allocator<MyHit>* MyHitAllocator = nullptr;
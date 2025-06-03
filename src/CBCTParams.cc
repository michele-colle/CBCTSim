// CBCTParams.cc
#include "CBCTParams.hh"

CBCTParams* CBCTParams::fInstance = nullptr;

CBCTParams* CBCTParams::Instance() {
    if (!fInstance) {
        fInstance = new CBCTParams();
    }
    return fInstance;
}

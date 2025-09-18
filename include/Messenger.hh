// Messenger.hh
#include "G4UImessenger.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include <G4UIcmdWithAString.hh>
#include "CBCTParams.hh"


class Messenger : public G4UImessenger {
public:
    Messenger();
    ~Messenger();

    void SetNewValue(G4UIcommand* command, G4String newValue) override;

private:
    G4UIdirectory* fDir;
    G4UIcmdWithADoubleAndUnit* fSetDSDCmd;
    G4UIcmdWithADoubleAndUnit* fSetDSOCmd;
    G4UIcmdWithADoubleAndUnit* fSetDetWidthCmd;
    G4UIcmdWithADoubleAndUnit* fSetDetHeightCmd;
    G4UIcmdWithAString* fSetFilter1MatCmd;
    G4UIcmdWithAString* fSetFilter2MatCmd;
    G4UIcmdWithADoubleAndUnit* fSetFilter1ThickCmd;
    G4UIcmdWithADoubleAndUnit* fSetFilter2ThickCmd;
    G4UIcmdWithAString* fSetDetMatCmd;
    G4UIcmdWithADoubleAndUnit* fSetDetThickCmd;
    G4UIcmdWithADoubleAndUnit* fSetObjectAngleCmd;
    G4UIcmdWithAString* fSetXRaySourceSpectrumCmd; 
    G4UIcmdWithAString* fCreateMaterialSpecturm; 
    G4UIcommand* fDrawFov;
    G4UIcmdWithADoubleAndUnit* fSetPatienStandPositionCmd;
    G4UIcmdWithAString* fSetPhantomCmd;
};

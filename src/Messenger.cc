#include "Messenger.hh"
#include "CBCTParams.hh"

#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIdirectory.hh"
#include "G4RunManager.hh"
#include "G4Material.hh"

Messenger::Messenger() {
    // Create command directory
    fDir = new G4UIdirectory("/mysim/");
    fDir->SetGuidance("Commands for simulation parameters");

    // Command to set source-detector distance
    fSetDSDCmd = new G4UIcmdWithADoubleAndUnit("/mysim/setDSD", this);
    fSetDSDCmd->SetGuidance("Set distance from source to detector.");
    fSetDSDCmd->SetParameterName("dsd", false);
    fSetDSDCmd->SetUnitCategory("Length");
    fSetDSDCmd->SetDefaultUnit("mm");

    // Command to set gun position
    fSetDSOCmd = new G4UIcmdWithADoubleAndUnit("/mysim/setDSO", this);
    fSetDSOCmd->SetGuidance("Set distance from source to rotation center.");
    fSetDSOCmd->SetParameterName("dso", false);
    fSetDSOCmd->SetUnitCategory("Length");
    fSetDSOCmd->SetDefaultUnit("mm");

    // Detector width
    fSetDetWidthCmd = new G4UIcmdWithADoubleAndUnit("/mysim/setDetWidth", this);
    fSetDetWidthCmd->SetGuidance("Set detector width.");
    fSetDetWidthCmd->SetParameterName("width", false);
    fSetDetWidthCmd->SetUnitCategory("Length");
    fSetDetWidthCmd->SetDefaultUnit("mm");

    // Detector height
    fSetDetHeightCmd = new G4UIcmdWithADoubleAndUnit("/mysim/setDetHeight", this);
    fSetDetHeightCmd->SetGuidance("Set detector height.");
    fSetDetHeightCmd->SetParameterName("height", false);
    fSetDetHeightCmd->SetUnitCategory("Length");
    fSetDetHeightCmd->SetDefaultUnit("mm");

    // Filter 1 material
    fSetFilter1MatCmd = new G4UIcmdWithAString("/mysim/setFilter1Material", this);
    fSetFilter1MatCmd->SetGuidance("Set filter 1 material by name.");

    // Filter 2 material
    fSetFilter2MatCmd = new G4UIcmdWithAString("/mysim/setFilter2Material", this);
    fSetFilter2MatCmd->SetGuidance("Set filter 2 material by name.");

    // Filter 1 thickness
    fSetFilter1ThickCmd = new G4UIcmdWithADoubleAndUnit("/mysim/setFilter1Thickness", this);
    fSetFilter1ThickCmd->SetGuidance("Set filter 1 thickness.");
    fSetFilter1ThickCmd->SetParameterName("thickness", false);
    fSetFilter1ThickCmd->SetUnitCategory("Length");
    fSetFilter1ThickCmd->SetDefaultUnit("mm");

    // Filter 2 thickness
    fSetFilter2ThickCmd = new G4UIcmdWithADoubleAndUnit("/mysim/setFilter2Thickness", this);
    fSetFilter2ThickCmd->SetGuidance("Set filter 2 thickness.");
    fSetFilter2ThickCmd->SetParameterName("thickness", false);
    fSetFilter2ThickCmd->SetUnitCategory("Length");
    fSetFilter2ThickCmd->SetDefaultUnit("mm");

    // Detector material
    fSetDetMatCmd = new G4UIcmdWithAString("/mysim/setDetectorMaterial", this);
    fSetDetMatCmd->SetGuidance("Set detector material by name.");

    // Detector thickness
    fSetDetThickCmd = new G4UIcmdWithADoubleAndUnit("/mysim/setDetectorThickness", this);
    fSetDetThickCmd->SetGuidance("Set detector thickness.");
    fSetDetThickCmd->SetParameterName("thickness", false);
    fSetDetThickCmd->SetUnitCategory("Length");
    fSetDetThickCmd->SetDefaultUnit("mm");

    // Object angle
    fSetObjectAngleCmd = new G4UIcmdWithADoubleAndUnit("/mysim/setObjectAngle", this);
    fSetObjectAngleCmd->SetGuidance("Set object angle in degree.");
    fSetObjectAngleCmd->SetParameterName("angle", false);
    fSetObjectAngleCmd->SetUnitCategory("Angle");
    fSetObjectAngleCmd->SetDefaultUnit("deg");
}

Messenger::~Messenger() {
    delete fSetDSDCmd;
    delete fSetDSOCmd;
    delete fSetDetWidthCmd;
    delete fSetDetHeightCmd;
    delete fSetFilter1MatCmd;
    delete fSetFilter2MatCmd;
    delete fSetFilter1ThickCmd;
    delete fSetFilter2ThickCmd;
    delete fSetDetMatCmd;
    delete fSetDetThickCmd;
    delete fSetObjectAngleCmd;
    delete fDir;
}

void Messenger::SetNewValue(G4UIcommand* command, G4String newValue) {
    auto* params = CBCTParams::Instance();

    if (command == fSetDSDCmd) {
        G4double dsd = fSetDSDCmd->GetNewDoubleValue(newValue);
        params->SetDSD(dsd);
        G4RunManager::GetRunManager()->GeometryHasBeenModified();

    } else if (command == fSetDSOCmd) {
        G4double dso = fSetDSOCmd->GetNewDoubleValue(newValue);
        params->SetDSO(dso);
        G4RunManager::GetRunManager()->GeometryHasBeenModified();

    } else if (command == fSetDetWidthCmd) {
        params->SetDetWidth(fSetDetWidthCmd->GetNewDoubleValue(newValue));
        G4RunManager::GetRunManager()->GeometryHasBeenModified();

    } else if (command == fSetDetHeightCmd) {
        params->SetDetHeight(fSetDetHeightCmd->GetNewDoubleValue(newValue));
        G4RunManager::GetRunManager()->GeometryHasBeenModified();

    } else if (command == fSetFilter1MatCmd) {
        G4Material* mat = G4Material::GetMaterial(newValue, false);
        if (mat) params->SetFilter1Material(mat);

    } else if (command == fSetFilter2MatCmd) {
        G4Material* mat = G4Material::GetMaterial(newValue, false);
        if (mat) params->SetFilter2Material(mat);

    } else if (command == fSetFilter1ThickCmd) {
        params->SetFilter1Thickness(fSetFilter1ThickCmd->GetNewDoubleValue(newValue));
        G4RunManager::GetRunManager()->GeometryHasBeenModified();

    } else if (command == fSetFilter2ThickCmd) {
        params->SetFilter2Thickness(fSetFilter2ThickCmd->GetNewDoubleValue(newValue));
        G4RunManager::GetRunManager()->GeometryHasBeenModified();

    } else if (command == fSetDetMatCmd) {
        G4Material* mat = G4Material::GetMaterial(newValue, false);
        if (mat) params->SetDetectorMaterial(mat);

    } else if (command == fSetDetThickCmd) {
        params->SetDetectorThickness(fSetDetThickCmd->GetNewDoubleValue(newValue));
        G4RunManager::GetRunManager()->GeometryHasBeenModified();

    } else if (command == fSetObjectAngleCmd) {
        params->SetObjectAngleInDegree(fSetObjectAngleCmd->GetNewDoubleValue(newValue));
    }
}
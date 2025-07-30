#include "Messenger.hh"
#include "CBCTParams.hh"

#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIdirectory.hh"
#include "G4RunManager.hh"
#include "G4Material.hh"
#include "G4Polyline.hh"
#include <G4VVisManager.hh>
#include <G4Colour.hh>
#include <G4VisAttributes.hh>

Messenger::Messenger()
{
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

    // XRaySourceSpectrum string
    fSetXRaySourceSpectrumCmd = new G4UIcmdWithAString("/mysim/setXRaySourceSpectrum", this);
    fSetXRaySourceSpectrumCmd->SetGuidance("Set the X-ray source spectrum string.");
    fSetXRaySourceSpectrumCmd->SetCandidates(
        "030K10DW 040K10DW 050K10DW 060K10DW 070K10DW 080K10DW 090K10DW 100K10DW 110K10DW 120K10DW 130K10DW 140K10DW 150K10DW 40kV-ImaS 110K15D0W2 060K15D0W2 120K15D0W2");

    fCreateMaterialSpecturm = new G4UIcmdWithAString("/mysim/createMaterialSpectrum", this);
    fCreateMaterialSpecturm->SetGuidance("Create mu spectrum from nist name, example: 'G4_WATER'.");

    //draw FOV command
    fDrawFov = new G4UIcommand("/mysim/drawFOV", this);
    fDrawFov->SetGuidance("Draw the field of view (FOV) of the detector.");


    // posizione barella
    fSetPatienStandPositionCmd = new G4UIcmdWithADoubleAndUnit("/mysim/setPatienStandPosition", this);
    fSetPatienStandPositionCmd->SetGuidance("Set object angle in degree.");
    fSetPatienStandPositionCmd->SetParameterName("position", false);
    fSetPatienStandPositionCmd->SetDefaultUnit("cm");
}

Messenger::~Messenger()
{
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
    delete fDrawFov;
    delete fSetPatienStandPositionCmd;

}

void Messenger::SetNewValue(G4UIcommand *command, G4String newValue)
{
    auto *params = CBCTParams::Instance();

    if (command == fSetDSDCmd)
    {
        G4double dsd = fSetDSDCmd->GetNewDoubleValue(newValue);
        params->SetDSD(dsd);
        G4RunManager::GetRunManager()->GeometryHasBeenModified();
    }
    else if (command == fSetDSOCmd)
    {
        G4double dso = fSetDSOCmd->GetNewDoubleValue(newValue);
        params->SetDSO(dso);
        G4RunManager::GetRunManager()->GeometryHasBeenModified();
    }
    else if (command == fSetDetWidthCmd)
    {
        params->SetDetWidth(fSetDetWidthCmd->GetNewDoubleValue(newValue));
        G4RunManager::GetRunManager()->GeometryHasBeenModified();
    }
    else if (command == fSetDetHeightCmd)
    {
        params->SetDetHeight(fSetDetHeightCmd->GetNewDoubleValue(newValue));
        G4RunManager::GetRunManager()->GeometryHasBeenModified();
    }
    else if (command == fSetFilter1MatCmd)
    {
        params->SetFilter1Material(newValue);
    }
    else if (command == fSetFilter2MatCmd)
    {
        params->SetFilter2Material(newValue);
    }
    else if (command == fSetFilter1ThickCmd)
    {
        params->SetFilter1Thickness(fSetFilter1ThickCmd->GetNewDoubleValue(newValue));
        G4RunManager::GetRunManager()->GeometryHasBeenModified();
    }
    else if (command == fSetFilter2ThickCmd)
    {
        params->SetFilter2Thickness(fSetFilter2ThickCmd->GetNewDoubleValue(newValue));
        G4RunManager::GetRunManager()->GeometryHasBeenModified();
    }
    else if (command == fSetDetMatCmd)
    {
        params->SetDetectorMaterial(newValue);
    }
    else if (command == fSetDetThickCmd)
    {
        params->SetDetectorThickness(fSetDetThickCmd->GetNewDoubleValue(newValue));
        G4RunManager::GetRunManager()->GeometryHasBeenModified();
    }
    else if (command == fSetObjectAngleCmd)
    {
        params->SetObjectAngleInDegree(fSetObjectAngleCmd->GetNewDoubleValue(newValue));
    }
    else if (command == fSetXRaySourceSpectrumCmd) {
        static const std::set<G4String> allowed = {
            "030K10DW", "040K10DW", "050K10DW", "060K10DW", "070K10DW", "080K10DW", "090K10DW",
            "100K10DW", "110K10DW", "120K10DW", "130K10DW", "140K10DW", "150K10DW",
            "40kV-ImaS", "110K15D0W2", "060K15D0W2", "120K15D0W2"
        };
        if (allowed.count(newValue)) {
            params->SetXRaySourceSpectrum(newValue);
        } else {
            G4cerr << "Invalid XRaySourceSpectrum: " << newValue << G4endl;
            G4cerr << "Allowed values are: ";
            for (const auto& v : allowed) G4cerr << v << " ";
            G4cerr << G4endl;
        }
    }
    else if (command == fCreateMaterialSpecturm) {
        params->CreateMaterialSpectrum(newValue);
    }
    else if (command == fSetPatienStandPositionCmd) {
        params->SetPatienStandPosition(fSetPatienStandPositionCmd->GetNewDoubleValue(newValue));
        G4RunManager::GetRunManager()->GeometryHasBeenModified();
    }
    else if (command == fDrawFov) {
        //disengo le righe del fov
        G4VVisManager *vis = G4VVisManager::GetConcreteInstance();
        auto par = CBCTParams::Instance();
        auto dsd = par->GetDSD();
        auto dso = par->GetDSO();
        auto ddo = dsd-dso;
        auto halfH = par->GetDetHeight() / 2.0;
        auto halfW = par->GetDetWidth() / 2.0;

        G4Polyline line;
        // define endpoints (in world coordinates)
        line.push_back(G4Point3D(0, -dso, 0));
        line.push_back(G4Point3D(halfH, ddo, halfW));

        G4Colour color(1.0, 0.0, 0.0); // red
        line.SetVisAttributes(G4VisAttributes(color));

        vis->Draw(line);

        G4Polyline line2;
        // define endpoints (in world coordinates)
        line2.push_back(G4Point3D(0, -dso, 0));
        line2.push_back(G4Point3D(-halfH, ddo, halfW));

        line2.SetVisAttributes(G4VisAttributes(color));

        vis->Draw(line2);

        G4Polyline line3;
        // define endpoints (in world coordinates)
        line3.push_back(G4Point3D(0, -dso, 0));
        line3.push_back(G4Point3D(halfH, ddo, -halfW));

        line3.SetVisAttributes(G4VisAttributes(color));

        vis->Draw(line3);

        G4Polyline line4;
        // define endpoints (in world coordinates)
        line4.push_back(G4Point3D(0, -dso, 0));
        line4.push_back(G4Point3D(-halfH, ddo, -halfW));

        line4.SetVisAttributes(G4VisAttributes(color));

        vis->Draw(line4);
      
    }
}
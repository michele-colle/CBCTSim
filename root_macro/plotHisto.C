#include "TCanvas.h"
#include "TROOT.h"
#include "TGraphErrors.h"
#include "TF1.h"
#include "TLegend.h"
#include "TArrow.h"
#include "TLatex.h"
#include <TH1D.h>
#include <TFile.h>
#include <TApplication.h>

// i/o example

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
using namespace std;

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

std::vector<std::vector<double>> readColumnWiseData(const std::string& filename) {
    std::ifstream infile(filename);
    std::string line;
    std::vector<std::vector<double>> data;
    bool firstLine = true;

    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        std::vector<double> rowValues;
        double value;

        // Read all values from the line
        while (iss >> value) {
            rowValues.push_back(value);
        }

        // Initialize the column-wise storage on the first line
        if (firstLine) {
            data.resize(rowValues.size());
            firstLine = false;
        }

        // Append each value to its corresponding column vector
        for (size_t i = 0; i < rowValues.size(); ++i) {
            data[i].push_back(rowValues[i]);
        }
    }

    return data;
}
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

template<typename T>
bool writeVectorsToFile(const std::vector<std::vector<T>>& vectors, const std::string& filename) {
    if (vectors.empty()) {
        std::cerr << "No vectors provided.\n";
        return false;
    }

    // Check that all vectors are of the same size
    size_t size = vectors[0].size();
    for (const auto& vec : vectors) {
        if (vec.size() != size) {
            std::cerr << "All vectors must be of the same size.\n";
            return false;
        }
    }

    std::ofstream outFile(filename);
    if (!outFile) {
        std::cerr << "Could not open file " << filename << " for writing.\n";
        return false;
    }

    for (size_t i = 0; i < size; ++i) {
        for (size_t j = 0; j < vectors.size(); ++j) {
            outFile << vectors[j][i];
            if (j < vectors.size() - 1)
                outFile << " "; // change delimiter here if needed
        }
        outFile << "\n";
    }

    outFile.close();
    return true;
}


void plotHisto()
{
  std::string filename = "nist_w.txt";
  std::vector<std::vector<double>> data = readColumnWiseData(filename);

  // Print to verify
  for (size_t row = 0; row < data[0].size(); ++row) 
  {
    for (size_t col = 0; col < data.size(); ++col) {
      std::cout << data[col][row] << " ";
    }
    std::cout << endl;
  }
  for (double &val : data[0]) {
  val *= 1e3; //MeV->keV
  }
  // Draw histos filled by Geant4 simulation 
  //   

  cout<<"alalaa adasdadasd"<<endl;
  TFile* f (TFile::Open("run0.root"));  
  f->ls();

  TCanvas* c1 = new TCanvas("c1", "  ");
  cout<<"c1: "<<c1<<endl;
  c1->SetLogx();
  c1->SetLogy();
  c1->SetGrid();
  TH1D* air = (TH1D*)(f -> Get("0"));
  TH1D* primary = (TH1D*)(f -> Get("1"));
  TH1D* scCollinear = (TH1D*)(f -> Get("2"));
  TH1D* scNonCollinear = (TH1D*)(f -> Get("3"));
  //hist1->Draw("Incident energy");

  double bin = primary->GetNbinsX();
  double thickness = 2.0*10.0*1e-4;//um to cm : 10^-4
  double rho = 19.254;

  vector<double> x, y;
  for (size_t i = 1; i < bin-1; i++)
  {
    double tair = air->GetAt(i);
    //double tair = 10000000.0/1024.0;
    double tval = primary->GetAt(i);
    double en = primary->GetXaxis()->GetBinCenter(i);
    double tx= en;//keV
    double ty = log(tair/tval)/thickness/rho;

    if(std::isnan(ty) || std::isinf(ty))
    {
      cout<<i<<" adfasdfasdfasdfadfasdfasdfasdfasdfasdf "<<ty<<endl;
      continue;
    }
    x.push_back(tx);
    y.push_back(ty);
  }
  
  std::vector<std::vector<double>> towrite = {x,y};
  writeVectorsToFile(towrite, "output.txt");

 
  auto gr = new TGraph (x.size(), x.data(), y.data());
  gr->Draw("AL");
  c1->Update();

  auto gr_nist = new TGraph((double)data[0].size(), data[0].data(),data[1].data());
  gr_nist->SetLineColor(kRed);
  gr_nist -> Draw("L SAME");

  gr_nist->SetTitle("Attenuation;keV;mu/rho");
  gr_nist->GetXaxis()->SetNdivisions(910);  // major = 5, minor = 10
  c1->Update();


  TLegend *legend = new TLegend(0.7, 0.7, 0.9, 0.85); // x1, y1, x2, y2 in NDC (0–1)
  legend->AddEntry(gr_nist, "nist", "l");     // "l" = line style
  legend->AddEntry(gr, "geant", "l");     // "l" = line style
  legend->Draw();
  c1->Update();

  TCanvas *c = new TCanvas("c", "Geant");
  c->SetLogx();
  c->SetLogy();
  c->SetGrid();
  c->cd();
  gr->Draw("AL");
  gr->SetTitle("Attenuation;keV;mu/rho");
  gr->GetXaxis()->SetNdivisions(990);  // major = 5, minor = 10
  gr->GetXaxis()->SetMoreLogLabels();
  c1->Update();

}  

void StandaloneApplication(int argc, char** argv) {
  // eventually, evaluate the application parameters argc, argv
  // ==>> here the ROOT macro is called
  plotHisto();
}
  // This is the standard "main" of C++ starting
  // a ROOT application
int main(int argc, char** argv) {
   TApplication app("ROOT Application", &argc, argv);
   StandaloneApplication(app.Argc(), app.Argv());
   app.Run();
   return 0;
}
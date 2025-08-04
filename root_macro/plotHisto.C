#include "TCanvas.h"
#include "TROOT.h"
#include "TGraphErrors.h"
#include "TF1.h"
#include "TLegend.h"
#include "TArrow.h"
#include "TLatex.h"
#include <TH1D.h>
#include <TH2D.h>
#include <TFile.h>
#include <TApplication.h>
#include <TNtuple.h>
#include "TStyle.h"

// i/o example
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

using namespace std;

std::vector<std::vector<double>> readColumnWiseData(const std::string &filename)
{
  std::ifstream infile(filename);
  std::string line;
  std::vector<std::vector<double>> data;
  bool firstLine = true;

  while (std::getline(infile, line))
  {
    std::istringstream iss(line);
    std::vector<double> rowValues;
    double value;

    // Read all values from the line
    while (iss >> value)
    {
      rowValues.push_back(value);
    }

    // Initialize the column-wise storage on the first line
    if (firstLine)
    {
      data.resize(rowValues.size());
      firstLine = false;
    }

    // Append each value to its corresponding column vector
    for (size_t i = 0; i < rowValues.size(); ++i)
    {
      data[i].push_back(rowValues[i]);
    }
  }

  return data;
}

template <typename T>
bool writeVectorsToFile(const std::vector<std::vector<T>> &vectors, const std::string &filename)
{
  if (vectors.empty())
  {
    std::cerr << "No vectors provided.\n";
    return false;
  }

  // Check that all vectors are of the same size
  size_t size = vectors[0].size();
  for (const auto &vec : vectors)
  {
    if (vec.size() != size)
    {
      std::cerr << "All vectors must be of the same size.\n";
      return false;
    }
  }

  std::ofstream outFile(filename);
  if (!outFile)
  {
    std::cerr << "Could not open file " << filename << " for writing.\n";
    return false;
  }

  for (size_t i = 0; i < size; ++i)
  {
    for (size_t j = 0; j < vectors.size(); ++j)
    {
      outFile << vectors[j][i];
      if (j < vectors.size() - 1)
        outFile << " "; // change delimiter here if needed
    }
    outFile << "\n";
  }

  outFile.close();
  return true;
}

void plotHistoNIST()
{
  std::string filename = "nist_w.txt";
  std::vector<std::vector<double>> data = readColumnWiseData(filename);

  // Print to verify
  for (size_t row = 0; row < data[0].size(); ++row)
  {
    for (size_t col = 0; col < data.size(); ++col)
    {
      std::cout << data[col][row] << " ";
    }
    std::cout << endl;
  }
  for (double &val : data[0])
  {
    val *= 1e3; // MeV->keV
  }
  // Draw histos filled by Geant4 simulation
  //

  cout << "alalaa adasdadasd" << endl;
  TFile *f(TFile::Open("./runtest0.root"));
  f->ls();

  TCanvas *c1 = new TCanvas("c1", "  ");
  cout << "c1: " << c1 << endl;
  c1->SetLogx();
  c1->SetLogy();
  c1->SetGrid();
  TH1D *airEnergy = (TH1D *)(f->Get("0"));
  TH1D *primaryEnergy = (TH1D *)(f->Get("1"));
  TH1D *scatterEnergy = (TH1D *)(f->Get("2"));
  TH1D *IPEMSourceEnergy = (TH1D *)(f->Get("spectrum"));
  // hist1->Draw("Incident energy");

  double bin = primaryEnergy->GetNbinsX();
  double thickness = 2.0 * 10.0 * 1e-4; // um to cm : 10^-4
  double rho = 19.254;

  vector<double> x, y;
  for (size_t i = 1; i < bin - 1; i++)
  {
    double tair = airEnergy->GetAt(i);
    // double tair = 10000000.0/1024.0;
    double tval = primaryEnergy->GetAt(i);
    double en = primaryEnergy->GetXaxis()->GetBinCenter(i);
    double tx = en; // keV
    double ty = log(tair / tval) / thickness / rho;

    if (std::isnan(ty) || std::isinf(ty))
    {
      cout << i << " adfasdfasdfasdfadfasdfasdfasdfasdfasdf " << ty << endl;
      continue;
    }
    x.push_back(tx);
    y.push_back(ty);
  }

  std::vector<std::vector<double>> towrite = {x, y};
  writeVectorsToFile(towrite, "output.txt");

  auto gr = new TGraph(x.size(), x.data(), y.data());
  gr->Draw("AL");
  c1->Update();

  auto gr_nist = new TGraph((double)data[0].size(), data[0].data(), data[1].data());
  gr_nist->SetLineColor(kRed);
  gr_nist->Draw("L SAME");

  gr_nist->SetTitle("Attenuation;keV;mu/rho");
  gr_nist->GetXaxis()->SetNdivisions(910); // major = 5, minor = 10
  c1->Update();

  TLegend *legend = new TLegend(0.7, 0.7, 0.9, 0.85); // x1, y1, x2, y2 in NDC (0–1)
  legend->AddEntry(gr_nist, "nist", "l");             // "l" = line style
  legend->AddEntry(gr, "geant", "l");                 // "l" = line style
  legend->Draw();
  c1->Update();
}
void saveTH2DAsBinary(TH2D *hist, const std::string &filename)
{
  std::ofstream out(filename, std::ios::binary);
  int nx = hist->GetNbinsX();
  int ny = hist->GetNbinsY();

  for (int iy = ny; iy > 0; --iy)
  {
    for (int ix = 1; ix <= nx; ++ix)
    {
      double val = hist->GetBinContent(ix, iy);
      out.write(reinterpret_cast<const char *>(&val), sizeof(double));
    }
  }
  out.close();
}
void plotHitMaps()
{
  TFile *f(TFile::Open("../runtest0.root"));

  TH2D *airMap = (TH2D *)(f->Get("3"));
  TH2D *primaryMap = (TH2D *)(f->Get("4"));
  TH2D *scatterMap = (TH2D *)(f->Get("5"));
  f->ls();
  gStyle->SetOptStat(0);

  // Calculate average of airMap (excluding under/overflow bins)
  double sum = 0;
  int nBins = 0;
  for (int ix = 1; ix <= airMap->GetNbinsX(); ++ix)
  {
    for (int iy = 1; iy <= airMap->GetNbinsY(); ++iy)
    {
      double val = airMap->GetBinContent(ix, iy);
      sum += val;
      nBins++;
    }
  }
  double avg = (nBins > 0) ? sum / nBins : 0;

  // Scale primaryMap and scatterMap
  for (int ix = 1; ix <= airMap->GetNbinsX(); ++ix)
  {
    for (int iy = 1; iy <= airMap->GetNbinsY(); ++iy)
    {
      double airVal = airMap->GetBinContent(ix, iy);
      if (airVal != 0)
      {
        double scale = avg / airVal;
        primaryMap->SetBinContent(ix, iy, primaryMap->GetBinContent(ix, iy) * scale);
        scatterMap->SetBinContent(ix, iy, scatterMap->GetBinContent(ix, iy) * scale);
      }
    }
  }

  // Plot airEnergy2D
  TCanvas *c2D1 = new TCanvas("c2D1", "Air Energy Deposition", 600, 600);
  c2D1->SetRightMargin(0.15);
  // c2D1->SetLogz(); // optional
  airMap->Draw("COLZ");
  c2D1->Update();

  // Plot primaryEnergy2D
  TCanvas *c2D2 = new TCanvas("c2D2", "Primary Energy Deposition", 600, 600);
  c2D2->SetRightMargin(0.15);
  // c2D2->SetLogz(); // optional
  primaryMap->Draw("COLZ");
  c2D2->Update();


  // Plot scatterEnergy2D
  TCanvas *c2D3 = new TCanvas("c2D3", "Scatter Energy Deposition", 600, 600);
  c2D3->SetRightMargin(0.15);
  // c2D3->SetLogz(); // optional
  scatterMap->Draw("COLZ");
  c2D3->Update();


    // Save with dimensions in filename
  int nx = primaryMap->GetNbinsX();
  int ny = primaryMap->GetNbinsY();
  std::ostringstream oss1, oss2, oss3;
  oss1 << "primaryMap float " << nx << "x" << ny << ".raw";
  oss2 << "scatterMap float " << nx << "x" << ny << ".raw";
  oss3 << "airMap float " << nx << "x" << ny << ".raw";

  saveTH2DAsBinary(primaryMap, oss1.str());
  saveTH2DAsBinary(scatterMap, oss2.str());
  saveTH2DAsBinary(airMap, oss3.str());
}
void plotHistoScatterEnergy()
{
  TFile *f(TFile::Open("run0.root"));
  f->ls();

  TH1D *scatterEnergy = (TH1D *)(f->Get("2"));

  // Plot primaryEnergy2D
  TCanvas *c2D4 = new TCanvas("c2D4", "Scatter Energy distribution", 600, 600);
  c2D4->SetRightMargin(0.15);
  // c2D2->SetLogz(); // optional
  scatterEnergy->Draw("HIST");
  scatterEnergy->SetTitle("Scatter energy;keV;mu/rho");
  scatterEnergy->GetXaxis()->SetNdivisions(990); // major = 5, minor = 10
  scatterEnergy->GetXaxis()->SetMoreLogLabels();
  c2D4->SetLogx();
  c2D4->SetLogy();
  c2D4->SetGrid();
}
std::pair<std::vector<double>, std::vector<double>> HistoToVectors(TH1D *histo, double scaleX = 1.0, double scaleY = 1.0)
{
  vector<double> x, y;
  double bin = histo->GetNbinsX();
  for (size_t i = 1; i < bin - 1; i++)
  {
    double tval = histo->GetAt(i);
    double en = histo->GetXaxis()->GetBinCenter(i);
    double tx = en * scaleX;
    double ty = tval * scaleY;
    // cout<<i<<" "<<tx<<" "<<ty<<endl;

    x.push_back(tx);
    y.push_back(ty);
  }
  return std::make_pair(x, y);
}
std::pair<std::vector<double>, std::vector<double>> NtupleToVectorsScaled(
    TNtuple *ntuple,
    const char *branch1 = "var0",
    const char *branch2 = "var1")
{
  std::vector<double> x, y;
  double v1, v2;

  ntuple->SetBranchAddress(branch1, &v1);
  ntuple->SetBranchAddress(branch2, &v2);
  Long64_t nentries = ntuple->GetEntries();
  for (Long64_t i = 0; i < nentries; ++i)
  {
    ntuple->GetEntry(i);
    x.push_back(v1);
    y.push_back(v2);
  }
  return std::make_pair(x, y);
}
void plotHistoIPEMPrimary()
{
  TFile *f(TFile::Open("run0.root"));
  f->ls();
  TH1D *airEnergyHisto = (TH1D *)(f->Get("0"));

  TNtuple *IPEMSourceEnergyHisto = (TNtuple *)(f->Get("spectrum_ntuple"));
  IPEMSourceEnergyHisto->Print();
  auto [kvIPEM, IPEMcounts] = NtupleToVectorsScaled(IPEMSourceEnergyHisto, "energy_keV", "intensity");
  for (auto &val : kvIPEM)
    val *= 1e3; // convert from MeV to keV
  auto maxIPEM = *std::max_element(IPEMcounts.begin(), IPEMcounts.end());
  for (auto &val : IPEMcounts)
    val /= maxIPEM; // normalize to maximum
  auto [kv, airCounts] = HistoToVectors(airEnergyHisto, 1.0, 1.0 / airEnergyHisto->GetMaximum());

  // Plot primaryEnergy2D
  TCanvas *c = new TCanvas("c2D5", "I0 Energy distribution", 600, 600);
  c->SetRightMargin(0.15);
  // c2D2->SetLogz(); // optional

  auto IPEMSourceEnergy1 = new TGraph(kvIPEM.size(), kvIPEM.data(), IPEMcounts.data());
  auto airEnergy1 = new TGraph(kv.size(), kv.data(), airCounts.data());

  IPEMSourceEnergy1->SetTitle("Energy spectra;keV;count");
  IPEMSourceEnergy1->SetLineColor(kRed); // Set line color to blue
  IPEMSourceEnergy1->SetLineWidth(5);
  IPEMSourceEnergy1->Draw("ALF");
  c->Update();

  airEnergy1->SetLineColor(kBlue); // Set line color to blue
  airEnergy1->Draw("L SAME");      // Draw with line and markers
  c->Update();

  // primaryEnergy->GetXaxis()->SetNdivisions(990);  // major = 5, minor = 10
  // primaryEnergy->GetXaxis()->SetMoreLogLabels();

  TLegend *legend1 = new TLegend(0.7, 0.7, 0.9, 0.85); // x1, y1, x2, y2 in NDC (0–1)
  legend1->AddEntry(airEnergy1, "air", "l");           // "l" = line style
  legend1->AddEntry(IPEMSourceEnergy1, "IPEM", "l");   // "l" = line style
  legend1->Draw();
  c->Update();
  c->SetGrid();
}

void StandaloneApplication(int argc, char **argv)
{
  // eventually, evaluate the application parameters argc, argv
  // ==>> here the ROOT macro is called
  // plotHistoIPEMPrimary();
  plotHitMaps();
}
// This is the standard "main" of C++ starting
// a ROOT application
int main(int argc, char **argv)
{
  TApplication app("ROOT Application", &argc, argv);
  StandaloneApplication(app.Argc(), app.Argv());
  app.Run();
  return 0;
}

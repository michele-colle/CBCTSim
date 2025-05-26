{
  // Draw histos filled by Geant4 simulation 
  //   
  TFile f = TFile("run0.root");  
  TCanvas* c1 = new TCanvas("c1", "  ");
  
  TH1D* hist1 = (TH1D*)f.Get("1");
  hist1->Draw("Incident energy");

  double n = hist1->GetEntries();
  double bin = hist1->GetNbinsX();
  double air = n/bin;

  TH1D* hist2 = (TH1D*)hist1->Clone();
  for (size_t i = 0; i < bin; i++)
  {
    hist2->SetAt(log(air)-log(hist1->GetAt(i)), i);
  }
  hist2->Draw("Attenuation");
  
}  

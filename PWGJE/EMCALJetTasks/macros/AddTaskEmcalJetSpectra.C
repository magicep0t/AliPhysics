

AliAnalysisTaskEmcalJetSpectra* AddTaskEmcalJetSpectra(
   const char *outfilename    = "AnalysisOutput.root",
   const char *nJets          = "Jets",
   UInt_t type                = 0,//AliAnalysisTaskEmcal::kTPC,
   const char *nRhosChEm      = "rhoChEm",
   const char *lrho           = "lrho",
   const Double_t minPhi      = 1.8,
   const Double_t maxPhi      = 2.74,
   const Double_t minEta      = -0.3,
   const Double_t maxEta      = 0.3,
   const Double_t minArea     = 0.4,
   const char *nTracks        = "PicoTracks"
)
{  
  // Get the pointer to the existing analysis manager via the static access method.
  //==============================================================================
  AliAnalysisManager *mgr = AliAnalysisManager::GetAnalysisManager();
  if (!mgr)
  {
    ::Error("AddTasEmcalJetSpectra", "No analysis manager to connect to.");
    return NULL;
  }  
  
  // Check the analysis type using the event handlers connected to the analysis manager.
  //==============================================================================
  if (!mgr->GetInputEventHandler())
  {
    ::Error("AddTaskEmcalJetSpectra", "This task requires an input event handler");
    return NULL;
  }
  
  //-------------------------------------------------------
  // Init the task and do settings
  //-------------------------------------------------------

  TString name(Form("Spectra_%s", nJets));
  AliAnalysisTaskEmcalJetSpectra *spectratask = new AliAnalysisTaskEmcalJetSpectra(name);
  spectratask->AddJetContainer(nJets);
  spectratask->SetAnaType(type);
  spectratask->SetRhoName(nRhosChEm);
  spectratask->SetLocalRhoName(lrho);
  spectratask->SetJetPhiLimits(minPhi,maxPhi);
  spectratask->SetJetEtaLimits(minEta,maxEta);
  spectratask->SetJetAreaCut(minArea);
  spectratask->AddParticleContainer(nTracks);

  //-------------------------------------------------------
  // Final settings, pass to manager and set the containers
  //-------------------------------------------------------

  mgr->AddTask(spectratask);

  // Create containers for input/output
  mgr->ConnectInput (spectratask, 0, mgr->GetCommonInputContainer() );
  AliAnalysisDataContainer *cospectra = mgr->CreateContainer(name,
                                                           TList::Class(),
                                                           AliAnalysisManager::kOutputContainer,
                                                           outfilename);
  mgr->ConnectOutput(spectratask,1,cospectra);

  return spectratask;
}

#include "AliAnalysisTaskCharmingFemto.h"

#include "AliAnalysisManager.h"
#include "AliInputEventHandler.h"
#include "AliMultSelection.h"
#include "AliPIDResponse.h"
#include "AliRDHFCutsDplustoKpipi.h"
#include "AliAODRecoDecayHF.h"
#include "AliAODRecoDecayHF3Prong.h"
#include "AliHFMLResponse.h"
#include "AliAODHandler.h"
#include "AliHFMLResponseDplustoKpipi.h"
#include "TDatabasePDG.h"

ClassImp(AliAnalysisTaskCharmingFemto)

    //____________________________________________________________________________________________________
    AliAnalysisTaskCharmingFemto::AliAnalysisTaskCharmingFemto()
    : AliAnalysisTaskSE("AliAnalysisTaskCharmingFemto"),
      fInputEvent(nullptr),
      fEvent(nullptr),
      fEvtCuts(nullptr),
      fProtonTrack(nullptr),
      fTrackCutsPartProton(nullptr),
      fTrackCutsPartAntiProton(nullptr),
      fConfig(nullptr),
      fPairCleaner(nullptr),
      fPartColl(nullptr),
      fIsMC(false),
      fIsLightweight(false),
      fTrigger(AliVEvent::kINT7),
      fTrackBufferSize(2500),
      fDmesonPDGs{},
      fGTI(nullptr),
      fQA(nullptr),
      fEvtHistList(nullptr),
      fTrackCutHistList(nullptr),
      fTrackCutHistMCList(nullptr),
      fAntiTrackCutHistList(nullptr),
      fAntiTrackCutHistMCList(nullptr),
      fDChargedHistList(nullptr),
      fResultList(nullptr),
      fResultQAList(nullptr),
      fHistDplusInvMassPt(nullptr),
      fHistDplusEta(nullptr),
      fHistDplusPhi(nullptr),
      fHistDplusChildPt(),
      fHistDplusChildEta(),
      fHistDplusChildPhi(),
      fHistDminusInvMassPt(nullptr),
      fHistDminusEta(nullptr),
      fHistDminusPhi(nullptr),
      fHistDminusChildPt(),
      fHistDminusChildEta(),
      fHistDminusChildPhi(),
      fDecChannel(kDplustoKpipi),
      fRDHFCuts(nullptr),
      fAODProtection(0),
      fDoNSigmaMassSelection(true),
      fNSigmaMass(3.),
      fLowerMassSelection(0.),
      fUpperMassSelection(999.),
      fApplyML(false),
      fConfigPath(""),
      fMLResponse(nullptr) {}

//____________________________________________________________________________________________________
AliAnalysisTaskCharmingFemto::AliAnalysisTaskCharmingFemto(const char *name,
                                                           const bool isMC)
    : AliAnalysisTaskSE(name),
      fInputEvent(nullptr),
      fEvent(nullptr),
      fEvtCuts(nullptr),
      fProtonTrack(nullptr),
      fTrackCutsPartProton(nullptr),
      fTrackCutsPartAntiProton(nullptr),
      fConfig(nullptr),
      fPairCleaner(nullptr),
      fPartColl(nullptr),
      fIsMC(isMC),
      fIsLightweight(false),
      fTrigger(AliVEvent::kINT7),
      fTrackBufferSize(2500),
      fDmesonPDGs{},
      fGTI(nullptr),
      fQA(nullptr),
      fEvtHistList(nullptr),
      fTrackCutHistList(nullptr),
      fTrackCutHistMCList(nullptr),
      fAntiTrackCutHistList(nullptr),
      fAntiTrackCutHistMCList(nullptr),
      fDChargedHistList(nullptr),
      fResultList(nullptr),
      fResultQAList(nullptr),
      fHistDplusInvMassPt(nullptr),
      fHistDplusEta(nullptr),
      fHistDplusPhi(nullptr),
      fHistDplusChildPt(),
      fHistDplusChildEta(),
      fHistDplusChildPhi(),
      fHistDminusInvMassPt(nullptr),
      fHistDminusEta(nullptr),
      fHistDminusPhi(nullptr),
      fHistDminusChildPt(),
      fHistDminusChildEta(),
      fHistDminusChildPhi(),
      fDecChannel(kDplustoKpipi),
      fRDHFCuts(nullptr),
      fAODProtection(0),
      fDoNSigmaMassSelection(true),
      fNSigmaMass(3.),
      fLowerMassSelection(0.),
      fUpperMassSelection(999.),
      fApplyML(false),
      fConfigPath(""),
      fMLResponse(nullptr) {
  DefineInput(0, TChain::Class());
  DefineOutput(1, TList::Class());
  DefineOutput(2, TList::Class());
  DefineOutput(3, TList::Class());
  DefineOutput(4, TList::Class());
  DefineOutput(5, TList::Class());
  DefineOutput(6, TList::Class());
  DefineOutput(7, TList::Class());
  switch(fDecChannel){ // save cut object for HF particle
    case kDplustoKpipi:
    {
      DefineOutput(8, AliRDHFCutsDplustoKpipi::Class());
      break;
    }
    default:
    {
      AliFatal("Invalid HF channel.");
      break;
    }
  }

  if (fIsMC) {
    DefineOutput(9, TList::Class());
    DefineOutput(10, TList::Class());
  }
}

//____________________________________________________________________________________________________
AliAnalysisTaskCharmingFemto::~AliAnalysisTaskCharmingFemto() {
  delete fPartColl;
  delete fPairCleaner;
  delete fProtonTrack;
  delete fRDHFCuts;

  if(fApplyML && fMLResponse) {
    delete fMLResponse;
  }
}

//____________________________________________________________________________________________________
void AliAnalysisTaskCharmingFemto::LocalInit()
{
    // Initialization
    switch(fDecChannel) {
      case kDplustoKpipi:
        AliRDHFCutsDplustoKpipi* copyCut = new AliRDHFCutsDplustoKpipi(*(static_cast<AliRDHFCutsDplustoKpipi*>(fRDHFCuts)));
        PostData(8, copyCut);
      break;
    }
  return;
}

//____________________________________________________________________________________________________
void AliAnalysisTaskCharmingFemto::UserExec(Option_t * /*option*/) {
  fInputEvent = static_cast<AliAODEvent*>(InputEvent());
  if (fIsMC)
    fMCEvent = MCEvent();

  // PREAMBLE - CHECK EVERYTHING IS THERE
  if (!fInputEvent) {
    AliError("No Input event");
    return;
  }

  // Protection against the mismatch of candidate TRefs:
  // Check if AOD and corresponding deltaAOD files contain the same number of events.
  // In case of discrepancy the event is rejected.
  if(fAODProtection >= 0) {
    // Protection against different number of events in the AOD and deltaAOD
    // In case of discrepancy the event is rejected.
    int matchingAODdeltaAODlevel = AliRDHFCuts::CheckMatchingAODdeltaAODevents();
    if (matchingAODdeltaAODlevel < 0 || (matchingAODdeltaAODlevel == 0 && fAODProtection == 1)) {
      // AOD/deltaAOD trees have different number of entries || TProcessID do not match while it was required
      return;
    }
  }

  // GET HF CANDIDATE ARRAY
  TClonesArray *arrayHF = nullptr;
  int absPdgMom = 0;
  if(!fInputEvent && AODEvent() && IsStandardAOD()) {
    // In case there is an AOD handler writing a standard AOD, use the AOD
    // event in memory rather than the input (ESD) event.
    fInputEvent = dynamic_cast<AliAODEvent*>(AODEvent());
    // in this case the braches in the deltaAOD (AliAOD.VertexingHF.root)
    // have to taken from the AOD event hold by the AliAODExtension
    AliAODHandler* aodHandler = (AliAODHandler*)((AliAnalysisManager::GetAnalysisManager())->GetOutputEventHandler());
    if(aodHandler->GetExtensions()) {
      AliAODExtension *ext = (AliAODExtension*)aodHandler->GetExtensions()->FindObject("AliAOD.VertexingHF.root");
      AliAODEvent *aodFromExt = ext->GetAOD();
      switch(fDecChannel) {
        case kDplustoKpipi:
          absPdgMom = 411;
          arrayHF = dynamic_cast<TClonesArray*>(aodFromExt->GetList()->FindObject("Charm3Prong"));
          break;
      }
    }
  }
  else if(fInputEvent){
    switch(fDecChannel) {
      case kDplustoKpipi:
        absPdgMom = 411;
        arrayHF = dynamic_cast<TClonesArray*>(fInputEvent->GetList()->FindObject("Charm3Prong"));
        break;
    }
  }

  if(!arrayHF) {
    AliError("Branch not found!\n");
    return;
  }

  if (fIsMC && !fMCEvent) {
    AliError("No MC event");
    return;
  }

  if (!fEvtCuts) {
    AliError("Event Cuts missing");
    return;
  }

  if (!fTrackCutsPartProton || !fTrackCutsPartAntiProton) {
    AliError("Proton Cuts missing");
    return;
  }

  // Reset the pair cleaner
  fPairCleaner->ResetArray();

  // EVENT SELECTION
  fEvent->SetEvent(fInputEvent);
  if (!fEvtCuts->isSelected(fEvent))
    return;

  // PROTON SELECTION
  ResetGlobalTrackReference();
  for (int iTrack = 0; iTrack < fInputEvent->GetNumberOfTracks(); ++iTrack) {
    AliAODTrack *track = static_cast<AliAODTrack *>(fInputEvent->GetTrack(
        iTrack));
    if (!track) {
      AliFatal("No Standard AOD");
      return;
    }
    StoreGlobalTrackReference(track);
  }
  static std::vector<AliFemtoDreamBasePart> protons;
  static std::vector<AliFemtoDreamBasePart> antiprotons;
  protons.clear();
  antiprotons.clear();
  const int multiplicity = fEvent->GetMultiplicity();
  fProtonTrack->SetGlobalTrackInfo(fGTI, fTrackBufferSize);
  for (int iTrack = 0; iTrack < fInputEvent->GetNumberOfTracks(); ++iTrack) {
    AliAODTrack *track = static_cast<AliAODTrack *>(fInputEvent->GetTrack(
        iTrack));
    fProtonTrack->SetTrack(track);
    fProtonTrack->SetInvMass(0.938);
    if (fTrackCutsPartProton->isSelected(fProtonTrack)) {
      protons.push_back(*fProtonTrack);
    }
    if (fTrackCutsPartAntiProton->isSelected(fProtonTrack)) {
      antiprotons.push_back(*fProtonTrack);
    }
  }

  // D MESON SELECTION
  static std::vector<AliFemtoDreamBasePart> dplus;
  static std::vector<AliFemtoDreamBasePart> dminus;
  dplus.clear();
  dminus.clear();

  // needed to initialise PID response
  fRDHFCuts->IsEventSelected(fInputEvent);

  AliAODRecoDecayHF *dMeson = nullptr;
  int nCand = arrayHF->GetEntriesFast();
  for (int iCand = 0; iCand < nCand; iCand++) {
    dMeson = dynamic_cast<AliAODRecoDecayHF*>(arrayHF->UncheckedAt(iCand));

    bool unsetVtx = false;
    bool recVtx = false;
    AliAODVertex *origOwnVtx = nullptr;

    int isSelected = IsCandidateSelected(dMeson, absPdgMom, unsetVtx, recVtx, origOwnVtx);
    if(!isSelected) {
      if (unsetVtx) {
        dMeson->UnsetOwnPrimaryVtx();
      }
      if (recVtx) {
        fRDHFCuts->CleanOwnPrimaryVtx(dMeson, fInputEvent, origOwnVtx);
      }
      continue;
    }

    const double mass = dMeson->InvMass(fDmesonPDGs.size(), &fDmesonPDGs[0]);
    if (dMeson->Charge() > 0) {
      fHistDplusInvMassPt->Fill(dMeson->Pt(), mass);
    } else {
      fHistDminusInvMassPt->Fill(dMeson->Pt(), mass);
    }

    // select D mesons mass window
    if (fDoNSigmaMassSelection) {
      // simple parametrisation from D+ in 5.02 TeV
      double massMean =
          TDatabasePDG::Instance()->GetParticle(absPdgMom)->Mass() +
          0.0025;  // mass shift observed in all Run2 data samples for all
                   // D-meson species
      double massWidth = 0.;
      switch (fDecChannel) {
        case kDplustoKpipi:
          massWidth = 0.0057 + dMeson->Pt() * 0.00066;
          break;
      }
      fLowerMassSelection = massMean - fNSigmaMass * massWidth;
      fUpperMassSelection = massMean + fNSigmaMass * massWidth;
    }
    if( mass > fLowerMassSelection && mass < fUpperMassSelection) {
      if (dMeson->Charge() > 0) {
        dplus.push_back( { dMeson, fInputEvent, (int)fDmesonPDGs.size(), &fDmesonPDGs[0] });
        fHistDplusEta->Fill(dMeson->Eta());
        fHistDplusPhi->Fill(dMeson->Phi());
        for (unsigned int iChild = 0; iChild < fDmesonPDGs.size(); iChild++) {
          AliAODTrack *track = (AliAODTrack *)dMeson->GetDaughter(iChild);
          fHistDplusChildPt[iChild]->Fill(track->Pt());
          fHistDplusChildEta[iChild]->Fill(track->Eta());
          fHistDplusChildPhi[iChild]->Fill(track->Phi());
        }
      }
      else {
        dminus.push_back( { dMeson, fInputEvent, (int)fDmesonPDGs.size(), &fDmesonPDGs[0] });
        fHistDminusEta->Fill(dMeson->Eta());
        fHistDminusPhi->Fill(dMeson->Phi());
        for (unsigned int iChild = 0; iChild < fDmesonPDGs.size(); iChild++) {
          AliAODTrack *track = (AliAODTrack *)dMeson->GetDaughter(iChild);
          fHistDminusChildPt[iChild]->Fill(track->Pt());
          fHistDminusChildEta[iChild]->Fill(track->Eta());
          fHistDminusChildPhi[iChild]->Fill(track->Phi());
        }
      }
    }

    if (unsetVtx) {
      dMeson->UnsetOwnPrimaryVtx();
    }
    if (recVtx) {
      fRDHFCuts->CleanOwnPrimaryVtx(dMeson, fInputEvent, origOwnVtx);
    }
  }

  // PAIR CLEANING AND FEMTO
  fPairCleaner->CleanTrackAndDecay(&protons, &dplus, 0);
  fPairCleaner->CleanTrackAndDecay(&protons, &dminus, 1);
  fPairCleaner->CleanTrackAndDecay(&antiprotons, &dplus, 2);
  fPairCleaner->CleanTrackAndDecay(&antiprotons, &dminus, 3);

  fPairCleaner->StoreParticle(protons);
  fPairCleaner->StoreParticle(antiprotons);
  fPairCleaner->StoreParticle(dplus);
  fPairCleaner->StoreParticle(dminus);

  fPartColl->SetEvent(fPairCleaner->GetCleanParticles(), fEvent);

  // flush the data
  PostData(1, fQA);
  PostData(2, fEvtHistList);
  PostData(3, fTrackCutHistList);
  PostData(4, fAntiTrackCutHistList);
  PostData(5, fDChargedHistList);
  PostData(6, fResultList);
  PostData(7, fResultQAList);
  if (fIsMC) {
    PostData(9, fTrackCutHistMCList);
    PostData(10, fAntiTrackCutHistMCList);
  }
}

//____________________________________________________________________________________________________
void AliAnalysisTaskCharmingFemto::ResetGlobalTrackReference() {
  // see AliFemtoDreamAnalysis for details
  for (int i = 0; i < fTrackBufferSize; i++) {
    fGTI[i] = 0;
  }
}

//____________________________________________________________________________________________________
void AliAnalysisTaskCharmingFemto::StoreGlobalTrackReference(
    AliAODTrack *track) {
  const int trackID = track->GetID();
  if (trackID < 0) {
    return;
  }

  if (trackID >= fTrackBufferSize) {
    printf("Warning: track ID too big for buffer: ID: %d, buffer %d\n", trackID,
           fTrackBufferSize);
    return;
  }

  if (fGTI[trackID]) {
    if ((!track->GetFilterMap()) && (!track->GetTPCNcls())) {
      return;
    }
    if (fGTI[trackID]->GetFilterMap() || fGTI[trackID]->GetTPCNcls()) {
      printf("Warning! global track info already there!");
      printf("         TPCNcls track1 %u track2 %u",
             (fGTI[trackID])->GetTPCNcls(), track->GetTPCNcls());
      printf("         FilterMap track1 %u track2 %u\n",
             (fGTI[trackID])->GetFilterMap(), track->GetFilterMap());
    }
  }
  (fGTI[trackID]) = track;
}

//____________________________________________________________________________________________________
void AliAnalysisTaskCharmingFemto::UserCreateOutputObjects() {
  fGTI = new AliAODTrack *[fTrackBufferSize];

  fEvent = new AliFemtoDreamEvent(true, !fIsLightweight, fTrigger);

  fProtonTrack = new AliFemtoDreamTrack();
  fProtonTrack->SetUseMCInfo(fIsMC);

  fPairCleaner = new AliFemtoDreamPairCleaner(4, 0,
                                              fConfig->GetMinimalBookingME());
  fPartColl = new AliFemtoDreamPartCollection(fConfig,
                                              fConfig->GetMinimalBookingME());

  fQA = new TList();
  fQA->SetName("QA");
  fQA->SetOwner(kTRUE);

  if (fEvtCuts) {
    fEvtCuts->InitQA();
    fQA->Add(fEvent->GetEvtCutList());
    if (fEvtCuts->GetHistList() && !fIsLightweight) {
      fEvtHistList = fEvtCuts->GetHistList();
    } else {
      fEvtHistList = new TList();
      fEvtHistList->SetName("EvtCuts");
      fEvtHistList->SetOwner(true);
    }
  } else {
    AliWarning("Event cuts are missing! \n");
  }

  if (!fConfig->GetMinimalBookingME() && fPairCleaner
      && fPairCleaner->GetHistList()) {
    fQA->Add(fPairCleaner->GetHistList());
  }

  fTrackCutsPartProton->Init("Proton");
  // necessary for the non-min booking case
  fTrackCutsPartProton->SetName("Proton");

  if (fTrackCutsPartProton && fTrackCutsPartProton->GetQAHists()) {
    fTrackCutHistList = fTrackCutsPartProton->GetQAHists();
    if (fIsMC && fTrackCutsPartProton->GetMCQAHists()
        && fTrackCutsPartProton->GetIsMonteCarlo()) {
      fTrackCutHistMCList = fTrackCutsPartProton->GetMCQAHists();
    }
  }

  fTrackCutsPartAntiProton->Init("Anti-proton");
  // necessary for the non-min booking case
  fTrackCutsPartAntiProton->SetName("Anti-proton");

  if (fTrackCutsPartAntiProton && fTrackCutsPartAntiProton->GetQAHists()) {
    fAntiTrackCutHistList = fTrackCutsPartAntiProton->GetQAHists();
    if (fIsMC && fTrackCutsPartAntiProton->GetMCQAHists()
        && fTrackCutsPartAntiProton->GetIsMonteCarlo()) {
      fAntiTrackCutHistMCList = fTrackCutsPartAntiProton->GetMCQAHists();
    }
  }

  // Eventually we might put this in a separate class but for the moment let's just leave it floating around here
  fDChargedHistList  = new TList();
  fDChargedHistList->SetName("DChargedQA");
  fDChargedHistList->SetOwner(true);

  fHistDplusInvMassPt = new TH2F("fHistDplusInvMassPt", "; #it{p}_{T} (GeV/#it{c}); #it{M}_{K#pi#pi} (GeV/#it{c}^{2})", 250, 0, 25, 1000, 1.77, 1.97);
  fDChargedHistList->Add(fHistDplusInvMassPt);
  fHistDplusEta = new TH1F("fHistDplusEta", ";#eta; Entries", 100, -1, 1);
  fDChargedHistList->Add(fHistDplusEta);
  fHistDplusPhi = new TH1F("fHistDplusPhi", ";#phi; Entries", 100, 0., 2.*TMath::Pi());
  fDChargedHistList->Add(fHistDplusPhi);

  fHistDminusInvMassPt = new TH2F("fHistDminusInvMassPt", "; #it{p}_{T} (GeV/#it{c}); #it{M}_{K#pi#pi} (GeV/#it{c}^{2})", 250, 0, 25, 1000, 1.77, 1.97);
  fDChargedHistList->Add(fHistDminusInvMassPt);
  fHistDminusEta = new TH1F("fHistDminusEta", ";#eta; Entries", 100, -1, 1);
  fDChargedHistList->Add(fHistDminusEta);
  fHistDminusPhi = new TH1F("fHistDminusPhi", ";#phi; Entries", 100, 0., 2.*TMath::Pi());
  fDChargedHistList->Add(fHistDminusPhi);

  std::vector<TString> nameVec;
  if (fDecChannel == kDplustoKpipi) {
    nameVec = {{"K", "Pi1", "Pi2"}};
  }
  for (unsigned int iChild = 0; iChild < fDmesonPDGs.size() ; ++iChild) {
    fHistDplusChildPt[iChild] = new TH1F(TString::Format("fHistDplusChildPt_%s", nameVec.at(iChild).Data()), "; #it{p}_{T} (GeV/#it{c}); Entries", 250, 0, 25);
    fHistDplusChildEta[iChild] = new TH1F(TString::Format("fHistDplusChildEta_%s", nameVec.at(iChild).Data()), "; #eta; Entries", 100, -1, 1);
    fHistDplusChildPhi[iChild] = new TH1F(TString::Format("fHistDplusChildPhi_%s", nameVec.at(iChild).Data()), "; #phi; Entries", 100, 0, 2.*TMath::Pi());
    fDChargedHistList->Add(fHistDplusChildPt[iChild]);
    fDChargedHistList->Add(fHistDplusChildEta[iChild]);
    fDChargedHistList->Add(fHistDplusChildPhi[iChild]);
    fHistDminusChildPt[iChild] = new TH1F(TString::Format("fHistDminusChildPt_%s", nameVec.at(iChild).Data()), "; #it{p}_{T} (GeV/#it{c}); Entries", 250, 0, 25);
    fHistDminusChildEta[iChild] = new TH1F(TString::Format("fHistDminusChildEta_%s", nameVec.at(iChild).Data()), "; #eta; Entries", 100, -1, 1);
    fHistDminusChildPhi[iChild] = new TH1F(TString::Format("fHistDminusChildPhi_%s", nameVec.at(iChild).Data()), "; #phi; Entries", 100, 0, 2.*TMath::Pi());
    fDChargedHistList->Add(fHistDminusChildPt[iChild]);
    fDChargedHistList->Add(fHistDminusChildEta[iChild]);
    fDChargedHistList->Add(fHistDminusChildPhi[iChild]);
  }
  

  if (fPartColl && fPartColl->GetHistList()) {
    fResultList = fPartColl->GetHistList();
  }
  if (!fConfig->GetMinimalBookingME() && fPartColl && fPartColl->GetQAList()) {
    fResultQAList = fPartColl->GetQAList();
  } else {
    fResultQAList = new TList();
    fResultQAList->SetName("ResultsQA");
    fResultQAList->SetOwner(true);
  }

  //ML model
  if(fApplyML) {
    switch(fDecChannel) {
      case kDplustoKpipi:
        fMLResponse = new AliHFMLResponseDplustoKpipi("DplustoKpipiMLResponse", "DplustoKpipiMLResponse", fConfigPath.Data());
        fMLResponse->MLResponseInit();
        break;
    }
  }

  PostData(1, fQA);
  PostData(2, fEvtHistList);
  PostData(3, fTrackCutHistList);
  PostData(4, fAntiTrackCutHistList);
  PostData(5, fDChargedHistList);
  PostData(6, fResultList);
  PostData(7, fResultQAList);
  if (fIsMC) {
    PostData(9, fTrackCutHistMCList);
    PostData(10, fAntiTrackCutHistMCList);
  }
}

//________________________________________________________________________
int AliAnalysisTaskCharmingFemto::IsCandidateSelected(AliAODRecoDecayHF *&dMeson, int absPdgMom, bool &unsetVtx, bool &recVtx, AliAODVertex *&origOwnVtx) {

  if(!dMeson) {
    return 0;
  }

  bool isSelBit = true;
  switch(fDecChannel) {
    case kDplustoKpipi:
      isSelBit = dMeson->HasSelectionBit(AliRDHFCuts::kDplusCuts);
      if(!isSelBit) {
        return 0;
      }
      break;
  }

  unsetVtx = false;
  if (!dMeson->GetOwnPrimaryVtx())
  {
    dMeson->SetOwnPrimaryVtx(dynamic_cast<AliAODVertex *>(fInputEvent->GetPrimaryVertex()));
    unsetVtx = true;
    // NOTE: the own primary vertex should be unset, otherwise there is a memory leak
    // Pay attention if you use continue inside this loop!!!
  }

  double ptD = dMeson->Pt();
  double yD = dMeson->Y(absPdgMom);
  int ptbin = fRDHFCuts->PtBin(ptD);
  if(ptbin < 0) {
    if (unsetVtx) {
      dMeson->UnsetOwnPrimaryVtx();
    }
    return 0;
  }

  bool isFidAcc = fRDHFCuts->IsInFiducialAcceptance(ptD, yD);
  if(!isFidAcc) {
    if (unsetVtx) {
      dMeson->UnsetOwnPrimaryVtx();
    }
    return 0;
  }

  int isSelected = fRDHFCuts->IsSelected(dMeson, AliRDHFCuts::kAll, fInputEvent);
  if(!isSelected) {
    if (unsetVtx) {
      dMeson->UnsetOwnPrimaryVtx();
    }
    return 0;
  }

  // ML application
  if(fApplyML) {
    AliAODPidHF* pidHF = fRDHFCuts->GetPidHF();
    bool isMLsel = true;
    std::vector<double> modelPred{};
    switch(fDecChannel) {
      case kDplustoKpipi:
        isMLsel = fMLResponse->IsSelectedMultiClass(modelPred, dMeson, fInputEvent->GetMagneticField(), pidHF);
        if(!isMLsel) {
          isSelected = 0;
        }
        break;
    }
  }

  recVtx = false;
  origOwnVtx = nullptr;

  if (fRDHFCuts->GetIsPrimaryWithoutDaughters())
  {
    if (dMeson->GetOwnPrimaryVtx()) {
      origOwnVtx = new AliAODVertex(*dMeson->GetOwnPrimaryVtx());
    }
    if (fRDHFCuts->RecalcOwnPrimaryVtx(dMeson, fInputEvent)) {
      recVtx = true;
    }
    else {
      fRDHFCuts->CleanOwnPrimaryVtx(dMeson, fInputEvent, origOwnVtx);
    }
  }
  
  return isSelected;
}

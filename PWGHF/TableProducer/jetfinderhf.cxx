// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

// hf jet finder task
//
// Author: Nima Zardoshti

#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/ASoA.h"

#include "PWGHF/DataModel/HFSecondaryVertex.h"
#include "PWGHF/DataModel/HFCandidateSelectionTables.h"

#include "fastjet/PseudoJet.hh"
#include "fastjet/ClusterSequenceArea.hh"

#include "PWGJE/DataModel/Jet.h"
#include "PWGJE/Core/JetFinder.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;


struct JetFinderHFTask {
  Produces<o2::aod::Jets> jetsTable;
  Produces<o2::aod::JetTrackConstituents> trackConstituents;
  OutputObj<TH1F> hJetPt{"h_jet_pt"};
  OutputObj<TH1F> hJetPhi{"h_jet_phi"};
  OutputObj<TH1F> hJetEta{"h_jet_eta"};
  OutputObj<TH1F> hJetNTracks{"h_jet_ntracks"};
  OutputObj<TH1F> hD0Pt{"h_D0_pt"};
  OutputObj<TH1F> hD0Status{"h_D0_status"};

  std::vector<fastjet::PseudoJet> jets;
  std::vector<fastjet::PseudoJet> inputParticles;
  JetFinder jetFinder;

  void init(InitContext const&)
  {
    hJetPt.setObject(new TH1F("h_jet_pt", "jet p_{T};p_{T} (GeV/#it{c})",
                              100, 0., 100.));
    hJetPhi.setObject(new TH1F("h_jet_phi", "jet #phi;#phi",
                             130, -6.5, 6.5));
    hJetEta.setObject(new TH1F("h_jet_eta", "jet #eta;#eta",
                             200, -1.0, 1.0));
    hJetNTracks.setObject(new TH1F("h_jet_ntracks", "jet n tracks;jet n tracks",
                             40, -0.5, 39.5));
    hD0Pt.setObject(new TH1F("h_D0_pt", "jet p_{T,D};p_{T,D} (GeV/#it{c})",
                             100, 0., 10.));
    hD0Status.setObject(new TH1F("h_D0_status", "status;status",
                             3, 0.5, 3.5));
  }

  Configurable<int> d_selectionFlagD0{"d_selectionFlagD0", 1, "Selection Flag for D0"};
  Configurable<int> d_selectionFlagD0bar{"d_selectionFlagD0bar", 1, "Selection Flag for D0bar"};

  //need enum as configurable
  enum pdgCode { pdgD0 = 421 };

  Filter trackCuts = (aod::track::pt > 0.15f && aod::track::eta > -0.9f && aod::track::eta < 0.9f);
  Filter seltrack = (aod::hf_selcandidate_d0::isSelD0 >= d_selectionFlagD0 || aod::hf_selcandidate_d0::isSelD0bar >= d_selectionFlagD0bar);

  void process(aod::Collision const& collision,
               soa::Filtered<aod::Tracks> const& tracks,
               soa::Filtered<soa::Join<aod::HfCandProng2, aod::HFSelD0Candidate>> const& candidates)
  {
    // TODO: retrieve pion mass from somewhere
    bool isHFJet;

    //this loop should be made more efficient
    for (auto& candidate : candidates) {
      jets.clear();
      inputParticles.clear();
      auto daughter1Id = candidate.index0Id();
      auto daughter2Id = candidate.index1Id();
      for (auto& track : tracks) {
        auto energy = std::sqrt(track.p() * track.p() + JetFinder::mPion * JetFinder::mPion);
        if (daughter1Id == track.globalIndex() || daughter2Id == track.globalIndex()) { //is it global index?
          continue;
        }
        inputParticles.emplace_back(track.px(), track.py(), track.pz(), energy);
        inputParticles.back().set_user_index(track.globalIndex());
      }
      inputParticles.emplace_back(candidate.px(), candidate.py(), candidate.pz(), candidate.e(RecoDecay::getMassPDG(pdgD0)));
      if (candidate.isSelD0() == 1 && candidate.isSelD0bar() == 0){
        inputParticles.back().set_user_index(-1);
      }
      else if (candidate.isSelD0() == 0 && candidate.isSelD0bar() == 1){
        inputParticles.back().set_user_index(-2);
      }
      else { //if (candidate.isSelD0() == 1 && candidate.isSelD0bar() == 1){ 
        inputParticles.back().set_user_index(-3);
      }

      fastjet::ClusterSequenceArea clusterSeq(jetFinder.findJets(inputParticles, jets));

      for (const auto& jet : jets) {
        isHFJet = false;
        for (const auto& constituent : jet.constituents()) {
          if (constituent.user_index() == -1 || constituent.user_index() == -2 || constituent.user_index() == -3) { //cen be removed
            isHFJet = true;
            hD0Pt->Fill(constituent.pt()); //can be removed
            hD0Status->Fill(TMath::Abs(constituent.user_index())); //can be removed
            break;
          }
        }
        if (isHFJet) {
          jetsTable(collision, jet.eta(), jet.phi(), jet.pt(),
                    jet.area(), jet.E(), jet.m(), jetFinder.jetR);
          for (const auto& constituent : jet.constituents()) {
            trackConstituents(jetsTable.lastIndex(), constituent.user_index());
          }
          hJetPt->Fill(jet.pt());
          hJetPhi->Fill(jet.phi());
          hJetEta->Fill(jet.eta());
          hJetNTracks->Fill(jet.constituents().size());
          break;
        }
      }
    }
  }
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<JetFinderHFTask>(cfgc, TaskName{"jet-finder-hf"})};
}

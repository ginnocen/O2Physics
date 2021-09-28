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

/// \file HFCandidateCreatorChic.cxx
/// \brief Reconstruction of chi_c candidates
/// \note Adapted from HFCandidateCreatorX
///
/// \author Alessandro De Falco <alessandro.de.falco@ca.infn.it>, Cagliari University

#include "Framework/AnalysisTask.h"
#include "DetectorsVertexing/DCAFitterN.h"
#include "PWGHF/DataModel/HFSecondaryVertex.h"
#include "Common/Core/trackUtilities.h"
#include "ReconstructionDataFormats/DCA.h"
#include "ReconstructionDataFormats/V0.h"
#include "PWGHF/DataModel/HFCandidateSelectionTables.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::aod::hf_cand;
using namespace o2::aod::hf_cand_prong2;
using namespace o2::aod::hf_cand_prong3;
using namespace o2::aod::hf_cand_chic; 
using namespace o2::framework::expressions;

void customize(std::vector<o2::framework::ConfigParamSpec>& workflowOptions)
{
  ConfigParamSpec optionDoMC{"doMC", VariantType::Bool, true, {"Perform MC matching."}};
  workflowOptions.push_back(optionDoMC);
}

#include "Framework/runDataProcessing.h"

/// Reconstruction of chic candidates
struct HFCandidateCreatorChic {
  Produces<aod::HfCandChicBase> rowCandidateBase; 

  Configurable<double> magneticField{"magneticField", 5., "magnetic field"};
  Configurable<bool> b_propdca{"b_propdca", true, "create tracks version propagated to PCA"};
  Configurable<double> d_maxr{"d_maxr", 200., "reject PCA's above this radius"};
  Configurable<double> d_maxdzini{"d_maxdzini", 4., "reject (if>0) PCA candidate if tracks DZ exceeds threshold"};
  Configurable<double> d_minparamchange{"d_minparamchange", 1.e-3, "stop iterations if largest change of any X is smaller than this"};
  Configurable<double> d_minrelchi2change{"d_minrelchi2change", 0.9, "stop iterations is chi2/chi2old > this"};
  Configurable<double> eneGammaMin{"eneGammaMin", 0.00, "minimum gamma energy threshold (GeV)"};
  Configurable<double> etaGammaMin{"etaGammaMin", -100.00, "minimum gamma pseudorapidity"};
  Configurable<double> etaGammaMax{"etaGammaMax", 100.00, "maximum gamma pseudorapidity"};
  Configurable<bool> b_dovalplots{"b_dovalplots", true, "do validation plots"};

  OutputObj<TH1F> hMassJpsi{TH1F("hMassJpsi", "J/#psi candidates;inv. mass (#e+ e-) (GeV/#it{c}^{2});entries", 500, 0., 5.)};
  OutputObj<TH1F> hPtJpsi{TH1F("hPtJpsi", "J/#psi candidates;candidate #it{p}_{T} (GeV/#it{c});entries", 100, 0., 10.)};
  OutputObj<TH1F> hCPAJpsi{TH1F("hCPAJpsi", "J/#psi candidates;cosine of pointing angle;entries", 110, -1.1, 1.1)};
  OutputObj<TH1F> hMassChic{TH1F("hMassChic", "2-prong candidates;inv. mass (J/#psi #gamma) (GeV/#it{c}^{2});entries", 500, 0., 5.)};
  OutputObj<TH1F> hCovPVXX{TH1F("hCovPVXX", "2-prong candidates;XX element of cov. matrix of prim. vtx position (cm^{2});entries", 100, 0., 1.e-4)};
  OutputObj<TH1F> hCovSVXX{TH1F("hCovSVXX", "2-prong candidates;XX element of cov. matrix of sec. vtx position (cm^{2});entries", 100, 0., 0.2)};

//  double massPi = RecoDecay::getMassPDG(kPiPlus);
  double massJpsi = RecoDecay::getMassPDG(443);
  double massJpsiGamma;

  Configurable<int> d_selectionFlagJpsi{"d_selectionFlagJpsi", 1, "Selection Flag for Jpsi"};
  Configurable<double> cutYCandMax{"cutYCandMax", -1., "max. cand. rapidity"};
  Filter filterSelectCandidates = (aod::hf_selcandidate_jpsi::isSelJpsiToEE >= d_selectionFlagJpsi);

  void process(aod::Collision const& collision,
               soa::Filtered<soa::Join<
                 aod::HfCandProng2,
                 aod::HFSelJpsiCandidate>> const& jpsiCands,
               aod::BigTracks const& tracks)
  {
    // 2-prong vertex fitter (to rebuild Jpsi vertex)
    o2::vertexing::DCAFitterN<2> df2;
    df2.setBz(magneticField);
    df2.setPropagateToPCA(b_propdca);
    df2.setMaxR(d_maxr);
    df2.setMaxDZIni(d_maxdzini);
    df2.setMinParamChange(d_minparamchange);
    df2.setMinRelChi2Change(d_minrelchi2change);
    df2.setUseAbsDCA(true);

    // 3-prong vertex fitter
//    o2::vertexing::DCAFitterN<3> df3;
    o2::vertexing::DCAFitterN<2> df3;   // jpsi gamma has two prongs
    df3.setBz(magneticField);
    df3.setPropagateToPCA(b_propdca);
    df3.setMaxR(d_maxr);
    df3.setMaxDZIni(d_maxdzini);
    df3.setMinParamChange(d_minparamchange);
    df3.setMinRelChi2Change(d_minrelchi2change);
    df3.setUseAbsDCA(true);

    // loop over Jpsi candidates
    for (auto& jpsiCand : jpsiCands) {
      if (!(jpsiCand.hfflag() & 1 << JpsiToEE)) {
        continue;
      }
      if (cutYCandMax >= 0. && std::abs(YJpsi(jpsiCand)) > cutYCandMax) {
        continue;
      }
      hMassJpsi->Fill(InvMassJpsiToEE(jpsiCand));
      hPtJpsi->Fill(jpsiCand.pt());
      hCPAJpsi->Fill(jpsiCand.cpa());
      // create Jpsi track to pass to DCA fitter; use cand table + rebuild vertex
      const std::array<float, 3> vertexJpsi = {jpsiCand.xSecondaryVertex(), jpsiCand.ySecondaryVertex(), jpsiCand.zSecondaryVertex()};
      array<float, 3> pvecJpsi = {jpsiCand.px(), jpsiCand.py(), jpsiCand.pz()};
      auto prong0 = jpsiCand.index0_as<aod::BigTracks>();
      auto prong1 = jpsiCand.index1_as<aod::BigTracks>();
      auto prong0TrackParCov = getTrackParCov(prong0);
      auto prong1TrackParCov = getTrackParCov(prong1);

      if (df2.process(prong0TrackParCov, prong1TrackParCov) == 0) {
        continue;
      }

      // propogate prong tracks to Jpsi vertex
      prong0TrackParCov.propagateTo(jpsiCand.xSecondaryVertex(), magneticField);
      prong1TrackParCov.propagateTo(jpsiCand.xSecondaryVertex(), magneticField);
      const std::array<float, 6> covJpsi = df2.calcPCACovMatrixFlat();
      // define the Jpsi track
      auto trackJpsi = o2::dataformats::V0(vertexJpsi, pvecJpsi, covJpsi, prong0TrackParCov, prong1TrackParCov, {0, 0}, {0, 0}); //FIXME: also needs covxyz???

      // used to check that prongs used for Jpsi and X reco are not the same prongs
      int index0Jpsi = jpsiCand.index0Id();
      int index1Jpsi = jpsiCand.index1Id();

      // -----------------------------------------------------------------
      // adf: this has to be rewritten
      
      // loop over pi+ candidates
      for (auto& trackPos : tracks) {   // adf for now use the pi+ instead of gamma
        if (trackPos.sign() < 0) { // select only positive tracks - use partitions?
          continue;
        }

//        if (trackPos.E() < eneGammaMin) {
//          continue;
//        }

//        if (trackPos.Eta() < etaGammaMin || trackPos.Eta() > etaGammaMax ) {
//          continue;
//        }

        auto trackParVarPos = getTrackParCov(trackPos);   
        array<float, 3> pvecPos;

        // reconstruct the Jpsi-gamma vertex
        if (df3.process(trackJpsi, trackParVarPos) == 0) {
          continue;
        }

        // calculate relevant properties
        const auto& ChicsecondaryVertex = df3.getPCACandidate();
        auto chi2PCA = df3.getChi2AtPCACandidate();
        auto covMatrixPCA = df3.calcPCACovMatrix().Array();
        hCovSVXX->Fill(covMatrixPCA[0]); // FIXME: Calculation of errorDecayLength(XY) gives wrong values without this line.

        df3.propagateTracksToVertex();          // propagate the pions and Jpsi to the X vertex
        df3.getTrack(0).getPxPyPzGlo(pvecJpsi); // update momentum of Jpsi at the X vertex
        df3.getTrack(1).getPxPyPzGlo(pvecPos);  // momentum of pi+ at the X vertex

        // get track impact parameters
        // This modifies track momenta!
        auto primaryVertex = getPrimaryVertex(collision);
        auto covMatrixPV = primaryVertex.getCov();
        hCovPVXX->Fill(covMatrixPV[0]);
        o2::dataformats::DCA impactParameter0;
        o2::dataformats::DCA impactParameter1;
        trackJpsi.propagateToDCA(primaryVertex, magneticField, &impactParameter0);
        trackParVarPos.propagateToDCA(primaryVertex, magneticField, &impactParameter1);

        // get uncertainty of the decay length
        double phi, theta;
        getPointDirection(array{collision.posX(), collision.posY(), collision.posZ()}, ChicsecondaryVertex, phi, theta);
        auto errorDecayLength = std::sqrt(getRotatedCovMatrixXX(covMatrixPV, phi, theta) + getRotatedCovMatrixXX(covMatrixPCA, phi, theta));
        auto errorDecayLengthXY = std::sqrt(getRotatedCovMatrixXX(covMatrixPV, phi, 0.) + getRotatedCovMatrixXX(covMatrixPCA, phi, 0.));

        int hfFlag = 1 << ChicToJpsiGamma;  

        // fill the candidate table for the chi_c here:
        rowCandidateBase(collision.globalIndex(),
                         collision.posX(), collision.posY(), collision.posZ(),
                         ChicsecondaryVertex[0], ChicsecondaryVertex[1], ChicsecondaryVertex[2],
                         errorDecayLength, errorDecayLengthXY,
                         chi2PCA,
                         pvecJpsi[0], pvecJpsi[1], pvecJpsi[2],
                         pvecPos[0], pvecPos[1], pvecPos[2],
//                           pvecNeg[0], pvecNeg[1], pvecNeg[2],
                         impactParameter0.getY(), impactParameter1.getY(), 
                        //impactParameter2.getY(),
                         std::sqrt(impactParameter0.getSigmaY2()), std::sqrt(impactParameter1.getSigmaY2()),
                         jpsiCand.globalIndex(), trackPos.globalIndex(),
                         hfFlag);

        // calculate invariant mass
        auto arrayMomenta = array{pvecJpsi, pvecPos};
        massJpsiGamma = RecoDecay::M(std::move(arrayMomenta), array{massJpsi, 0.});
        hMassChic->Fill(massJpsiGamma);
      }   // pi+ loop
      // ---------------------------- adf: chic specific code stops here
    }     // Jpsi loop
  }       // process
};        // struct

/// Extends the base table with expression columns.
struct HFCandidateCreatorChicExpressions {
  //  Spawns<aod::HfCandChicExt> rowCandidateChic; 
  Spawns<aod::HfCandChicExt> rowCandidateChic;  
  void init(InitContext const&) {}
};

/// Performs MC matching.
struct HFCandidateCreatorChicMC {
  Produces<aod::HfCandChicMCRec> rowMCMatchRec;   
  Produces<aod::HfCandChicMCGen> rowMCMatchGen;   

  void process(aod::HfCandChic const& candidates,  // adf should this be such a thing?
               aod::HfCandProng2,            
               aod::BigTracksMC const& tracks,
               aod::McParticles const& particlesMC)
  {
    int indexRec = -1;
    int8_t sign = 0;
    int8_t flag = 0;
    int8_t origin = 0;
    int8_t channel = 0;

    // Match reconstructed candidates.
    for (auto& candidate : candidates) {
      //Printf("New rec. candidate");
      flag = 0;
      origin = 0;
      channel = 0;
      auto jpsiTrack = candidate.index0();
      auto arrayDaughters = array{candidate.index1_as<aod::BigTracksMC>(),
//                                  candidate.index2_as<aod::BigTracksMC>(), adf: this should not be here, correct?
                                  jpsiTrack.index0_as<aod::BigTracksMC>(),
                                  jpsiTrack.index1_as<aod::BigTracksMC>()};

      // chi_c → J/ψ gamma
      //Printf("Checking X → J/ψ π+ π-");
      indexRec = RecoDecay::getMatchedMCRec(particlesMC, arrayDaughters, 20443, array{+kPiPlus, +kElectron, -kElectron}, true, &sign, 2); // adf change this when the photon is available
      if (indexRec > -1) {
        flag = 1 << ChicToJpsiGamma;  
      }

      // Check whether the particle is non-prompt (from a b quark).
      if (flag != 0) {
        auto particle = particlesMC.iteratorAt(indexRec);
        origin = (RecoDecay::getMother(particlesMC, particle, 5, true) > -1 ? NonPrompt : Prompt);
      }

      rowMCMatchRec(flag, origin, channel);
    }

    // Match generated particles.
    for (auto& particle : particlesMC) {
      //Printf("New gen. candidate");
      flag = 0;
      origin = 0;
      channel = 0;

      // chi_c → J/ψ gamma 
      //Printf("Checking X → J/ψ π+ π-");
      if (RecoDecay::isMatchedMCGen(particlesMC, particle, 20443, array{443, +kPiPlus}, true)) {  // adf change this! 
        // Match J/psi --> e+e-
        std::vector<int> arrDaughter;
        RecoDecay::getDaughters(particlesMC, particle, &arrDaughter, array{443}, 1);
        auto jpsiCandMC = particlesMC.iteratorAt(arrDaughter[0]);
        if (RecoDecay::isMatchedMCGen(particlesMC, jpsiCandMC, 443, array{+kElectron, -kElectron}, true)) {
          flag = 1 << ChicToJpsiGamma; 
        }
      }

      // Check whether the particle is non-prompt (from a b quark).
      if (flag != 0) {
        origin = (RecoDecay::getMother(particlesMC, particle, 5, true) > -1 ? NonPrompt : Prompt);
      }

      rowMCMatchGen(flag, origin, channel);
    } // candidate loop
  }   // process
};    // struct

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  WorkflowSpec workflow{
    adaptAnalysisTask<HFCandidateCreatorChic>(cfgc, TaskName{"hf-cand-creator-chic"}),
    adaptAnalysisTask<HFCandidateCreatorChicExpressions>(cfgc, TaskName{"hf-cand-creator-chic-expressions"})};
  const bool doMC = cfgc.options().get<bool>("doMC");
  if (doMC) {
    workflow.push_back(adaptAnalysisTask<HFCandidateCreatorChicMC>(cfgc, TaskName{"hf-cand-creator-chic-mc"}));
  }
  return workflow;
}

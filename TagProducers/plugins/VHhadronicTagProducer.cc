#include "FWCore/Framework/interface/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "DataFormats/Common/interface/Handle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/EDMException.h"
#include "DataFormats/TrackReco/interface/HitPattern.h"
#include "DataFormats/PatCandidates/interface/Jet.h"
#include "DataFormats/Math/interface/deltaR.h"

#include "flashgg/MicroAODFormats/interface/Jet.h"
#include "flashgg/MicroAODFormats/interface/DiPhotonCandidate.h"
#include "flashgg/TagFormats/interface/VHhadronicTag.h"

#include <vector>
#include <algorithm>
#include <string>
#include <utility>
#include <TLorentzVector.h>
#include "TMath.h"

using namespace std;
using namespace edm;

typedef std::pair< edm::Ptr<flashgg::Jet>,edm::Ptr<flashgg::Jet> > jetptrpair;


namespace flashgg {
  
  class VHhadronicTagProducer : public EDProducer {
    
  public:

    VHhadronicTagProducer( const ParameterSet & );
  private:
    
    void produce( Event &, const EventSetup & ) override;
    
    EDGetTokenT<View<DiPhotonCandidate> > diPhotonToken_;
    EDGetTokenT<View<Jet> > thejetToken_;
    EDGetTokenT<View<DiPhotonMVAResult> > mvaResultToken_;
    
    //Thresholds
    double leadPhoOverMassThreshold_;
    double subleadPhoOverMassThreshold_;
    double diphoMVAThreshold_;
    double jetsNumberThreshold_;
    double jetPtThreshold_;
    double jetEtaThreshold_;
    double phoIdMVAThreshold_;
    

  };

  VHhadronicTagProducer::VHhadronicTagProducer(const ParameterSet & iConfig) :

    diPhotonToken_(consumes<View<flashgg::DiPhotonCandidate> >(iConfig.getUntrackedParameter<InputTag> ("DiPhotonTag", InputTag("flashggDiPhotons")))),
    thejetToken_(consumes<View<flashgg::Jet> >(iConfig.getUntrackedParameter<InputTag>("JetTag",InputTag("flashggJets")))),
    mvaResultToken_(consumes<View<flashgg::DiPhotonMVAResult> >(iConfig.getUntrackedParameter<InputTag> ("MVAResultTag", InputTag("flashggDiPhotonMVA"))))
  {

    // ***** define thresholds ***********************

    double default_leadPhoOverMassThreshold_    = 0.375;
    double default_subleadPhoOverMassThreshold_ = 0.25;
    double default_diphoMVAThreshold_           = -1.0;  // CHECK THIS
    double default_jetsNumberThreshold_         = 2;
    double default_jetPtThreshold_              = 40.;
    double default_jetEtaThreshold_             = 2.4;
    double default_PuIDCutoffThreshold_         = 0.8;  // CHECK THIS
    double default_phoIdMVAThreshold_           = -0.2; // assumes we apply the same cut as for all other VH categories
    
    leadPhoOverMassThreshold_    = iConfig.getUntrackedParameter<double>("leadPhoOverMassThreshold",default_leadPhoOverMassThreshold_);
    subleadPhoOverMassThreshold_ = iConfig.getUntrackedParameter<double>("subleadPhoOverMassThreshold",default_subleadPhoOverMassThreshold_);
    diphoMVAThreshold_           = iConfig.getUntrackedParameter<double>("diphoMVAThreshold",default_diphoMVAThreshold_);
    jetsNumberThreshold_         = iConfig.getUntrackedParameter<double>("jetsNumberThreshold",default_jetsNumberThreshold_);
    jetPtThreshold_              = iConfig.getUntrackedParameter<double>("jetPtThreshold",default_jetPtThreshold_);
    jetEtaThreshold_             = iConfig.getUntrackedParameter<double>("jetEtaThreshold",default_jetEtaThreshold_);
    phoIdMVAThreshold_           = iConfig.getUntrackedParameter<double>("phoIdMVAThreshold",default_phoIdMVAThreshold_);

    // *************************************************

    produces<vector<VHhadronicTag> >(); 
  }

  void VHhadronicTagProducer::produce( Event & evt, const EventSetup & )
  {

    Handle<View<flashgg::DiPhotonCandidate> > diPhotons;
    evt.getByToken(diPhotonToken_,diPhotons);
    const PtrVector<flashgg::DiPhotonCandidate>& diPhotonPointers = diPhotons->ptrVector();  
  
    Handle<View<flashgg::Jet> > theJets;
    evt.getByToken(thejetToken_,theJets);
    const PtrVector<flashgg::Jet>& jetPointers = theJets->ptrVector();
  
    Handle<View<flashgg::DiPhotonMVAResult> > mvaResults;
    evt.getByToken(mvaResultToken_,mvaResults);
    const PtrVector<flashgg::DiPhotonMVAResult>& mvaResultPointers = mvaResults->ptrVector();
    
    std::auto_ptr<vector<VHhadronicTag> > vhhadtags(new vector<VHhadronicTag>);
    
    assert(diPhotonPointers.size() == mvaResultPointers.size());

    bool   tagged = false;
    double idmva1 = 0.;
    double idmva2 = 0.;

    for(unsigned int diphoIndex = 0; diphoIndex < diPhotonPointers.size(); diphoIndex++ )
      {
	
	edm::Ptr<flashgg::DiPhotonCandidate> dipho = diPhotonPointers[diphoIndex];
	edm::Ptr<flashgg::DiPhotonMVAResult> mvares = mvaResultPointers[diphoIndex];
	
	// ********** photon ID and diphoton requirements: *********

	if(dipho->leadingPhoton()->pt() < (dipho->mass())*leadPhoOverMassThreshold_) continue;
	if(dipho->subLeadingPhoton()->pt() < (dipho->mass())*subleadPhoOverMassThreshold_) continue;

	idmva1 = dipho->leadingPhoton()->getPhoIdMvaDWrtVtx(dipho->getVertex());
	idmva2 = dipho->subLeadingPhoton()->getPhoIdMvaDWrtVtx(dipho->getVertex());
	if(idmva1 <= phoIdMVAThreshold_|| idmva2 <= phoIdMVAThreshold_) continue;

	if(mvares->result < diphoMVAThreshold_) continue;

	// jet-selection logic: loop over jets and build a vector of jets passing kinematical cuts. second loop to check if at least one pair of jets passes the invariant mass requirement. MAKE SURE THIS IS THE GOOD WAY TO GO
	
	edm::PtrVector<flashgg::Jet> goodJets;

	for( size_t ijet = 0; ijet < jetPointers.size(); ijet++ ) {

	  edm::Ptr<flashgg::Jet> thejet = jetPointers[ijet];
	  //if( thejet->getPuJetId(dipho) <  PuIDCutoffThreshold_ ) continue;
	  if (!thejet->passesPuJetId(dipho))                        continue;
	  if( fabs(thejet->eta()) > jetEtaThreshold_ )              continue; 
	  if( thejet->pt() < jetPtThreshold_ )                      continue;
	  	  
	  goodJets.push_back( thejet );
	} 

	// *********************************************************************

	vector<jetptrpair> tagJetPairs;
	
	for( size_t jjet = 0; jjet < goodJets.size(); jjet++ ) {

	  for( size_t kjet = (jjet+1); kjet < goodJets.size(); kjet++ ) {

	    TLorentzVector jetl, jets, dijet, phol, phos, diphoton, vstar; 
	    jetl.SetPtEtaPhiE( jetPointers[jjet]->pt(),jetPointers[jjet]->eta(),jetPointers[jjet]->phi(),jetPointers[jjet]->energy() );
	    jets.SetPtEtaPhiE( jetPointers[kjet]->pt(),jetPointers[kjet]->eta(),jetPointers[kjet]->phi(),jetPointers[kjet]->energy() );
	    phol.SetPtEtaPhiE( dipho->leadingPhoton()->pt(), dipho->leadingPhoton()->eta(), dipho->leadingPhoton()->phi(), dipho->leadingPhoton()->energy() );
	    phos.SetPtEtaPhiE( dipho->subLeadingPhoton()->pt(), dipho->subLeadingPhoton()->eta(), dipho->subLeadingPhoton()->phi(), dipho->subLeadingPhoton()->energy() );
	    dijet = jetl + jets;
	    diphoton = phol + phos;
	    vstar = diphoton + dijet;
	    
	    float invmass = dijet.M();
	    if( invmass < 60 || invmass > 120 ) continue;
	    
	    dijet.Boost( -vstar.BoostVector() );     // check definition of costhetastar angle
	    float costhetastar = TMath::Cos( dijet.Angle(vstar.BoostVector()) );  
	    if( abs(costhetastar) > 0.5 ) continue; 

	    tagJetPairs.push_back( make_pair(jetPointers[jjet],jetPointers[kjet]) );
	  }
	} // end of double jet loop
	
	// at least ONE pair should pass the selection on invariant mass
	if( tagJetPairs.size() > 0 ) {
	  
	  tagged = true;
	  VHhadronicTag vhhadtag_obj(dipho,mvares);
	  vhhadtag_obj.setJets( tagJetPairs[0].first, tagJetPairs[0].second );    // select the first pair (highest pt jets)
	  vhhadtags->push_back( vhhadtag_obj );
	}

      }  // END OF DIPHOTON LOOP
    
    evt.put(vhhadtags);
  }



}
typedef flashgg::VHhadronicTagProducer FlashggVHhadronicTagProducer;
DEFINE_FWK_MODULE(FlashggVHhadronicTagProducer);

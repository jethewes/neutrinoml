////////////////////////////////////////////////////////////////////////
// Class:       HDF5Maker
// Plugin Type: analyzer (art v3_06_03)
// File:        HDF5Maker_module.cc
//
// Generated at Wed May  5 08:23:31 2021 by Jeremy Hewes using cetskelgen
// from cetlib version v3_11_01.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "canvas/Persistency/Common/FindManyP.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "nusimdata/SimulationBase/MCTruth.h"
#include "lardataobj/RecoBase/Hit.h"
#include "lardataobj/RecoBase/SpacePoint.h"

#include "larsim/MCCheater/BackTrackerService.h"
#include "larsim/MCCheater/ParticleInventoryService.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"

#include "hep_hpc/hdf5/make_ntuple.hpp"

#include "numl/NeutrinoML/NeutrinoMLUtils.h"

using std::array;
using std::endl;
using std::setfill;
using std::set;
using std::setw;
using std::string;
using std::vector;

using simb::MCParticle;
using simb::MCTruth;
using sim::TrackIDE;
using recob::Hit;
using recob::SpacePoint;

using mf::LogInfo;

using art::ServiceHandle;
using cheat::BackTrackerService;
using cheat::ParticleInventoryService;
using detinfo::DetectorClocksService;
using detinfo::DetectorPropertiesService;

using hep_hpc::hdf5::Column;
using hep_hpc::hdf5::make_ntuple;
using hep_hpc::hdf5::make_scalar_column;
using hep_hpc::hdf5::make_column;

namespace numl {

  class HDF5Maker : public art::EDAnalyzer {
  public:
    explicit HDF5Maker(fhicl::ParameterSet const& p);
    ~HDF5Maker() noexcept {}; // bare pointers are cleaned up by endSubRun

    HDF5Maker(HDF5Maker const&) = delete;
    HDF5Maker(HDF5Maker&&) = delete;
    HDF5Maker& operator=(HDF5Maker const&) = delete;
    HDF5Maker& operator=(HDF5Maker&&) = delete;

    void beginSubRun(art::SubRun const& sr) override;
    void endSubRun(art::SubRun const& sr) override;
    void analyze(art::Event const& e) override;

  private:

    string fTruthLabel;
    string fHitLabel;
    string fSPLabel;

    string fOutputName;

    hep_hpc::hdf5::File fFile;  ///< Output HDF5 file

    hep_hpc::hdf5::Ntuple<Column<int, 1>,    // event id (run, subrun, event)
                          Column<int, 1>,    // is cc
                          Column<float, 1>,  // nu energy
                          Column<float, 1>,  // lep energy
                          Column<float, 1>   // nu dir (x, y, z)
    >* fEventNtuple; ///< event ntuple

    hep_hpc::hdf5::Ntuple<Column<int, 1>,    // event id (run, subrun, event)
                          Column<int, 1>,    // spacepoint id
                          Column<float, 1>,  // 3d position (x, y, z)
                          Column<int, 1>     // 2d hit (u, v, y)
    >* fSpacePointNtuple; ///< spacepoint ntuple

    hep_hpc::hdf5::Ntuple<Column<int, 1>,    // event id (run, subrun, event)
                          Column<int, 1>,    // hit id
                          Column<float, 1>,  // hit integral
                          Column<float, 1>,  // hit rms
                          Column<int, 1>,    // tpc id
                          Column<int, 1>,    // global plane
                          Column<int, 1>,    // global wire
                          Column<float, 1>,  // global time
                          Column<int, 1>,    // raw plane
                          Column<int, 1>,    // raw wire
                          Column<float, 1>   // raw time
    >* fHitNtuple; ///< hit ntuple

    hep_hpc::hdf5::Ntuple<Column<int, 1>,    // event id (run, subrun, event)
                          Column<int, 1>,    // g4 id
                          Column<int, 1>,    // particle type
                          Column<int, 1>,    // parent g4 id
                          Column<float, 1>,  // momentum
                          Column<float, 1>,  // start position (x, y, z)
                          Column<float, 1>,  // end position (x, y, z)
                          Column<string, 1>, // start process
                          Column<string, 1>  // end process
    >* fParticleNtuple; ///< particle ntuple

    hep_hpc::hdf5::Ntuple<Column<int, 1>,    // event id (run, subrun, event)
                          Column<int, 1>,    // hit id
                          Column<int, 1>,    // g4 id
                          Column<float, 1>   // energy fraction
    >* fEnergyDepNtuple; ///< energy deposit ntuple
  };


  HDF5Maker::HDF5Maker(fhicl::ParameterSet const& p)
    : EDAnalyzer{p},
      fTruthLabel(p.get<string>("TruthLabel")),
      fHitLabel(  p.get<string>("HitLabel")),
      fSPLabel(   p.get<string>("SPLabel")),
      fOutputName(p.get<string>("OutputName"))
  {}

  void HDF5Maker::analyze(art::Event const& e)
  {
    art::ServiceHandle<cheat::BackTrackerService> bt;

    int run = e.id().run();
    int subrun = e.id().subRun();
    int event = e.id().event();

    array<int, 3> evtID { run, subrun, event };

    // Get MC truth
    art::Handle< vector< MCTruth > > truthHandle;
    e.getByLabel(fTruthLabel, truthHandle);
    if (!truthHandle.isValid() || truthHandle->size() != 1) {
      throw art::Exception(art::errors::LogicError)
        << "Expected to find exactly one MC truth object!";
    }
    simb::MCNeutrino nutruth = truthHandle->at(0).GetNeutrino();

    array<float, 3> nuMomentum {
      (float)nutruth.Nu().Momentum().Vect().Unit().X(),
      (float)nutruth.Nu().Momentum().Vect().Unit().Y(),
      (float)nutruth.Nu().Momentum().Vect().Unit().Z()
    };

    // Fill event table
    fEventNtuple->insert( evtID.data(),
      nutruth.CCNC() == simb::kCC,
      nutruth.Nu().E(),
      nutruth.Lepton().E(),
      nuMomentum.data()
    );

    LogInfo("HDF5Maker") << "Filling event table"
                         << "\nrun " << evtID[0] << ", subrun " << evtID[1]
                         << ", event " << evtID[2]
                         << "\nis cc? " << (nutruth.CCNC() == simb::kCC)
                         << ", nu energy " << nutruth.Nu().E()
                         << ", lepton energy " << nutruth.Lepton().E()
                         << "\nnu momentum x " << nuMomentum[0] << ", y "
                         << nuMomentum[1] << ", z " << nuMomentum[2];

    // Get spacepoints from the event record
    art::Handle< vector< SpacePoint > > spListHandle;
    vector< art::Ptr< SpacePoint > > splist;
    if (e.getByLabel(fSPLabel, spListHandle))
      art::fill_ptr_vector(splist, spListHandle);

    // Get hits from the event record
    art::Handle< vector< Hit > > hitListHandle;
    vector< art::Ptr< Hit > > hitlist;
    if (e.getByLabel(fHitLabel, hitListHandle))
      art::fill_ptr_vector(hitlist, hitListHandle);

    // Get assocations from spacepoints to hits
    art::FindManyP< Hit > fmp(spListHandle, e, fSPLabel);
    vector< vector< art::Ptr< Hit > > > sp2Hit(splist.size());
    for (size_t spIdx = 0; spIdx < sp2Hit.size(); ++spIdx) {
      sp2Hit[spIdx] = fmp.at(spIdx);
    } // for spacepoint

    // Fill spacepoint table
    for (size_t i = 0; i < splist.size(); ++i) {

      array<float, 3> pos {
        (float)splist[i]->XYZ()[0],
        (float)splist[i]->XYZ()[1],
        (float)splist[i]->XYZ()[2]
      };

      array<int, 3> hitID { 0, 0, 0 };
      for (size_t j = 0; j < 3; ++j)
        hitID[sp2Hit[i][j]->View()] = sp2Hit[i][j].key();

      fSpacePointNtuple->insert(evtID.data(),
        splist[i]->ID(), pos.data(), hitID.data()
      );

      LogInfo("HDF5Maker") << "Filling spacepoint table"
                           << "\nrun " << evtID[0] << ", subrun " << evtID[1]
                           << ", event " << evtID[2]
                           << "\nspacepoint id " << splist[i]->ID()
                           << "\nposition x " << pos[0] << ", y " << pos[1]
                           << ", z " << pos[2]
                           << "\nhit ids " << hitID[0] << ", " << hitID[1]
                           << ", " << hitID[2];

    } // for spacepoint

    std::set<int> g4id;
    auto const clockData = ServiceHandle<DetectorClocksService>()->DataFor(e);
    auto const detProp = ServiceHandle<DetectorPropertiesService>()->DataFor(e, clockData);

    // Loop over hits
    for (art::Ptr< Hit > hit : hitlist) {

      // Fill hit table
      geo::WireID wireid = hit->WireID();

      //array<float, 3> c = GlobalToLocal(wireid, hit->PeakTime());
      GlobalToLocal(wireid, hit->PeakTime());

      unsigned int plane, wire;
      double time;

      // size_t plane = wireid.Plane;
      // size_t wire = wireid.Wire;
      // double time = hit- >PeakTime();

      GetDUNE10ktGlobalWireTDC(detProp, wireid.Wire, hit->PeakTime(), wireid.Plane, wireid.TPC,
                                wire, plane, time);

      fHitNtuple->insert(evtID.data(),
        hit.key(), hit->Integral(), hit->RMS(), wireid.TPC,
        plane, wire, time,
        wireid.Plane, wireid.Wire, hit->PeakTime()
      );

      LogInfo("HDF5Maker") << "Filling hit table"
                           << "\nrun " << evtID[0] << ", subrun " << evtID[1]
                           << ", event " << evtID[2]
                           << "\nhit id " << hit.key() << ", integral "
                           << hit->Integral() << ", RMS " << hit->RMS()
                           << ", TPC " << wireid.TPC
                           << "\nglobal plane " << plane << ", global wire "
                           << wire << ", global time " << time
                           << "\nlocal plane " << wireid.Plane
                           << ", local wire " << wireid.Wire
                           << ", local time " << hit->PeakTime();

      // Fill energy deposit table
      for (const TrackIDE& ide : bt->HitToTrackIDEs(clockData, hit)) {
        g4id.insert(ide.trackID);
        fEnergyDepNtuple->insert(evtID.data(),
          hit.key(), ide.trackID, ide.energyFrac
        );
        LogInfo("HDF5Maker") << "Filling energy deposit table"
                             << "\nrun " << evtID[0] << ", subrun " << evtID[1]
                             << ", event " << evtID[2]
                             << "\nhit id " << hit.key() << ", g4 id "
                             << ide.trackID << ", energy fraction "
                             << ide.energyFrac;
      } // for energy deposit
    } // for hit

    ServiceHandle<ParticleInventoryService> pi;
    set<int> allIDs = g4id; // Copy original so we can safely modify it

    // Add invisible particles to hierarchy
    for (int id : g4id) {
      const MCParticle* p = pi->TrackIdToParticle_P(abs(id));
      while (p->Mother() != 0) {
        allIDs.insert(abs(p->Mother()));
        p = pi->TrackIdToParticle_P(abs(p->Mother()));
      }
    }

    // Loop over true particles and fill table
    for (int id : allIDs) {
      const MCParticle* p = pi->TrackIdToParticle_P(abs(id));
      array<float, 3> particleStart { (float)p->Vx(), (float)p->Vy(), (float)p->Vz() };
      array<float, 3> particleEnd { (float)p->EndX(), (float)p->EndY(), (float)p->EndZ() };
      fParticleNtuple->insert(evtID.data(),
        abs(id), p->PdgCode(), p->Mother(), (float)p->P(),
        particleStart.data(), particleEnd.data(),
        p->Process(), p->EndProcess()
      );
      LogInfo("HDF5Maker") << "Filling particle table"
                           << "\nrun " << evtID[0] << ", subrun " << evtID[1]
                           << ", event " << evtID[2]
                           << "\ng4 id " << abs(id) << ", pdg code "
                           << p->PdgCode() << ", parent " << p->Mother()
                           << ", momentum " << p->P()
                           << "\nparticle start x " << particleStart[0]
                           << ", y " << particleStart[1]
                           << ", z " << particleStart[2]
                           << "\nparticle end x " << particleEnd[0] << ", y "
                           << particleEnd[1] << ", z " << particleEnd[2]
                           << "\nstart process " << p->Process()
                           << ", end process " << p->EndProcess();
    }
  } // function HDF5Maker::analyze

  void HDF5Maker::beginSubRun(art::SubRun const& sr) {

    // Open HDF5 output
    std::ostringstream fileName;
    fileName << fOutputName << "_r" << setfill('0') << setw(5) << sr.run()
      << "_r" << setfill('0') << setw(5) << sr.subRun() << ".h5";

    fFile = hep_hpc::hdf5::File(fileName.str(), H5F_ACC_TRUNC);

    fEventNtuple = new hep_hpc::hdf5::Ntuple(
      make_ntuple({fFile, "event_table", 1000},
        make_column<int>("event_id", 3),
        make_scalar_column<int>("is_cc"),
        make_scalar_column<float>("nu_energy"),
        make_scalar_column<float>("lep_energy"),
        make_column<float>("nu_dir", 3)
    ));

    fSpacePointNtuple = new hep_hpc::hdf5::Ntuple(
      make_ntuple({fFile, "spacepoint_table", 1000},
        make_column<int>("event_id", 3),
        make_scalar_column<int>("spacepoint_id"),
        make_column<float>("position", 3),
        make_column<int>("hit_id", 3)
    ));

    fHitNtuple = new hep_hpc::hdf5::Ntuple(
      make_ntuple({fFile, "hit_table", 1000},
        make_column<int>("event_id", 3),
        make_scalar_column<int>("hit_id"),
        make_scalar_column<float>("integral"),
        make_scalar_column<float>("rms"),
        make_scalar_column<int>("tpc"),
        make_scalar_column<int>("global_plane"),
        make_scalar_column<int>("global_wire"),
        make_scalar_column<float>("global_time"),
        make_scalar_column<int>("local_plane"),
        make_scalar_column<int>("local_wire"),
        make_scalar_column<float>("local_time")
    ));

    fParticleNtuple = new hep_hpc::hdf5::Ntuple(
      make_ntuple({fFile, "particle_table", 1000},
        make_column<int>("event_id", 3),
        make_scalar_column<int>("g4_id"),
        make_scalar_column<int>("type"),
        make_scalar_column<int>("parent_id"),
        make_scalar_column<float>("momentum"),
        make_column<float>("start_position", 3),
        make_column<float>("end_position", 3),
        make_scalar_column<string>("start_process"),
        make_scalar_column<string>("end_process")
    ));

    fEnergyDepNtuple = new hep_hpc::hdf5::Ntuple(
      make_ntuple({fFile, "edep_table", 1000},
        make_column<int>("event_id", 3),
        make_scalar_column<int>("hit_id"),
        make_scalar_column<int>("g4_id"),
        make_scalar_column<float>("energy_fraction")
    ));
  }

  void HDF5Maker::endSubRun(art::SubRun const& sr) {
    delete fEventNtuple;
    delete fSpacePointNtuple;
    delete fHitNtuple;
    delete fParticleNtuple;
    delete fEnergyDepNtuple;
    fFile.close();
  }

  DEFINE_ART_MODULE(HDF5Maker)

} // namespace numl

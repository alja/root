/// \file
/// \ingroup tutorial_eve7
///  This example display geometry, tracks and hits in web browser
///
/// \macro_code
///

#include <vector>
#include <string>
#include <iostream>

#include "TClass.h"
#include "TRandom.h"
#include "TGeoTube.h"
#include "TGeoSphere.h"
#include "TParticle.h"
#include "TApplication.h"
#include "TMatrixDSym.h"
#include "TVector.h"
#include "TMatrixDEigen.h"

#include <ROOT/REveGeoShape.hxx>
#include <ROOT/REveScene.hxx>
#include <ROOT/REveViewer.hxx>
#include <ROOT/REveElement.hxx>
#include <ROOT/REveManager.hxx>
#include <ROOT/REveUtil.hxx>
#include <ROOT/REveGeoShape.hxx>
#include <ROOT/REveProjectionManager.hxx>
#include <ROOT/REveProjectionBases.hxx>
#include <ROOT/REvePointSet.hxx>
#include <ROOT/REveJetCone.hxx>
#include <ROOT/REveTrans.hxx>

#include <ROOT/REveTrack.hxx>
#include <ROOT/REveTrackPropagator.hxx>

using namespace ROOT::Experimental;

// globals
REveManager *eveMng = nullptr;
REveProjectionManager *mngRhoPhi = nullptr;
REveProjectionManager *mngRhoZ   = nullptr;
REveScene  *rPhiGeomScene = nullptr, *rPhiEventScene = nullptr;
REveScene  *rhoZGeomScene = nullptr, *rhoZEventScene = nullptr;

const Double_t kR_min = 240;
const Double_t kR_max = 250;
const Double_t kZ_d   = 300;


REvePointSet *getPointSet(int npoints = 2, float s=2, int color=28)
{
   TRandom &r = *gRandom;

   auto ps = new REvePointSet("fu", "", npoints);

   for (Int_t i=0; i<npoints; ++i)
       ps->SetNextPoint(r.Uniform(-s,s), r.Uniform(-s,s), r.Uniform(-s,s));

   ps->SetMarkerColor(color);
   ps->SetMarkerSize(3+r.Uniform(1, 7));
   ps->SetMarkerStyle(4);
   return ps;
}

void addPoints()
{
   REveElement* event = eveMng->GetEventScene();

   auto pntHolder = new REveElement("Hits");

   auto ps1 = getPointSet(20, 100);
   ps1->SetName("Points_1");
   ps1->SetTitle("Points_1 title"); // used as tooltip

   pntHolder->AddElement(ps1);

   auto ps2 = getPointSet(10, 200, 4);
   ps2->SetName("Points_2");
   ps2->SetTitle("Points_2 title"); // used as tooltip
   pntHolder->AddElement(ps2);

   event->AddElement(pntHolder);
}

void addTracks()
{
   TRandom &r = *gRandom;

   REveElement* event = eveMng->GetEventScene();
   auto prop = new REveTrackPropagator();
   prop->SetMagFieldObj(new REveMagFieldDuo(350, 3.5, -2.0));
   prop->SetMaxR(300);
   prop->SetMaxZ(600);
   prop->SetMaxOrbs(6);

   auto trackHolder = new REveElement("Tracks");

   double v = 0.2;
   double m = 5;

   int N_Tracks = 10 + r.Integer(20);
   for (int i = 0; i < N_Tracks; i++)
   {
      TParticle* p = new TParticle();

      int pdg = 11 * (r.Integer(2) > 0 ? 1 : -1);
      p->SetPdgCode(pdg);

      p->SetProductionVertex(r.Uniform(-v,v), r.Uniform(-v,v), r.Uniform(-v,v), 1);
      p->SetMomentum(r.Uniform(-m,m), r.Uniform(-m,m), r.Uniform(-m,m)*r.Uniform(1, 3), 1);
      auto track = new REveTrack(p, 1, prop);
      track->MakeTrack();
      if (i % 4 == 3) track->SetLineStyle(2); // enabled dashed style for some tracks
      track->SetMainColor(kBlue);
      track->SetName(Form("RandomTrack_%d", i));
      track->SetTitle(Form("RandomTrack_%d title", i)); // used as tooltip
      trackHolder->AddElement(track);
   }

   event->AddElement(trackHolder);
}

void addJets()
{
   TRandom &r = *gRandom;

   REveElement *event = eveMng->GetEventScene();
   auto jetHolder = new REveElement("Jets");

   int N_Jets = 5 + r.Integer(5);
   for (int i = 0; i < N_Jets; i++)
   {
      auto jet = new REveJetCone(Form("Jet_%d", i));
      jet->SetTitle(Form("Jet_%d title", i)); // used as tooltip
      jet->SetCylinder(2*kR_max, 2*kZ_d);
      jet->AddEllipticCone(r.Uniform(-3.5, 3.5), r.Uniform(0, TMath::TwoPi()),
                           r.Uniform(0.02, 0.2), r.Uniform(0.02, 0.3));
      jet->SetFillColor(kPink - 8);
      jet->SetLineColor(kViolet - 7);

      jetHolder->AddElement(jet);
   }
   event->AddElement(jetHolder);
}

void makeEventScene()
{
   addPoints();
   addTracks();
   addJets();
}

void makeGeometryScene()
{
   auto b1 = new REveGeoShape("Barrel 1");
   b1->SetShape(new TGeoTube(kR_min, kR_max, kZ_d));
   b1->SetMainColor(kCyan);
   eveMng->GetGlobalScene()->AddElement(b1);

   // Debug of surface fill in RPhi (index buffer screwed).
   b1->SetNSegments(40);
}

void projectScenes()
{
   // project RhoPhi
   rPhiGeomScene  = eveMng->SpawnNewScene("RPhi Geometry","RPhi");
   rPhiEventScene = eveMng->SpawnNewScene("RPhi Event Data","RPhi");

   mngRhoPhi = new REveProjectionManager(REveProjection::kPT_RPhi);

   REveViewer *rphiView = eveMng->SpawnNewViewer("RPhi View", "");
   rphiView->AddScene(rPhiGeomScene);
   rphiView->AddScene(rPhiEventScene);

   // ----------------------------------------------------------------

   rhoZGeomScene  = eveMng->SpawnNewScene("RhoZ Geometry", "RhoZ");
   rhoZEventScene = eveMng->SpawnNewScene("RhoZ Event Data","RhoZ");

   mngRhoZ = new REveProjectionManager(REveProjection::kPT_RhoZ);

   REveViewer *rhoZView = eveMng->SpawnNewViewer("RhoZ View", "");
   rhoZView->AddScene(rhoZGeomScene);
   rhoZView->AddScene(rhoZEventScene);
   for (auto &ie : eveMng->GetGlobalScene()->RefChildren())
   {
      mngRhoPhi->ImportElements(ie, rPhiGeomScene);
      mngRhoZ  ->ImportElements(ie, rhoZGeomScene);
   }
   for (auto &ie : eveMng->GetEventScene()->RefChildren())
   {
      mngRhoPhi->ImportElements(ie, rPhiEventScene);
      mngRhoZ  ->ImportElements(ie, rhoZEventScene);
   }
}

//==============================================================================

class EventManager : public REveElement
{
public:
   EventManager() = default;

   virtual ~EventManager() {}

   virtual void NextEvent()
   {
      eveMng->DisableRedraw();
      auto scene =  eveMng->GetEventScene();
      scene->DestroyElements();
      makeEventScene();
      for (auto &ie : scene->RefChildren())
      {
         if (mngRhoPhi)
         mngRhoPhi->ImportElements(ie, rPhiEventScene);
         if (mngRhoZ)
         mngRhoZ  ->ImportElements(ie, rhoZEventScene);
      }
      eveMng->EnableRedraw();
      eveMng->DoRedraw3D();
   }

   virtual void QuitRoot()
   {
      printf("Quit ROOT\n");
      if (gApplication) gApplication->Terminate();
   }

};

void event_demo()
{
   // disable browser cache - all scripts and html files will be loaded every time, useful for development
   // gEnv->SetValue("WebGui.HttpMaxAge", 0);

   gRandom->SetSeed(0); // make random seed

   eveMng = REveManager::Create();

   auto eventMng = new EventManager();
   eventMng->SetName("EventManager");
   eveMng->GetWorld()->AddElement(eventMng);

   eveMng->GetWorld()->AddCommand("QuitRoot", "sap-icon://log", eventMng, "QuitRoot()");

   eveMng->GetWorld()->AddCommand("NextEvent", "sap-icon://step", eventMng, "NextEvent()");

   makeGeometryScene();
   makeEventScene();
   projectScenes();

   eveMng->Show();
}

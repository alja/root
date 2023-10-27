/// \file
/// \ingroup tutorial_eve7
///  This example display only texts in web browser
///
/// \Waad
///

#include "TRandom.h"
#include <ROOT/REveElement.hxx>
#include <ROOT/REveScene.hxx>
#include <ROOT/REveManager.hxx>
#include <ROOT/REveZText.hxx>
#include <ROOT/REveJetCone.hxx>

namespace REX = ROOT::Experimental;

using namespace ROOT::Experimental;
void makeTexts(int N_Texts, REX::REveElement *textHolder)
{
   TRandom &r = *gRandom;

   for (int i = 0; i < N_Texts; i++)
   {
      auto text = new REX::REveZText(Form("Text_%d",i ));
      text -> SetFontColor(kViolet - r.Uniform(0, 50));
      REX::REveVector pos(r.Uniform(10, 4500), r.Uniform(150, 3500), 1);
      text -> SetPosition(pos);
      text -> SetFontSize(r.Uniform(50, 400));
      text -> SetFont(r.Uniform(1, 6));
      text -> SetText(Form("Text_%d",i ));
      textHolder->AddElement(text);
   }
}
void makeJets(int N_Jets, REveElement *jetHolder)
{
   TRandom &r = *gRandom;

   const Double_t kR_min = 240;
   const Double_t kR_max = 250;
   const Double_t kZ_d   = 300;
   for (int i = 0; i < N_Jets; i++)
   {
      auto jet = new REveJetCone(Form("Jet_%d",i ));
      jet->SetCylinder(2*kR_max, 2*kZ_d);
      jet->AddEllipticCone(r.Uniform(-0.5, 0.5), r.Uniform(0, TMath::TwoPi()),
                           0.1, 0.2);
      jet->SetFillColor(kRed);
      jet->SetLineColor(kRed);

      jetHolder->AddElement(jet);
   }
}

void texts()
{
   auto eveMng = REX::REveManager::Create();

   //add box to overlay
   REX::REveScene* os = eveMng->SpawnNewScene("OverlyScene", "OverlayTitle");
   ((REveViewer*)(eveMng->GetViewers()->FirstChild()))->AddScene(os);
   os->SetIsOverlay(true);

   REX::REveElement *textHolder = new REX::REveElement("texts");
   makeTexts(30, textHolder);
   os->AddElement(textHolder);

   auto jetHolder = new REveElement("jets");
   makeJets(2,jetHolder);
   eveMng->GetEventScene()->AddElement(jetHolder);

   eveMng->Show();
}

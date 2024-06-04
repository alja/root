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
#include <ROOT/REveText.hxx>
#include <ROOT/REveJetCone.hxx>

namespace REX = ROOT::Experimental;

using namespace ROOT::Experimental;

void makeTexts(int N_Texts, REX::REveElement *textHolder)
{
   const char *bla[] = { "Love", "Peace", "ROOT", "Code" };
   const double pi = TMath::Pi();
   const double lim = 300;

   TRandom &r = *gRandom;

   for (int i = 0; i < N_Texts; i++)
   {
      auto name_text = Form("%s_%d", bla[r.Integer(sizeof(bla)/sizeof(char*))], i);
      auto text = new REX::REveText(name_text);
      text->SetText(name_text);
      // text->SetFontColor(kViolet - r.Uniform(0, 50));
      int mode = r.Integer(2);
      text->SetMode(mode);
      if (mode == 0) { // world
         auto &t = text->RefMainTrans();
         t.SetRotByAngles(r.Uniform(-pi, pi), r.Uniform(-pi, pi), r.Uniform(-pi, pi));
         t.SetPos(r.Uniform(-lim, lim), r.Uniform(-lim, lim), r.Uniform(-lim, lim));
         text->SetFontSize(r.Uniform(0.01*lim, 0.2*lim));
      } else { // screen [0, 0] bottom left, [1, 1] top-right corner, font-size in y-units, x scaled with the window aspect ratio.
         text->SetPosition(REX::REveVector(r.Uniform(-0.1, 0.9), r.Uniform(0.1, 1.1), r.Uniform(0.0, 1.0)));
         text->SetFontSize(r.Uniform(0.001, 0.05));
      }
      text->SetFont(10 * r.Uniform(0, 3) + r.Uniform(0, 4));
      text->SetTextColor(TColor::GetColor((float) r.Uniform(0, 0.5), (float) r.Uniform(0, 0.5), (float) r.Uniform(0, 0.5)));
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
   eveMng->AllowMultipleRemoteConnections(false, false);

   //add box to overlay
   REX::REveScene* os = eveMng->SpawnNewScene("OverlyScene", "OverlayTitle");
   ((REveViewer*)(eveMng->GetViewers()->FirstChild()))->AddScene(os);
   os->SetIsOverlay(true);

   REX::REveElement *textHolder = new REX::REveElement("texts");
   makeTexts(200, textHolder);
   // os->AddElement(textHolder);
   eveMng->GetEventScene()->AddElement(textHolder);

   auto jetHolder = new REveElement("jets");
   makeJets(2,jetHolder);
   eveMng->GetEventScene()->AddElement(jetHolder);

   eveMng->Show();
}

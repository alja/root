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

namespace REX = ROOT::Experimental;


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

void texts()
{
   auto eveMng = REX::REveManager::Create();

   //add box to overlay
   REX::REveScene* os = eveMng->SpawnNewScene("OverlyScene", "OverlayTitle");
   os->SetIsOverlay(true);

   REX::REveElement *textHolder = new REX::REveElement("texts");
   makeTexts(30, textHolder);
   os->AddElement(textHolder);

   eveMng->Show();
}
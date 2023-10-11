// @(#)root/eve7:$Id$
// Author: Waad Alshehri, 2023

/*************************************************************************
 * Copyright (C) 1995-2019, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/


#include <ROOT/REveText.hxx>
#include <ROOT/REveTrans.hxx>
#include <ROOT/REveRenderData.hxx>

#include "TMath.h"
#include "TClass.h"

#include <cassert>

#include <nlohmann/json.hpp>

using namespace ROOT::Experimental;

////////////////////////////////////////////////////////////////////////////////
/// Constructor.

REveText::REveText(const Text_t* n, const Text_t* t) :
   REveShape(n, t)
{
   // MainColor set to FillColor in Shape.
   fPickable  = true;
   fLineWidth = 0.05; // override, in text-size units
}

////////////////////////////////////////////////////////////////////////////////
/// Fill core part of JSON representation.

Int_t REveText::WriteCoreJson(nlohmann::json &j, Int_t rnr_offset)
{
   Int_t ret = REveShape::WriteCoreJson(j, rnr_offset);

   j["fText"] = fText;
   j["fFont"] = fFont;
   j["fPosX"] = fPosition.fX;
   j["fPosY"] = fPosition.fY;
   j["fPosZ"] = fPosition.fZ;
   j["fFontSize"] = fFontSize;
   j["fFontHinting"] = fFontHinting;
   j["fExtraBorder"] = fExtraBorder;
   j["fMode"] = fMode;
   j["fTextColor"] = fTextColor;

   return ret;
}

////////////////////////////////////////////////////////////////////////////////
/// Crates 3D point array for rendering.

void REveText::BuildRenderData()
{
   fRenderData = std::make_unique<REveRenderData>("makeZText");
   REveShape::BuildRenderData();
   // TODO write fPosition and fFontSize here ...
   fRenderData->PushV(0.f, 0.f, 0.f); // write floats so the data is not empty
}

////////////////////////////////////////////////////////////////////////////////
/// Compute bounding-box of the data.

void REveText::ComputeBBox()
{
   //BBoxInit();
}

////////////////////////////////////////////////////////////////////////////////
/// Check if font exists, otherwise try to create it.
/// Returns true if font files are present, false otherwise.
/// Static function.

#include "TSystem.h"
#include "TROOT.h"
#include "TEnv.h"

bool REveText::AssertSdfFont(std::string_view font_name, std::string_view ttf_font)
{
   std::string base( gEnv->GetValue("WebGui.RootUi5Path", gSystem->ExpandPathName("${ROOTSYS}/ui5")) );
   base += "/eve7/fonts/";
   base += font_name;
   std::string png = base + ".png";
   std::string js  = base + ".js.gz";

   if (gSystem->AccessPathName(png.data()) || gSystem->AccessPathName(js.data())) {
      if (gSystem->AccessPathName(ttf_font.data())) {
         ::Warning("REveText::AssertSdfFont", "Source TTF font '%s' not found.", ttf_font.data());
         return false;
      }
      // Invoke through interpreter to avoid REve dependece on RGL.
      char command[8192];
      snprintf(command, 8192, "TGLSdfFontMaker::MakeFont(\"%s\", \"%s\")",
               ttf_font.data(), base.data());
      gROOT->ProcessLine(command);
      if (gSystem->AccessPathName(png.data()) || gSystem->AccessPathName(js.data())) {
         ::Warning("REveText::AssertSdfFont", "Creation failed.");
         return false;
      }
   }
   return true;
}

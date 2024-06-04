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
   fFillColor = kGreen;
}

////////////////////////////////////////////////////////////////////////////////
/// Fill core part of JSON representation.

Int_t REveText::WriteCoreJson(nlohmann::json &t, Int_t rnr_offset)
{
   Int_t ret = REveElement::WriteCoreJson(t, rnr_offset);

   t["fText"] = fText;
   t["fPosX"] = fPosition.fX;
   t["fPosY"] = fPosition.fY;
   t["fPosZ"] = fPosition.fZ;
   t["fFontSize"] = fFontSize;
   t["fFontHinting"] = fFontHinting;
   t["fMode"] = fMode;
   t["fFont"] = fFont;
   t["fTextColor"] = fTextColor;

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

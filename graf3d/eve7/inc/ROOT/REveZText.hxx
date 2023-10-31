// @(#)root/eve7:$Id$
// Author: Waad Alshehri, 2023

/*************************************************************************
 * Copyright (C) 1995-2019, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT7_REveZText
#define ROOT7_REveZText

#include <ROOT/REveShape.hxx>
#include <ROOT/REveVector.hxx>

namespace ROOT {
namespace Experimental {

//------------------------------------------------------------------------------
// REveZText
//------------------------------------------------------------------------------

class REveZText : public REveShape
{

private:
   REveZText(const REveZText &) = delete;
   REveZText &operator=(const REveZText &) = delete;

protected:
   std::string fText{"INITIALIZE"};
   Int_t fFontSize{80};
   Float_t fFontHinting{1.0};
   Int_t fMode{1};
   REveVector fPosition{10, 1600,1};
   Color_t fFontColor{kMagenta};
   Int_t fFont{1};

public:
   REveZText(const Text_t *n = "REveZText", const Text_t *t = "");
   virtual ~REveZText() {}

    Int_t WriteCoreJson(nlohmann::json &t, Int_t rnr_offset) override;
   void BuildRenderData() override;

   void ComputeBBox() override;

   Int_t GetFontSize() const { return fFontSize; }
   void SetFontSize(Int_t size) { fFontSize = size; }

   Int_t GetMode() const { return fMode; }
   void SetMode(Int_t mode) { fMode = mode;}

   Float_t GetFontHinting() const { return fFontHinting; }
   void SetFontHinting(Float_t fontHinting) { fFontHinting = fontHinting; }

   REveVector GetPosition() const { return fPosition; }
   void SetPosition(REveVector& position) { fPosition = position; }

   Color_t GetFontColor() const { return fFontColor; }
   void SetFontColor(Color_t color) { fFontColor = color; }

   std::string GetText() const { return fText; }
   void SetText(const std::string &text) { fText = text; }

   Int_t GetFont() const { return fFont; }
   void SetFont(Int_t font) { fFont = font;}



};

}// namespace Experimental
} // namespace ROOT

#endif

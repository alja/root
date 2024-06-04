// @(#)root/eve7:$Id$
// Author: Waad Alshehri, 2023

/*************************************************************************
 * Copyright (C) 1995-2019, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT7_REveText
#define ROOT7_REveText

#include <ROOT/REveShape.hxx>
#include <ROOT/REveVector.hxx>

namespace ROOT {
namespace Experimental {

//------------------------------------------------------------------------------
// REveText
//------------------------------------------------------------------------------

class REveText : public REveShape
{
private:
   REveText(const REveText &) = delete;
   REveText &operator=(const REveText &) = delete;

protected:
   std::string fText {"<no-text>"};
   REveVector  fPosition {0, 0, 0};
   Float_t     fFontSize {80};
   Float_t     fFontHinting {1.0};
   Int_t       fMode {1}; // default mode is in relative screen coordinates [0,1]
   Int_t       fFont {1};
   Color_t     fTextColor {kMagenta};

public:
   REveText(const Text_t *n = "REveText", const Text_t *t = "");
   virtual ~REveText() {}

    Int_t WriteCoreJson(nlohmann::json &t, Int_t rnr_offset) override;
   void BuildRenderData() override;

   void ComputeBBox() override;

   Float_t GetFontSize() const { return fFontSize; }
   void SetFontSize(float size) { fFontSize = size; StampObjProps();}

   Int_t GetMode() const { return fMode; }
   void SetMode(Int_t mode) { fMode = mode;}

   Float_t GetFontHinting() const { return fFontHinting; }
   void SetFontHinting(Float_t fontHinting) { fFontHinting = fontHinting; StampObjProps();}

   REveVector GetPosition() const { return fPosition; }
   REveVector& RefPosition() { return fPosition; }
   void SetPosition(const REveVector& position) { fPosition = position;}
   void SetPosX(float x) { fPosition.fX = x; StampObjProps(); }
   void SetPosY(float y) { fPosition.fY = y; StampObjProps(); }

   Color_t GetTextColor() const { return fTextColor; }
   void SetTextColor(Color_t color) { fTextColor = color; StampObjProps();}

   std::string GetText() const { return fText; }
   void SetText(const char* text) { fText = text; StampObjProps(); }

   Int_t GetFont() const { return fFont; }
   void SetFont(Int_t font) { fFont = font; StampObjProps();}
};

}// namespace Experimental
} // namespace ROOT

#endif

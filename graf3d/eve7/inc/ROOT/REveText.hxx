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
   std::string fText{"INITIALIZE"};
   Float_t fFontSize{80};
   Float_t fFontHinting{1.0};
   Int_t fMode{1}; // default mode is in relative screen coordinates [0,1]
   REveVector fPosition{10, 1600,1};
   Color_t fFontColor{kMagenta};
   Int_t fFont{1};
   Int_t fScreenWidth{1425};
   Int_t fScreenHeight{822};

public:
   REveText(const Text_t *n = "REveText", const Text_t *t = "");
   virtual ~REveText() {}

    Int_t WriteCoreJson(nlohmann::json &t, Int_t rnr_offset) override;
   void BuildRenderData() override;

   void ComputeBBox() override;

   Float_t GetFontSize() const { return fFontSize; }
   void SetFontSize(double);// { fFontSize = size; StampObjProps();}

   Int_t GetMode() const { return fMode; }
   void SetMode(Int_t mode) { fMode = mode;}

   Float_t GetFontHinting() const { return fFontHinting; }
   void SetFontHinting(Float_t fontHinting) { fFontHinting = fontHinting; StampObjProps();}

   REveVector GetPosition() const { return fPosition; }
   void SetPosition(REveVector& position) { fPosition = position;}
   void SetPosX(double x);
   void SetPosY(double y);

   Color_t GetFontColor() const { return fFontColor; }
   void SetFontColor(Color_t color) { fFontColor = color; StampObjProps();}

   std::string GetText() const { return fText; }
   void SetText(const char* text) { fText = text; StampObjProps(); }

   Int_t GetFont() const { return fFont; }
   void SetFont(Int_t font) { fFont = font; StampObjProps();}

   Int_t GetScreenWidth() const { return fScreenWidth; }
   void SetScreenWidth(Int_t screenW) { fScreenWidth = screenW;  StampObjProps();}

   Int_t GetScreenHeight() const { return fScreenHeight; }
   void SetScreenHeight(Int_t screenH) { fScreenHeight = screenH; StampObjProps();}
};

}// namespace Experimental
} // namespace ROOT

#endif

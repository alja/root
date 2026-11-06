// @(#)root/eve7:$Id$
// Authors: Matevz Tadel & Alja Mrak-Tadel: 2026

/*************************************************************************
 * Copyright (C) 1995-2026, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT7_REveWindowManager
#define ROOT7_REveWindowManager

#include <ROOT/REveElement.hxx>

#include <memory>
#include <vector>

namespace ROOT {
namespace Experimental {

class REveViewer;
class REveWindowManager;

////////////////////////////////////////////////////////////////////////////////
/// REveWindowPack
/// Container arranging viewers and sub-packs either horizontally or vertically.
////////////////////////////////////////////////////////////////////////////////

class REveWindowPack
{
   friend class REveWindowManager;

public:
   enum EOrientation { kHorizontal, kVertical };

private:
   struct Item {
      REveViewer *fViewer{nullptr};
      std::unique_ptr<REveWindowPack> fPack;
      float fWeight{1};
   };

   REveWindowPack(const REveWindowPack &) = delete;
   REveWindowPack &operator=(const REveWindowPack &) = delete;

   REveWindowManager *fManager{nullptr}; //!
   EOrientation fOrientation{kHorizontal};
   std::vector<Item> fItems;             //!

   void LayoutChanged();
   bool RemoveViewerRecursively(REveViewer *v);
   void WriteJson(nlohmann::json &j) const;

public:
   REveWindowPack(REveWindowManager *m, EOrientation o) : fManager(m), fOrientation(o) {}

   REveWindowPack *NewPack(EOrientation o, float weight = 1);
   REveWindowPack *NewHorizontalPack(float weight = 1) { return NewPack(kHorizontal, weight); }
   REveWindowPack *NewVerticalPack(float weight = 1) { return NewPack(kVertical, weight); }

   void AddViewer(REveViewer *v, float weight = 1);

   void SetOrientation(EOrientation o);
   void SetVertical(bool x = true) { SetOrientation(x ? kVertical : kHorizontal); }
   void SetHorizontal() { SetOrientation(kHorizontal); }

   EOrientation GetOrientation() const { return fOrientation; }
   bool IsVertical() const { return fOrientation == kVertical; }

   size_t GetNItems() const { return fItems.size(); }
   bool IsEmpty() const { return fItems.empty(); }

   void Clear();
};

////////////////////////////////////////////////////////////////////////////////
/// REveWindowManager
/// Describes how viewers are nested in the main area of the web client.
/// The summary view and the menu bar are not part of the layout.
/// If the main pack is empty, the client uses its default layout.
////////////////////////////////////////////////////////////////////////////////

class REveWindowManager : public REveElement
{
private:
   REveWindowManager(const REveWindowManager &) = delete;
   REveWindowManager &operator=(const REveWindowManager &) = delete;

   std::unique_ptr<REveWindowPack> fMainPack; //!

public:
   REveWindowManager(const std::string &n = "WindowManager", const std::string &t = "");
   ~REveWindowManager() override;

   REveWindowPack *GetMainPack() const { return fMainPack.get(); }

   void ResetLayout();
   void RemoveViewer(REveViewer *v);

   Int_t WriteCoreJson(nlohmann::json &j, Int_t rnr_offset) override;
};

} // namespace Experimental
} // namespace ROOT

#endif

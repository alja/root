// @(#)root/eve7:$Id$
// Authors: Matevz Tadel & Alja Mrak-Tadel: 2026

/*************************************************************************
 * Copyright (C) 1995-2026, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include <ROOT/REveWindowManager.hxx>
#include <ROOT/REveViewer.hxx>

#include <nlohmann/json.hpp>

using namespace ROOT::Experimental;

/** \class REveWindowPack
\ingroup REve
Container of viewers and sub-packs, arranged horizontally or vertically.
Each item has a weight that determines its relative size in the pack.

Example:
~~~ {.cpp}
   auto pack = eveMng->GetWindowManager()->GetMainPack();  // horizontal
   pack->AddViewer(view3D, 2);
   auto column = pack->NewVerticalPack(1);
   column->AddViewer(rphiView);
   column->AddViewer(rhoZView);
~~~
*/

////////////////////////////////////////////////////////////////////////////////
/// Notify clients that the layout has changed.

void REveWindowPack::LayoutChanged()
{
   fManager->StampObjProps();
}

////////////////////////////////////////////////////////////////////////////////
/// Create a new sub-pack with given orientation and append it to this pack.

REveWindowPack *REveWindowPack::NewPack(EOrientation o, float weight)
{
   Item item;
   item.fPack = std::make_unique<REveWindowPack>(fManager, o);
   item.fWeight = weight;
   fItems.push_back(std::move(item));
   LayoutChanged();
   return fItems.back().fPack.get();
}

////////////////////////////////////////////////////////////////////////////////
/// Append viewer to this pack. A viewer can be shown only once, so it is
/// removed from its previous position in the layout, if any.

void REveWindowPack::AddViewer(REveViewer *v, float weight)
{
   static const REveException eh("REveWindowPack::AddViewer ");

   if (!v)
      throw eh + "viewer is null.";

   fManager->GetMainPack()->RemoveViewerRecursively(v);

   Item item;
   item.fViewer = v;
   item.fWeight = weight;
   fItems.push_back(std::move(item));
   LayoutChanged();
}

////////////////////////////////////////////////////////////////////////////////
/// Set orientation of the pack.

void REveWindowPack::SetOrientation(EOrientation o)
{
   if (o == fOrientation)
      return;
   fOrientation = o;
   LayoutChanged();
}

////////////////////////////////////////////////////////////////////////////////
/// Remove all items from the pack.

void REveWindowPack::Clear()
{
   fItems.clear();
   LayoutChanged();
}

////////////////////////////////////////////////////////////////////////////////
/// Remove viewer from this pack and its sub-packs. Returns true if found.

bool REveWindowPack::RemoveViewerRecursively(REveViewer *v)
{
   for (auto i = fItems.begin(); i != fItems.end(); ++i) {
      if (i->fViewer == v) {
         fItems.erase(i);
         return true;
      }
      if (i->fPack && i->fPack->RemoveViewerRecursively(v))
         return true;
   }
   return false;
}

////////////////////////////////////////////////////////////////////////////////
/// Write pack description, including sub-packs.

void REveWindowPack::WriteJson(nlohmann::json &j) const
{
   j["Vertical"] = IsVertical();
   j["Items"] = nlohmann::json::array();
   for (auto &item : fItems) {
      nlohmann::json ji;
      ji["Weight"] = item.fWeight;
      if (item.fViewer) {
         ji["ViewerId"] = item.fViewer->GetElementId();
      } else {
         item.fPack->WriteJson(ji["Pack"]);
      }
      j["Items"].push_back(ji);
   }
}

/** \class REveWindowManager
\ingroup REve
Describes nesting of viewers in the main area of the web client, to the right
of the summary view. The layout is a tree of REveWindowPack objects, the
top-level one is the main pack.

Viewers that are not placed in the layout are appended to the main pack on the
client. If the main pack is empty, the client uses its default layout: the first
viewer on the left and the rest stacked vertically on the right.
*/

////////////////////////////////////////////////////////////////////////////////
/// Constructor.

REveWindowManager::REveWindowManager(const std::string &n, const std::string &t)
   : REveElement(n, t), fMainPack(std::make_unique<REveWindowPack>(this, REveWindowPack::kHorizontal))
{
}

////////////////////////////////////////////////////////////////////////////////
/// Destructor.

REveWindowManager::~REveWindowManager() {}

////////////////////////////////////////////////////////////////////////////////
/// Clear the layout, client falls back to the default one.

void REveWindowManager::ResetLayout()
{
   fMainPack = std::make_unique<REveWindowPack>(this, REveWindowPack::kHorizontal);
   StampObjProps();
}

////////////////////////////////////////////////////////////////////////////////
/// Remove viewer from the layout. Called from viewer destructor.

void REveWindowManager::RemoveViewer(REveViewer *v)
{
   if (fMainPack->RemoveViewerRecursively(v))
      StampObjProps();
}

////////////////////////////////////////////////////////////////////////////////
/// Stream the layout.
/// Virtual from REveElement.

Int_t REveWindowManager::WriteCoreJson(nlohmann::json &j, Int_t rnr_offset)
{
   fMainPack->WriteJson(j["Layout"]);
   j["UT_PostStream"] = "UT_WindowManagerUpdate";

   return REveElement::WriteCoreJson(j, rnr_offset);
}

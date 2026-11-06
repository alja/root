/// \file
/// \ingroup tutorial_eve_7
///  This example shows several nested viewer layouts made with REveWindowManager.
///  The layout is chosen with the macro argument ("grid", "spiral", "dashboard",
///  "columns") and can be cycled at run time with the "NextLayout" command.
///
///  Usage: root 'nested_layouts.C("spiral")'
///
/// \macro_code
///
/// \author Alja Mrak-Tadel

#include "TRandom.h"

#include <ROOT/REveManager.hxx>
#include <ROOT/REveScene.hxx>
#include <ROOT/REveViewer.hxx>
#include <ROOT/REveWindowManager.hxx>
#include <ROOT/REvePointSet.hxx>
#include <ROOT/REveStraightLineSet.hxx>
#include <ROOT/REveProjectionManager.hxx>

namespace REX = ROOT::Experimental;

REX::REveManager *eveMng = nullptr;

// viewers, in order: 3D, RPhi, RhoZ, Front, Side, Top
std::vector<REX::REveViewer *> views;

//==============================================================================
// Content
//==============================================================================

REX::REvePointSet *makePoints(int npoints, float s, int color)
{
   TRandom &r = *gRandom;
   auto ps = new REX::REvePointSet("Hits", "", npoints);
   for (int i = 0; i < npoints; ++i)
      ps->SetNextPoint(r.Uniform(-s, s), r.Uniform(-s, s), r.Uniform(-2 * s, 2 * s));
   ps->SetMarkerColor(color);
   ps->SetMarkerSize(4);
   return ps;
}

REX::REveStraightLineSet *makeFrame(float a, float d, int color)
{
   auto ls = new REX::REveStraightLineSet("Frame");
   float x[2] = {-a, a}, z[2] = {-d, d};
   for (int i = 0; i < 2; ++i)
      for (int j = 0; j < 2; ++j) {
         ls->AddLine(x[i], -a, z[j], x[i], a, z[j]);
         ls->AddLine(-a, x[i], z[j], a, x[i], z[j]);
         ls->AddLine(x[i], x[j], -d, x[i], x[j], d);
      }
   ls->SetMainColor(color);
   return ls;
}

REX::REveViewer *makeView(const char *name, REX::REveCamera::ECameraType cam)
{
   auto v = eveMng->SpawnNewViewer(name);
   v->AddScene(eveMng->GetGlobalScene());
   v->AddScene(eveMng->GetEventScene());
   v->SetCameraType(cam);
   return v;
}

REX::REveViewer *makeProjectedView(const char *name, REX::REveProjection::EPType_e type)
{
   auto geomScene = eveMng->SpawnNewScene(Form("%s Geom", name));
   auto eventScene = eveMng->SpawnNewScene(Form("%s Event", name));

   auto mng = new REX::REveProjectionManager(type);
   for (auto &ie : eveMng->GetGlobalScene()->RefChildren())
      mng->ImportElements(ie, geomScene);
   for (auto &ie : eveMng->GetEventScene()->RefChildren())
      mng->ImportElements(ie, eventScene);

   auto v = eveMng->SpawnNewViewer(name);
   v->AddScene(geomScene);
   v->AddScene(eventScene);
   v->SetCameraType(REX::REveViewer::kCameraOrthoXOY);
   return v;
}

//==============================================================================
// Layouts
//==============================================================================

// Two rows of three viewers: a vertical pack of horizontal packs.
//
//   +------+------+------+
//   |  3D  | RPhi | RhoZ |
//   +------+------+------+
//   | Front| Side | Top  |
//   +------+------+------+
void gridLayout(REX::REveWindowPack *main)
{
   main->SetVertical();
   for (int r = 0; r < 2; ++r) {
      auto row = main->NewHorizontalPack();
      for (int c = 0; c < 3; ++c)
         row->AddViewer(views[3 * r + c]);
   }
}

// Each pack splits off one viewer and nests the rest in a pack of the
// other orientation, five levels deep.
//
//   +-----------+-----------+
//   |           |   RPhi    |
//   |           +-----+-----+
//   |    3D     |     |Front|
//   |           |RhoZ +--+--+
//   |           |     |Si|To|
//   +-----------+-----+--+--+
void spiralLayout(REX::REveWindowPack *main)
{
   auto pack = main; // horizontal
   for (size_t i = 0; i < views.size(); ++i) {
      bool last = (i == views.size() - 1);
      pack->AddViewer(views[i]);
      if (!last && i < views.size() - 2)
         pack = pack->IsVertical() ? pack->NewHorizontalPack() : pack->NewVerticalPack();
   }
}

// Large 3D view on top, below it projections in a column next to a
// row of orthographic views.
//
//   +-----------------------+
//   |          3D           |
//   +------+----------------+
//   | RPhi |                |
//   +------+ Front|Side|Top |
//   | RhoZ |                |
//   +------+----------------+
void dashboardLayout(REX::REveWindowPack *main)
{
   main->SetVertical();
   main->AddViewer(views[0], 2);
   auto bottom = main->NewHorizontalPack(1);
   auto proj = bottom->NewVerticalPack(1);
   proj->AddViewer(views[1]);
   proj->AddViewer(views[2]);
   auto ortho = bottom->NewHorizontalPack(3);
   for (int i = 3; i < 6; ++i)
      ortho->AddViewer(views[i]);
}

// Three columns with different splits.
//
//   +------+------+------+
//   |      | RPhi | Front|
//   |  3D  +------+------+
//   |      | RhoZ | Side |
//   |      |      +------+
//   |      |      | Top  |
//   +------+------+------+
void columnsLayout(REX::REveWindowPack *main)
{
   main->AddViewer(views[0], 1.5);
   auto c1 = main->NewVerticalPack();
   c1->AddViewer(views[1]);
   c1->AddViewer(views[2]);
   auto c2 = main->NewVerticalPack();
   for (int i = 3; i < 6; ++i)
      c2->AddViewer(views[i]);
}

std::vector<std::pair<std::string, void (*)(REX::REveWindowPack *)>> layouts = {
   {"grid", gridLayout}, {"spiral", spiralLayout}, {"dashboard", dashboardLayout}, {"columns", columnsLayout}};

void applyLayout(size_t idx)
{
   auto wm = eveMng->GetWindowManager();
   wm->ResetLayout();
   layouts[idx].second(wm->GetMainPack());
   printf("nested_layouts: using layout \"%s\"\n", layouts[idx].first.c_str());
}

class LayoutCycler : public REX::REveElement {
   size_t fIdx{0};

public:
   LayoutCycler(size_t idx) : REX::REveElement("LayoutCycler"), fIdx(idx) {}

   void NextLayout()
   {
      fIdx = (fIdx + 1) % layouts.size();
      applyLayout(fIdx);
   }
};

//==============================================================================

void nested_layouts(const char *layout = "dashboard")
{
   eveMng = REX::REveManager::Create();

   eveMng->GetGlobalScene()->AddElement(makeFrame(50, 100, kBlue));
   eveMng->GetEventScene()->AddElement(makePoints(150, 45, kRed));

   views.push_back(eveMng->GetDefaultViewer());
   views.push_back(makeProjectedView("RPhi", REX::REveProjection::kPT_RPhi));
   views.push_back(makeProjectedView("RhoZ", REX::REveProjection::kPT_RhoZ));
   views.push_back(makeView("Front", REX::REveCamera::kCameraOrthoXOY));
   views.push_back(makeView("Side", REX::REveCamera::kCameraOrthoZOY));
   views.push_back(makeView("Top", REX::REveCamera::kCameraOrthoZOX));

   size_t idx = 0;
   while (idx < layouts.size() && layouts[idx].first != layout)
      ++idx;
   if (idx == layouts.size()) {
      printf("nested_layouts: unknown layout \"%s\", using \"%s\"\n", layout, layouts[0].first.c_str());
      idx = 0;
   }
   applyLayout(idx);

   auto cycler = new LayoutCycler(idx);
   eveMng->GetWorld()->AddElement(cycler);
   eveMng->GetWorld()->AddCommand("NextLayout", "sap-icon://grid", cycler, "NextLayout()");

   eveMng->Show();
}

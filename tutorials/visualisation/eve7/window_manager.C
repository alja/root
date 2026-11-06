/// \file
/// \ingroup tutorial_eve_7
///  This example demonstrates nesting of viewers with REveWindowManager.
///  Viewers are arranged in horizontal and vertical packs to the right of
///  the summary view. Command "SwitchLayout" changes the layout at run time.
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
#include <ROOT/REveBox.hxx>
#include <ROOT/REveProjectionManager.hxx>

namespace REX = ROOT::Experimental;

REX::REveManager *eveMng = nullptr;
std::vector<REX::REveViewer *> viewers;

REX::REvePointSet *makePoints(int npoints, float s, int color)
{
   TRandom &r = *gRandom;
   auto ps = new REX::REvePointSet("Points", "", npoints);
   for (int i = 0; i < npoints; ++i)
      ps->SetNextPoint(r.Uniform(-s, s), r.Uniform(-s, s), r.Uniform(-2 * s, 2 * s));
   ps->SetMarkerColor(color);
   ps->SetMarkerSize(4);
   return ps;
}

REX::REveBox *makeBox(float a, float d, int color)
{
   auto b = new REX::REveBox("Box");
   b->SetMainColor(color);
   b->SetMainTransparency(50);
   b->SetVertex(0, -a, -a, -d);
   b->SetVertex(1, -a, a, -d);
   b->SetVertex(2, a, a, -d);
   b->SetVertex(3, a, -a, -d);
   b->SetVertex(4, -a, -a, d);
   b->SetVertex(5, -a, a, d);
   b->SetVertex(6, a, a, d);
   b->SetVertex(7, a, -a, d);
   return b;
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

   auto view = eveMng->SpawnNewViewer(name);
   view->AddScene(geomScene);
   view->AddScene(eventScene);
   view->SetCameraType(REX::REveViewer::kCameraOrthoXOY);
   return view;
}

// 3D view on the left, on the right a column with RPhi on top and below
// RhoZ and the side 3D view next to each other.
//
//   +-----------+-----------------+
//   |           |      RPhi       |
//   |    3D     +--------+--------+
//   |           |  RhoZ  |  Side  |
//   +-----------+--------+--------+
void nestedLayout()
{
   auto wm = eveMng->GetWindowManager();
   wm->ResetLayout();
   auto main = wm->GetMainPack(); // horizontal
   main->AddViewer(viewers[0], 1.5);
   auto column = main->NewVerticalPack(1);
   column->AddViewer(viewers[1]);
   auto row = column->NewHorizontalPack(1);
   row->AddViewer(viewers[2]);
   row->AddViewer(viewers[3]);
}

// All viewers stacked vertically.
void columnLayout()
{
   auto wm = eveMng->GetWindowManager();
   wm->ResetLayout();
   auto main = wm->GetMainPack();
   main->SetVertical();
   for (auto v : viewers)
      main->AddViewer(v);
}

class LayoutSwitcher : public REX::REveElement {
   int fLayout{0};

public:
   LayoutSwitcher() : REX::REveElement("LayoutSwitcher") {}

   void SwitchLayout()
   {
      fLayout = (fLayout + 1) % 3;
      if (fLayout == 0)
         nestedLayout();
      else if (fLayout == 1)
         columnLayout();
      else
         eveMng->GetWindowManager()->ResetLayout(); // client default layout
   }
};

void window_manager()
{
   eveMng = REX::REveManager::Create();

   eveMng->GetGlobalScene()->AddElement(makeBox(50, 100, kCyan));
   eveMng->GetEventScene()->AddElement(makePoints(100, 40, kRed));

   viewers.push_back(eveMng->GetDefaultViewer());
   viewers.push_back(makeProjectedView("RPhi View", REX::REveProjection::kPT_RPhi));
   viewers.push_back(makeProjectedView("RhoZ View", REX::REveProjection::kPT_RhoZ));

   auto side = eveMng->SpawnNewViewer("Side View");
   side->AddScene(eveMng->GetGlobalScene());
   side->AddScene(eveMng->GetEventScene());
   side->SetCameraType(REX::REveViewer::kCameraOrthoZOY);
   viewers.push_back(side);

   nestedLayout();

   auto switcher = new LayoutSwitcher();
   eveMng->GetWorld()->AddElement(switcher);
   eveMng->GetWorld()->AddCommand("SwitchLayout", "sap-icon://grid", switcher, "SwitchLayout()");

   eveMng->Show();
}

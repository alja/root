
#include <ROOT/REveGeoTopNode.hxx> 
#include <ROOT/REveRenderData.hxx> 
#include <ROOT/RGeomData.hxx>


#include "TMath.h"
#include "TClass.h"
#include "TGeoNode.h"
#include "TGeoManager.h"
#include "TBase64.h"

#include <cassert>
#include <iostream>

#include <nlohmann/json.hpp>


using namespace ROOT::Experimental;

////////////////////////////////////////////////////////////////////////////////
/// Constructor.

REveGeoTopNode::REveGeoTopNode(const Text_t* n, const Text_t* t) :
   REveElement(n, t)
{
}

////////////////////////////////////////////////////////////////////////////////
/// Fill core part of JSON representation.
namespace {
TGeoNode *getNodeFromPath(TGeoNode *top, std::string path)
{
   TGeoNode *node = top;
   std::istringstream f(path);
   std::string s;
   while (getline(f, s, '/'))
      node = node->GetVolume()->FindNode(s.c_str());

   return node;
}
} // namespace

Int_t REveGeoTopNode::WriteCoreJson(nlohmann::json &j, Int_t rnr_offset)
{
   // test
   TGeoNode* top = gGeoManager->GetTopVolume()->FindNode("CMSE_1");
   TGeoNode* n = getNodeFromPath(top, "TRAK_1/SVTX_1/TGBX_1/GAW1_1");

   // end of demo prep

   Int_t ret = REveElement::WriteCoreJson(j, rnr_offset);

   ROOT::Experimental::RGeomDescription data;

   data.Build(n->GetVolume());

   std::string json = data.ProduceJson();

   j["geomDescription"] = TBase64::Encode(json.c_str());
   return ret;
}

////////////////////////////////////////////////////////////////////////////////
/// Crates 3D point array for rendering.

void REveGeoTopNode::BuildRenderData()
{
   fRenderData = std::make_unique<REveRenderData>("makeGeoTopNode");
}

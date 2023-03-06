
#include <ROOT/REveGeoTopNode.hxx> 
#include <ROOT/REveRenderData.hxx> 
#include <ROOT/RGeomData.hxx>
#include <ROOT/REveManager.hxx>


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

REveGeoTopNodeData::REveGeoTopNodeData(const Text_t* n, const Text_t* t) :
   REveElement(n, t)
{
}

void REveGeoTopNodeData::SetTNode(TGeoNode* n) 
{
   fGeoNode = n;
   fDesc.Build(fGeoNode->GetVolume());
}

void REveGeoTopNodeData::SetChannel(int chid)
{
   fWebHierarchy = std::make_shared<RGeomHierarchy>(fDesc);
   printf(">> set channel %d\n", chid);
   
   fWebHierarchy->Show({ gEve->GetWebWindow(), chid });
}



////////////////////////////////////////////////////////////////////////////////
/// Fill core part of JSON representation.

Int_t REveGeoTopNodeData::WriteCoreJson(nlohmann::json &j, Int_t rnr_offset)
{
   Int_t ret = REveElement::WriteCoreJson(j, rnr_offset);

   if (!fGeoNode){ return ret;}

   std::string json = fDesc.ProduceJson();

   //std::cout << json << "\n";

   j["geomDescription"] = TBase64::Encode(json.c_str());
   j["UT_PostStream"] = "UT_GeoTopNode_PostStream";
   return ret;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
REveGeoTopNodeViz::REveGeoTopNodeViz(const Text_t* n, const Text_t* t) :
   REveElement(n, t)
{
}

void REveGeoTopNodeViz::BuildRenderData()
{
   fRenderData = std::make_unique<REveRenderData>("makeGeoTopNode");
}

int REveGeoTopNodeViz::WriteCoreJson(nlohmann::json &j, Int_t rnr_offset)
{
   Int_t ret = REveElement::WriteCoreJson(j, rnr_offset);
   if (!fGeoData)
      j["dataId"] = -1;
   else
      j["dataId"] = fGeoData->GetElementId();

   return ret;
}

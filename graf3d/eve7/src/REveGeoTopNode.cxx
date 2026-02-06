
#include <ROOT/REveGeoTopNode.hxx>
#include <ROOT/REveRenderData.hxx>
#include <ROOT/RGeomData.hxx>
#include <ROOT/RWebWindow.hxx>
#include <ROOT/REveManager.hxx>
#include <ROOT/REveGeoPolyShape.hxx>

#include <ROOT/REveSelection.hxx>

#include <ROOT/REveUtil.hxx>



#include "TBufferJSON.h"
#include "TMath.h"

#include "TGeoCompositeShape.h"
#include "TGeoManager.h"
#include "TClass.h"
#include "TGeoNode.h"
#include "TBase64.h"

#include <cassert>
#include <iostream>

#include <nlohmann/json.hpp>


using namespace ROOT::Experimental;

thread_local ElementId_t gSelId;

#define REVEGEO_DEBUG
#ifdef REVEGEO_DEBUG
#define REVEGEO_DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define REVEGEO_DEBUG_PRINT(fmt, ...)
#endif


////////////////////////////////////////////////////////////////////////////////
/// Table signal handling

void REveGeomHierarchy::WebWindowCallback(unsigned connid, const std::string &arg)
{
    using namespace std::string_literals;

   if (arg.compare(0, 6, "CDTOP:") == 0) {
         fDesc.IssueSignal(this, "CdTop");
         auto connids = fWebWindow->GetConnections(connid);

         for (auto id : connids)
            fWebWindow->Send(id, "UPDATE"s);
   }
   else if (arg.compare(0, 6, "CDUP:") == 0) {
         fDesc.IssueSignal(this, "CdUp");
         auto connids = fWebWindow->GetConnections(connid);

         for (auto id : connids)
            fWebWindow->Send(id, "UPDATE"s);
   }
   else if ((arg.compare(0, 7, "SETVI0:") == 0) || (arg.compare(0, 7, "SETVI1:") == 0)) {
      // change visibility for specified nodeid

      bool on = (arg[5] == '1');

      auto path = TBufferJSON::FromJSON<std::vector<std::string>>(arg.substr(7));
      if (fDesc.ChangeNodeVisibility(*path, on)) {
         std::cout << "set visibility NODE " << on << "\n";
         
         fReceiver->VisibilityChanged(on, false, *path);
      }
      // fDesc.IssueSignal(this, "NodeVisibility");
   }
   else if ((arg.compare(0, 5, "SHOW:") == 0) || (arg.compare(0, 5, "HIDE:") == 0)) {

         fDesc.IssueSignal(this, "NodeVisibility");
      REveManager::ChangeGuard ch;
      auto path = TBufferJSON::FromJSON<std::vector<std::string>>(arg.substr(5));
      bool on = (arg.compare(0, 5, "SHOW:") == 0);
      if (path && fDesc.SetPhysNodeVisibility(*path, on)) {
         std::cout << "Set visibilty rnr PHY \n";
         fReceiver->VisibilityChanged(on, true, *path);
      }
   }

   else {
      RGeomHierarchy::WebWindowCallback(connid, arg);
   }
}


////////////////////////////////////////////////////////////////////////////////
/// Constructor.

REveGeoTopNodeData::REveGeoTopNodeData(const Text_t *n, const Text_t *t) : REveElement(n, t)
{
   fWebHierarchy = std::make_shared<REveGeomHierarchy>(fDesc, true);
   fWebHierarchy->SetReceiver(this);

   // this below will be obsolete
   fDesc.AddSignalHandler(this, [this](const std::string &kind) { ProcessSignal(kind); });
}

TGeoNode *REveGeoTopNodeData::locateNodeWithPath(const std::vector<std::string> &path)
{
   TGeoNode *top = gGeoManager->GetTopNode();
   printf("Top node name from geoData name (%s)\n", top->GetName());
   for (size_t t = 0; t < path.size(); t++) {
      std::string s = path[t];
       std::cout << s << std::endl;
      // if (t > 0)


      top = top->GetVolume()->FindNode(s.c_str());
   }
   return top;
}

void REveGeoTopNodeData::SetTNode(TGeoNode *n)
{
   fGeoNode = n;
   printf("set top node %s \n", n->GetName());
   fDesc.Build(fGeoNode->GetVolume());
   //sc.AddSignalHandler(this, [this](const std::string &kind) { ProcessSignal(kind); });

   for (auto &el : fNieces) {
      REveGeoTopNodeViz *etn = dynamic_cast<REveGeoTopNodeViz *>(el);
      etn->BuildDesc();
   }
}

void REveGeoTopNodeData::SetTopNodeWithPath(const std::vector<std::string>& path)
{
   fGeoNodePath = path;
   TGeoNode* n = locateNodeWithPath(path);
   SetTNode(n);
}


void REveGeoTopNodeData::VisibilityChanged(bool on, bool phy, const std::vector<std::string>& path)
{

   for (auto &el : fNieces) {
      REveGeoTopNodeViz *etn = dynamic_cast<REveGeoTopNodeViz *>(el);
      etn->VisibilityChanged(on, phy, path);
   }
}


////////////////////////////////////////////////////////////////////////////////

void REveGeoTopNodeData::SetChannel(unsigned connid, int chid)
{
   fWebHierarchy->Show({gEve->GetWebWindow(), connid, chid});
}

////////////////////////////////////////////////////////////////////////////////
/*
namespace {
std::size_t getHash(std::vector<int> &vec)
{
   std::size_t seed = vec.size();
   for (auto &x : vec) {
      uint32_t i = (uint32_t)x;
      seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
   }
   return seed;
}
} // namespace
*/
void REveGeoTopNodeData::ProcessSignal(const std::string &kind)
{
   REveManager::ChangeGuard ch;
   if ((kind == "SelectTop") || (kind == "NodeVisibility")) {
      printf("Select top callback !!!\n");
      const std::vector<int> &sstack = fDesc.GetSelectedStack();
      std::vector<std::string> path = fDesc.MakePathByStack(sstack);
      std::vector<std::string> result = fGeoNodePath;
      if (path.size() > 1) {
         result.insert(result.end(), path.begin() + 1, path.end());

         // TGeoNode* n = locateNodeWithPath(path);
         SetTopNodeWithPath(result);
      }
      /*
      for (auto &n : fNieces) {
         REveGeoTopNodeViz* viz = dynamic_cast<REveGeoTopNodeViz*>(n);
         viz->BuildDesc();
      }*/
   } 
   else if (kind == "CdTop")
   {
     //  TGeoNode* n = gGeoManager->GetTopNode();
     // SetTNode(n);
     std::vector<std::string > ep;
     SetTopNodeWithPath(ep);
   }

   else if (kind == "CdUp") {
       std::vector<std::string> result = fGeoNodePath;
       result.pop_back();
       SetTopNodeWithPath(result);
   }

   else if (kind == "HighlightItem") {
      /*
      printf("REveGeoTopNodeData element highlighted --------------------------------"\n);
      */

   } else if (kind == "ClickItem") {
      printf("REveGeoTopNodeData element CLICKED selected --------------------------------\n");
      auto sstack = fDesc.GetClickedItem();
      std::set<int> ss;

      for (auto &n : fNieces) {
         REveGeoTopNodeViz* viz = dynamic_cast<REveGeoTopNodeViz*>(n);
         viz->GetIndicesFromBrowserStack(sstack, ss);
         bool multi = false;
         bool secondary = true;
         gEve->GetSelection()->NewElementPicked(n->GetElementId(), multi, secondary, ss);
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Fill core part of JSON representation.

Int_t REveGeoTopNodeData::WriteCoreJson(nlohmann::json &j, Int_t rnr_offset)
{
   Int_t ret = REveElement::WriteCoreJson(j, rnr_offset);

   if (!fGeoNode){ return ret;}
   return ret;
}

std::string REveGeoTopNodeData::GetNodePathAsFlatString() const
{
   if (fGeoNodePath.empty())
      return "";

   std::ostringstream oss;

   oss << fGeoNodePath[0];

   for (size_t i = 1; i < fGeoNodePath.size(); ++i)
      oss << "/" << fGeoNodePath[i];

   return oss.str();
}
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
REveGeoTopNodeViz::REveGeoTopNodeViz(const Text_t *n, const Text_t *t) : REveElement(n, t) {
   SetAlwaysSecSelect(true);
}

std::string REveGeoTopNodeViz::GetHighlightTooltip(const std::set<int> & set) const
{
   if (set.empty()) {
      return "";
   } else {
      auto it = set.begin();
      int pos = *it;
      //const BNode &bn = fNodes[pos];

      std::string res = "GeoNode name";

      TGeoNode *top = fGeoData->fGeoNode;
      TGeoIterator git(top->GetVolume());
      TGeoNode *node;
      int i = 0;
      int vislevel = fGeoData->fDesc.GetVisLevel();
            TString path;
      while ((node = git.Next())) {

         if (git.GetLevel() > vislevel)
         {
            continue;
         }
         if (i == pos) {
            git.GetPath(path);
            res = path;
            break;
         }
         i++;
      }
      return res;
   }
}

void REveGeoTopNodeViz::BuildDesc()
{
   // locate top node
   const std::vector<int> &stack = fGeoData->fDesc.GetSelectedStack();
   std::vector<std::string> path = fGeoData->fDesc.MakePathByStack(stack);
   TGeoNode *top = fGeoData->fGeoNode;
   printf("Top node name from geoData name (%s)\n", top->GetName());
   for (size_t t = 0; t < path.size(); t++) {
      std::string s = path[t];
      std::cout << s << std::endl;
      if (t > 0)
         top = top->GetVolume()->FindNode(s.c_str());
   }

   fNodes.clear();
   fShapes.clear();
   // shape array
   std::set<TGeoShape *> shapes;
   CollectShapes(top, shapes, fShapes);
   std::cout << "Shape size " << shapes.size() << "\n";

   // node array
   CollectNodes(top->GetVolume(), fNodes, fShapes, fGeoData->fDesc.GetVisLevel());
   std::cout << "Node size " << fNodes.size() << "\n";

   StampObjProps();
}

void REveGeoTopNodeViz::CollectNodes(TGeoVolume *volume, std::vector<BNode> &bnl, std::vector<BShape> &browsables,
                                     int vislevel)
{
   printf("collect nodes \n");
   TGeoIterator it(volume);
   TGeoNode *node;
   int nodeId = 0;
   while ((node = it.Next())) {

      // block at vislevel

      if (it.GetLevel() > vislevel)
         continue;

      const TGeoMatrix *mat = it.GetCurrentMatrix();
      const Double_t *t = mat->GetTranslation();    // size 3
      const Double_t *r = mat->GetRotationMatrix(); // size 9 (3x3)

      Double_t m[16];
      if (mat->IsScale()) {
         const Double_t *s = mat->GetScale();
         m[0] = r[0] * s[0];
         m[1] = r[3] * s[0];
         m[2] = r[6] * s[0];
         m[3] = 0;
         m[4] = r[1] * s[1];
         m[5] = r[4] * s[1];
         m[6] = r[7] * s[1];
         m[7] = 0;
         m[8] = r[2] * s[2];
         m[9] = r[5] * s[2];
         m[10] = r[8] * s[2];
         m[11] = 0;
         m[12] = t[0];
         m[13] = t[1];
         m[14] = t[2];
         m[15] = 1;
      } else {
         m[0] = r[0];
         m[1] = r[3];
         m[2] = r[6];
         m[3] = 0;
         m[4] = r[1];
         m[5] = r[4];
         m[6] = r[7];
         m[7] = 0;
         m[8] = r[2];
         m[9] = r[5];
         m[10] = r[8];
         m[11] = 0;
         m[12] = t[0];
         m[13] = t[1];
         m[14] = t[2];
         m[15] = 1;
      }

      BNode b;
      b.node = node;
      b.nodeId = nodeId;
      b.color = node->GetVolume()->GetLineColor();
      // TString path; it.GetPath(path);
      // printf("[%d] %d %s \n", b.color, it.GetLevel(), path.Data());
      // set BNode transformation matrix
      for (int i = 0; i < 16; ++i)
         b.trans[i] = m[i];

      // find shape
      TGeoShape *shape = node->GetVolume()->GetShape();
      b.shapeId = -1; // mark invalid at start
      for (size_t i = 0; i < browsables.size(); i++) {
         if (shape == browsables[i].shape) {
            b.shapeId = i;
            break;
         }
      }
      assert(b.shapeId >= 0);
      // printf("Node %d shape id %d \n", (int)bnl.size(), b.shapeId);
      bnl.push_back(b);
      nodeId++;

      //  break;
   }
}

void REveGeoTopNodeViz::CollectShapes(TGeoNode *tnode, std::set<TGeoShape *> &shapes, std::vector<BShape> &browsables)
{
   printf("collect shapes \n");
   TGeoIterator geoit(tnode->GetVolume());
   TGeoNode *node = nullptr;
   int vislevel = fGeoData->fDesc.GetVisLevel();
   while ((node = geoit.Next())) {

      // block at vislevel

      if (geoit.GetLevel() > vislevel)
         continue;

      TGeoVolume *vol = node->GetVolume();
      if (vol) {
         TGeoShape *shape = vol->GetShape();
         if (shape) {
            auto it = shapes.find(shape);
            if (it == shapes.end()) {
               shapes.insert(shape); // use set to avoid duplicates
               REveGeoPolyShape polyShape;
               TGeoCompositeShape *compositeShape = dynamic_cast<TGeoCompositeShape *>(shape);
               int n_seg = 60; // default value in the geo manager and poly shape
               if (compositeShape)
                  polyShape.BuildFromComposite(compositeShape, n_seg);
               else
                  polyShape.BuildFromShape(shape, n_seg);

               // printf("[%d] Shape name %s %s \n",(int)browsables.size(), shape->GetName(), shape->ClassName());

               //   printf("vertices %lu: \n", polyShape.fVertices.size());

               // create browser shape
               BShape browserShape;
               browserShape.shape = shape;
               browsables.push_back(browserShape);

               // copy vertices transform vec double to float
               browsables.back().vertices.reserve(polyShape.fVertices.size());
               for (size_t i = 0; i < polyShape.fVertices.size(); i++)
                  browsables.back().vertices.push_back(polyShape.fVertices[i]);

               // copy indices kip the first integer in the sequence of 4
               for (size_t i = 0; i < polyShape.fPolyDesc.size(); i += 4) {
                  browsables.back().indices.push_back(polyShape.fPolyDesc[i + 1]);
                  browsables.back().indices.push_back(polyShape.fPolyDesc[i + 2]);
                  browsables.back().indices.push_back(polyShape.fPolyDesc[i + 3]);
               }
               // printf("last browsable size indices size %lu \n",  browsables.back().indices.size());
            }
         }
      }
   }
}

void REveGeoTopNodeViz::BuildRenderData()
{
   fRenderData = std::make_unique<REveRenderData>("makeGeoTopNode");
   for (size_t i = 0; i < fNodes.size(); ++i) {

      UChar_t c[4] = {1, 2, 3, 4};
      REveUtil::ColorFromIdx(fNodes[i].color, c);
      // if (i < 400) printf("%d > %d %d %d %d \n",fNodes[i].color, c[0], c[1], c[2], c[3]);
      uint32_t v = (c[0] << 16) + (c[1] << 8) + c[2];
      float pc = *(float *)&v;

      GetRenderData()->PushV(pc);
   }
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void REveGeoTopNodeViz::SetGeoData(REveGeoTopNodeData *d, bool rebuild)
{
   fGeoData = d;
   if (rebuild)
      BuildDesc();
}

//------------------------------------------------------------------------------

int REveGeoTopNodeViz::WriteCoreJson(nlohmann::json &j, Int_t rnr_offset)
{

   Int_t ret = REveElement::WriteCoreJson(j, rnr_offset);

   if (!fGeoData) {
      j["dataId"] = -1;
   } else {
      // std::string json = fGeoData->fDesc.ProduceJson();
      // j["geomDescription"] = TBase64::Encode(json.c_str());
      j["dataId"] = fGeoData->GetElementId();
   }
   j["visLevel"] = fGeoData ? fGeoData->fDesc.GetVisLevel() : 0;

   // put shapes vector in json array
   using namespace nlohmann;

   json shapeVertexArr = json::array();
   int vertexOff = 0;

   json shapeIndexArr = json::array();
   json shapePolySizeArr = json::array();
   json shapePolyOffArr = json::array();

   json nodeVisibility = json::array();

   int polyOff = 0;

   // need four integers for
   for (size_t i = 0; i < fShapes.size(); ++i) {
      // vertices

      std::copy(fShapes[i].vertices.begin(), fShapes[i].vertices.end(), std::back_inserter(shapeVertexArr));

      int numVertices = int(fShapes[i].vertices.size());
      // indices
      // write shape indices with the vertexOff
      for (size_t p = 0; p < fShapes[i].indices.size(); ++p)
         shapeIndexArr.push_back(fShapes[i].indices[p] + vertexOff);

      int numIndices = int(fShapes[i].indices.size());
      shapePolySizeArr.push_back(numIndices);
      shapePolyOffArr.push_back(polyOff);

      // printf("shape [%d] numIndices %d \n", i, numIndices);

      polyOff += numIndices;
      vertexOff += numVertices / 3;
   }

   // write vector of shape ids for visible nodes
   json nodeShapeIds = json::array();
   json nodeTrans = json::array();
   json nodeColors = json::array();

   for (size_t i = 0; i < fNodes.size(); ++i) {
      nodeShapeIds.push_back(fNodes[i].shapeId);
      nodeVisibility.push_back(fNodes[i].visible);
      for (int t = 0; t < 16; t++)
         nodeTrans.push_back(fNodes[i].trans[t]);
   }
   // shape basic array

   j["shapeVertices"] = shapeVertexArr;

   // shape basic indices array
   j["shapeIndices"] = shapeIndexArr;

   // shape poly offset array
   j["shapeIndicesOff"] = shapePolyOffArr;
   j["shapeIndicesSize"] = shapePolySizeArr;

   j["nodeShapeIds"] = nodeShapeIds;
   j["nodeTrans"] = nodeTrans;
   j["nodeVisibility"] = nodeVisibility;
   j["fSecondarySelect"] = fAlwaysSecSelect;

   // std::cout << "Write Core json " << j.dump(1) << "\n";
   return ret;
}

void REveGeoTopNodeViz::SetVisLevel(int vl)
{
   if (fGeoData) {
      fGeoData->fDesc.SetVisLevel(vl);
      StampObjProps();
   }
}

void REveGeoTopNodeViz::GetIndicesFromBrowserStack(const std::vector<int> &stack, std::set<int> &outStack)
{
   std::vector<std::string> path = fGeoData->fDesc.MakePathByStack(stack);
   // TGeoNode* node = fGeoData->locateNodeWithPath(path);

   std::vector<std::string> result = fGeoData->fGeoNodePath;
   if (path.size() > 1)
      result.insert(result.end(), path.begin() + 1, path.end());

   TGeoNode *node = fGeoData->locateNodeWithPath(result);
   if (!node) {
      printf("no node with given stack \n");
   }

   std::set<TGeoNode *> cset;

   // add children
   TGeoIterator it(node->GetVolume());
   TGeoNode *cnd;
   int level = fGeoData->fDesc.GetVisLevel() + 1 - path.size();
   while ((cnd = it())) {
      if (it.GetLevel() <= level) {
         cset.insert(cnd);
      }
   }

   // add self
   cset.insert(node);

   for (size_t i = 0; i < fNodes.size(); i++) {
      if (cset.find(fNodes[i].node) != cset.end()) {
         outStack.insert(i);
         // printf("Fill extra selection data matched node with sequence ID = %zu \n", i);
      }
   }

   printf("GetIndicesFromBrowserStack size %zu\n", outStack.size());
}

void REveGeoTopNodeViz::VisibilityChanged(bool on, bool phy, const std::vector<std::string> &path)
{
   std::cout << "TGeo Node VIX viz changes !!!! PHY === " << phy << "\n";

   std::vector<std::string> result = fGeoData->fGeoNodePath;
   if (path.size() > 1)
      result.insert(result.end(), path.begin() + 1, path.end());

   TGeoNode *node = fGeoData->locateNodeWithPath(result);

   for (size_t i = 0; i < fNodes.size(); i++) {
      if (fNodes[i].node == node) {
         printf("change node visibility for %zu to val %d \n", i, on);
         fNodes[i].visible = on;
         break;
      }
   }

   StampObjProps();
}
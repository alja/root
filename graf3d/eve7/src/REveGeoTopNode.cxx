
#include <ROOT/REveGeoTopNode.hxx>
#include <ROOT/REveRenderData.hxx>
#include <ROOT/RGeomData.hxx>
#include <ROOT/REveManager.hxx>
#include <ROOT/REveGeoPolyShape.hxx>

#include <ROOT/REveSelection.hxx>

#include <ROOT/REveUtil.hxx>


#include "TMath.h"

#include "TGeoCompositeShape.h"
#include "TGeoManager.h"
#include "TClass.h"
#include "TGeoNode.h"
#include "TGeoManager.h"
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
/// Constructor.

REveGeoTopNodeData::REveGeoTopNodeData(const Text_t *n, const Text_t *t) : REveElement(n, t)
{
   fWebHierarchy = std::make_shared<RGeomHierarchy>(fDesc, true);
}

void REveGeoTopNodeData::SetTNode(TGeoNode *n)
{
   fGeoNode = n;
   fDesc.Build(fGeoNode->GetVolume());
   fDesc.AddSignalHandler(this, [this](const std::string &kind) { ProcessSignal(kind); });

   for (auto &el : fNieces) {
      REveGeoTopNodeViz *etn = dynamic_cast<REveGeoTopNodeViz *>(el);
      etn->BuildDesc();
   }
}
////////////////////////////////////////////////////////////////////////////////

void REveGeoTopNodeData::SetChannel(unsigned connid, int chid)
{
   fWebHierarchy->Show({gEve->GetWebWindow(), connid, chid});
}

////////////////////////////////////////////////////////////////////////////////
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

void REveGeoTopNodeData::ProcessSignal(const std::string &kind)
{
   REveManager::ChangeGuard ch;
   if ((kind == "SelectTop") || (kind == "NodeVisibility")) {
      printf("Select top callback !!!\n");
      StampObjProps();
      for (auto &n : fNieces) {
         REveGeoTopNodeViz* viz = dynamic_cast<REveGeoTopNodeViz*>(n);
         viz->BuildDesc();
      }
   } else if (kind == "HighlightItem") {
      // printf("REveGeoTopNodeData element highlighted --------------------------------");
      auto sstack = fDesc.GetHighlightedItem();
      std::set<int> ss;
      ss.insert((int)getHash(sstack));
      for (auto &n : fNieces) {
         gEve->GetHighlight()->NewElementPicked(n->GetElementId(), false, true, ss);
      }
      gSelId = gEve->GetHighlight()->GetElementId();

   } else if (kind == "ClickItem") {
      // printf("REveGeoTopNodeData element selected --------------------------------");
      auto sstack = fDesc.GetClickedItem();
      std::set<int> ss;
      ss.insert((int)getHash(sstack));

      for (auto &n : fNieces) {
         gEve->GetSelection()->NewElementPicked(n->GetElementId(), false, true, ss);
      }
      gSelId = gEve->GetSelection()->GetElementId();
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

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
REveGeoTopNodeViz::REveGeoTopNodeViz(const Text_t *n, const Text_t *t) : REveElement(n, t) {}

std::string REveGeoTopNodeViz::GetHighlightTooltip(const std::set<int> &) const
{
   auto stack = fGeoData->fDesc.GetHighlightedItem();
   auto sa = fGeoData->fDesc.MakePathByStack(stack);
   if (sa.empty())
      return "";
   else {
      std::string res;
      size_t n = sa.size();
      for (size_t i = 0; i < n; ++i) {
         res += sa[i];
         if (i < (n - 1))
            res += "/";
      }
      printf("Path to tooltip %s \n", res.c_str());
      return res;
   }
}

void REveGeoTopNodeViz::BuildDesc()
{
   // locate top node
   const std::vector<int>& stack = fGeoData->fDesc.GetSelectedStack();
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

void REveGeoTopNodeViz::CollectNodes(TGeoVolume *volume, std::vector<BNode> &bnl, std::vector<BShape> &browsables, int vislevel)
{
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

      Double_t        m[16];
      if (mat->IsScale())
      {
         const Double_t *s = mat->GetScale();
         m[0]  = r[0]*s[0]; m[1]  = r[3]*s[0]; m[2]  = r[6]*s[0]; m[3]  = 0;
         m[4]  = r[1]*s[1]; m[5]  = r[4]*s[1]; m[6]  = r[7]*s[1]; m[7]  = 0;
         m[8]  = r[2]*s[2]; m[9]  = r[5]*s[2]; m[10] = r[8]*s[2]; m[11] = 0;
         m[12] = t[0];      m[13] = t[1];      m[14] = t[2];      m[15] = 1;
      }
      else
      {
         m[0]  = r[0];      m[1]  = r[3];      m[2]  = r[6];      m[3]  = 0;
         m[4]  = r[1];      m[5]  = r[4];      m[6]  = r[7];      m[7]  = 0;
         m[8]  = r[2];      m[9]  = r[5];      m[10] = r[8];      m[11] = 0;
         m[12] = t[0];      m[13] = t[1];      m[14] = t[2];      m[15] = 1;
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

void REveGeoTopNodeViz::CollectShapes(TGeoNode *node, std::set<TGeoShape *> &shapes, std::vector<BShape> &browsables)
{
   if (!node)
      return;

   // Get the volume
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
           /*
            for (size_t i = 0; i < polyShape.fVertices.size(); i += 3) {
               printf("V[%zu] = (%g, %g, %g)\n", i / 3, polyShape.fVertices[i], polyShape.fVertices[i + 1],
                      polyShape.fVertices[i + 2]);
            }*/

         //   printf("num polygons %d\n", polyShape.fNbPols);
            /* for (size_t i = 0; i < polyShape.fPolyDesc.size(); i += 4) {
               printf("POLY [%zu] = (%d, %d, %d, %d)\n", i / 4, polyShape.fPolyDesc[i], polyShape.fPolyDesc[i + 1],
                      polyShape.fPolyDesc[i + 2], polyShape.fPolyDesc[i + 3]);
            }*/

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

   // Recurse to children
   int nd = node->GetNdaughters();
   for (int i = 0; i < nd; ++i) {
      CollectShapes(node->GetDaughter(i), shapes, browsables);
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
      float pc = *(float *) &v;

      GetRenderData()->PushV(pc);
   }
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void REveGeoTopNodeViz::SetGeoData(REveGeoTopNodeData *d, bool rebuild)
{
   fGeoData = d;
   if (rebuild) BuildDesc();
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
   json shapePolyOffArr = json::array(); // optional
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
   json nodeIds = json::array();

   for (size_t i = 0; i < fNodes.size(); ++i) {
      nodeShapeIds.push_back(fNodes[i].shapeId);
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
  // pack["nodeColors"] = nodeColors;
   j["nodeIds"] = nodeIds;



   // std::cout << "Write Core json " << j.dump(1) << "\n";
   return ret;
}

void REveGeoTopNodeViz::FillExtraSelectionData(nlohmann::json &j, const std::set<int> &) const
{
   j["stack"] = nlohmann::json::array();
   std::vector<int> stack;
   if (gSelId == gEve->GetHighlight()->GetElementId())
      stack = fGeoData->fDesc.GetHighlightedItem();
   else if (gSelId == gEve->GetSelection()->GetElementId())
      stack = fGeoData->fDesc.GetClickedItem();

   if (stack.empty())
      return;

#ifdef REVEGEO_DEBUG
   printf("cicked stack: ");
   for (auto i : stack)
      printf(" %d, ", i);
   printf("\n");
#endif

   for (auto i : stack)
      j["stack"].push_back(i);


#ifdef REVEGEO_DEBUG
   printf("extra stack: ");
   int ss = j["stack"].size();
   for (int i = 0; i < ss; ++i) {
      int d = j["stack"][i];
      printf(" %d,", d);
   }
   printf("----\n");
   auto ids = fGeoData->fDesc.MakeIdsByStack(stack);
   printf("node ids from stack: ");
   for (auto i : ids)
      printf(" %d, ", i);
   printf("\n");

   int id = fGeoData->fDesc.FindNodeId(stack);
   printf("NODE ID %d\n", id);
#endif
}

void REveGeoTopNodeViz::SetVisLevel(int vl) {
   if (fGeoData) {
      fGeoData->fDesc.SetVisLevel(vl);
      StampObjProps();
   }
}
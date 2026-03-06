/// \file
/// \ingroup tutorial_eve_7
///
/// \macro_code
///

#include <ROOT/REveGeoTopNode.hxx>
#include <ROOT/REveGeoPolyShape.hxx>
#include <ROOT/REveManager.hxx>

#include <set>
#include <vector>
#include <iostream>

using namespace ROOT::Experimental;


struct BShape
{
   TGeoShape* shape;
   std::vector<int> indices;
   std::vector<float> vertices;
};

struct BNode
{
   TGeoNode* node;
   int shapeId;
   int nodeId;
   int color;
   float trans[16];
};

void CollectNodes(TGeoVolume *volume, std::vector<BNode> &bnl, std::vector<BShape> &browsables, int vislevel)
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
      TString path; it.GetPath(path);
      printf("[%d] %d %s \n", b.color, it.GetLevel(), path.Data());
      // set BNode transformation matrix
      for (int i = 0; i < 16; ++i)
         b.trans[i] = m[i];

      // find shape
      TGeoShape *shape = node->GetVolume()->GetShape();
      b.shapeId = -1; // mark invalid at start
      for (int i = 0; i < browsables.size(); i++) {
         if (shape == browsables[i].shape) {
            b.shapeId = i;
            break;
         }
      }
      assert(b.shapeId >= 0);
      // printf("Node %d shape id %d \n", (int)bnl.size(), b.shapeId);
      bnl.push_back(b);
      nodeId++;

      break;
    
   }
}

void CollectShapes(TGeoNode *node, std::set<TGeoShape *> &shapes, std::vector<BShape> &browsables)
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

            printf("[%d] Shape name %s %s \n",(int)browsables.size(), shape->GetName(), shape->ClassName());
         
            printf("vertices %lu: \n", polyShape.fVertices.size());
            /*
            for (size_t i = 0; i < polyShape.fVertices.size(); i += 3) {
               printf("V[%zu] = (%g, %g, %g)\n", i / 3, polyShape.fVertices[i], polyShape.fVertices[i + 1],
                      polyShape.fVertices[i + 2]);
            }*/

            printf("num polygons %d\n", polyShape.fNbPols);
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
            printf("last browsable size indices size %lu \n",  browsables.back().indices.size());
         }
      }
   }

   // Recurse to children
   int nd = node->GetNdaughters();
   for (int i = 0; i < nd; ++i) {
      CollectShapes(node->GetDaughter(i), shapes, browsables);
   }
}

const Double_t kR_min = 240;
const Double_t kR_max = 250;
const Double_t kZ_d = 300;

void makeJets(int N_Jets, REveElement *jetHolder)
{
   TRandom &r = *gRandom;

   for (int i = 0; i < N_Jets; i++) {
      auto jet = new REveJetCone(Form("Jet_%d", i));
      jet->SetCylinder(2 * kR_max, 2 * kZ_d);
      jet->AddEllipticCone(r.Uniform(-0.5, 0.5), r.Uniform(0, TMath::TwoPi()), 0.1, 0.2);
      jet->SetFillColor(kPink - 8);
      jet->SetLineColor(kViolet - 7);

      jetHolder->AddElement(jet);
   }
}


void eveGeoBrowser()
{
//   gEnv->SetValue("WebEve.GLViewer", "Three");
   auto eveMng = REveManager::Create();
   eveMng->AllowMultipleRemoteConnections(false, false);

   TFile::SetCacheFileDir(".");
   TGeoManager::Import("http://xrd-cache-1.t2.ucsd.edu/alja/mail/geo/cmsSimGeo2026.root");
   TGeoNode *top = gGeoManager->GetTopVolume()->FindNode("tracker:Tracker_1");
 //  top = top->GetVolume()->FindNode("pixbar:Phase2PixelBarrel_1");
//   top = top->GetVolume()->FindNode("pixel:Layer1_1");

   // initialize RGeomDesc from TGeoNode
   auto data = new REveGeoTopNodeData();
  // data->SetTNode(top);
  std::vector< std::string > path;
 path.push_back("tracker:Tracker_1");
   data->SetTopNodeWithPath(path);
   data->RefDescription().SetVisLevel(4);

   // make geoTable
   auto scene = eveMng->SpawnNewScene("GeoSceneTable");
   auto view = eveMng->SpawnNewViewer("GeoTable");
   view->AddScene(scene);
   scene->AddElement(data);

   // 3D EveViz representation
   auto geoViz = new REveGeoTopNodeViz();
   geoViz->SetVizMode(REveGeoTopNodeViz::kModeMixed);
   geoViz->SetGeoData(data);
   geoViz->SetPickable(true);


// add jets for BBox issues
   data->AddNiece(geoViz);
   eveMng->GetEventScene()->AddElement(geoViz);
   REveElement *jetHolder = new REveElement("Jets");
   eveMng->GetEventScene()->AddElement(jetHolder);
   makeJets(7, jetHolder);



   eveMng->Show();
}


#ifndef ROOT7_REveGeoTopNode
#define ROOT7_REveGeoTopNode

#include <ROOT/REveElement.hxx>
#include <ROOT/RGeomData.hxx>
#include <ROOT/RGeomHierarchy.hxx>
#include "ROOT/REveSecondarySelectable.hxx"

class TGeoNode;

namespace ROOT {
namespace Experimental {

class REveGeoTopNodeData;

class REveGeomHierarchy : public RGeomHierarchy {
   REveGeoTopNodeData* fReceiver{nullptr};

protected:
   virtual void WebWindowCallback(unsigned connid, const std::string &kind);

public:
   REveGeomHierarchy(RGeomDescription &desc, bool th) :
      RGeomHierarchy(desc, th){};
   
   void SetReceiver(REveGeoTopNodeData* data) { fReceiver = data; }
   virtual ~REveGeomHierarchy(){};
};



class REveGeoTopNodeData : public REveElement,
                           public REveAuntAsList
{
  friend class REveGeoTopNodeViz;
private:
   void SetTNode(TGeoNode* n);
protected:
   REveGeoTopNodeData(const REveGeoTopNodeData &) = delete;
   REveGeoTopNodeData &operator=(const REveGeoTopNodeData &) = delete;

   TGeoNode* fGeoNode{nullptr};
   std::vector<std::string> fGeoNodePath;
   RGeomDescription fDesc;                        ///<! geometry description, send to the client as first message
   std::shared_ptr<REveGeomHierarchy> fWebHierarchy; ///<! web handle for hierarchy part

   TGeoNode* locateNodeWithPath(const std::vector<std::string>& path);

public:
   REveGeoTopNodeData(const Text_t *n = "REveGeoTopNodeData", const Text_t *t = "");
   virtual ~REveGeoTopNodeData() {}

   Int_t WriteCoreJson(nlohmann::json &j, Int_t rnr_offset) override;
   void ProcessSignal(const std::string &);
   RGeomDescription& RefDescription() {return fDesc;}
   void SetTopNodeWithPath(const std::vector<std::string>& path);

   void SetChannel(unsigned connid, int chid);

   std::string GetNodePathAsFlatString() const;
   void VisibilityChanged(bool on, bool recurse, const std::vector<std::string>& path);
};


//-------------------------------------------------------------------
class REveGeoTopNodeViz : public REveElement,
                          public REveSecondarySelectable
{
private:
   struct BShape {
      TGeoShape *shape;
      std::vector<int> indices;
      std::vector<float> vertices;
   };

   struct BNode {
      TGeoNode *node;
      int shapeId;
      int nodeId;
      int color;
      float trans[16];
      bool visible{true};
   };
   REveGeoTopNodeViz(const REveGeoTopNodeViz &) = delete;
   REveGeoTopNodeViz &operator=(const REveGeoTopNodeViz &) = delete;

   REveGeoTopNodeData *fGeoData{nullptr};
   std::vector<BNode> fNodes;
   std::vector<BShape> fShapes;

   void CollectNodes(TGeoVolume *volume, std::vector<BNode> &bnl, std::vector<BShape> &browsables, int vislevel);

   void CollectShapes(TGeoNode *node, std::set<TGeoShape *> &shapes, std::vector<BShape> &browsables);

public:
   REveGeoTopNodeViz(const Text_t *n = "REveGeoTopNodeViz", const Text_t *t = "");
   void SetGeoData(REveGeoTopNodeData *d, bool rebuild = true);
   Int_t WriteCoreJson(nlohmann::json &j, Int_t rnr_offset) override;
   void BuildRenderData() override;
   void GetIndicesFromBrowserStack(const std::vector<int> &stack, std::set<int>& outStack);

   // bool RequiresExtraSelectionData() const override { return true; };
   // void FillExtraSelectionData(nlohmann::json &j, const std::set<int> &secondary_idcs) const override;

   void SetVisLevel(int);
   // int GetVisLevel() const { return fVisLevel; }

   void VisibilityChanged(bool on, bool phy, const std::vector<std::string>& path);
   void BuildDesc();

   using REveElement::GetHighlightTooltip;
   std::string GetHighlightTooltip(const std::set<int>& secondary_idcs) const override;
};

} // namespace Experimental
} // namespace ROOT

#endif


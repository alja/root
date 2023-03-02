
#ifndef ROOT7_REveGeoTopNode
#define ROOT7_REveGeoTopNode

#include <ROOT/REveElement.hxx>

class TGeoNode;

namespace ROOT {
namespace Experimental {


class REveGeoTopNodeData : public REveElement,
                           public REveAuntAsList
{
private:
   REveGeoTopNodeData(const REveGeoTopNodeData &) = delete;
   REveGeoTopNodeData &operator=(const REveGeoTopNodeData &) = delete;

   TGeoNode* fGeoNode{nullptr};
public:
   REveGeoTopNodeData(const Text_t *n = "REveGeoTopNode", const Text_t *t = "");
   virtual ~REveGeoTopNodeData() {}

   Int_t WriteCoreJson(nlohmann::json &j, Int_t rnr_offset) override;
   void SetTNode(TGeoNode* n) {fGeoNode = n;}
};

class REveGeoTopNodeViz : public REveElement
{
 private:
   REveGeoTopNodeViz(const REveGeoTopNodeViz &) = delete;
   REveGeoTopNodeViz &operator=(const REveGeoTopNodeViz &) = delete;

   REveGeoTopNodeData* fGeoData{nullptr};

 public:
   REveGeoTopNodeViz(const Text_t *n = "REveGeoTopNodeViz", const Text_t *t = "");
   void SetGeoData(REveGeoTopNodeData* d) {fGeoData = d;}
   Int_t WriteCoreJson(nlohmann::json &j, Int_t rnr_offset) override;
   void BuildRenderData() override;
};

} // namespace Experimental
} // namespace ROOT

#endif


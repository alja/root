
#ifndef ROOT7_REveGeoTopNode
#define ROOT7_REveGeoTopNode

#include <ROOT/REveElement.hxx>

namespace ROOT {
namespace Experimental {


class REveGeoTopNode : public REveElement
{

private:
   REveGeoTopNode(const REveGeoTopNode &) = delete;
   REveGeoTopNode &operator=(const REveGeoTopNode &) = delete;

public:
   REveGeoTopNode(const Text_t *n = "REveGeoTopNode", const Text_t *t = "");
   virtual ~REveGeoTopNode() {}
   Int_t WriteCoreJson(nlohmann::json &j, Int_t rnr_offset) override;
   void BuildRenderData() override;
};

} // namespace Experimental
} // namespace ROOT

#endif


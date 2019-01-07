#ifndef ROOT7_REveDataProxySimpleBuilderTemplate
#define ROOT7_REveDataProxySimpleBuilderTemplate


#include <ROOT/REveDataSimpleProxyBuilder.hxx>


namespace ROOT {
namespace Experimental {

template <typename T>
class REveDataSimpleProxyBuilderTemplate : public REveDataSimpleProxyBuilder {

public:
   REveDataSimpleProxyBuilderTemplate() : REveDataSimpleProxyBuilder("GL")
   {
   }

protected:
   using REveDataSimpleProxyBuilder::Build;
   virtual void Build(const void*iData, REveElement* itemHolder, const REveViewContext* context)
   {
      if(0!=iData) {
         Build(*reinterpret_cast<const T*> (iData), itemHolder, context);
      }
   }
 
   virtual void Build(const T& iData, REveElement* itemHolder, const REveViewContext* context)
   {
      throw std::runtime_error("virtual build(const T&, unsigned int, TEveElement&, const FWViewContext*) not implemented by inherited class.");
   }

private:
   REveDataSimpleProxyBuilderTemplate(const REveDataSimpleProxyBuilderTemplate&); // stop default

   const REveDataSimpleProxyBuilderTemplate& operator=(const REveDataSimpleProxyBuilderTemplate&); // stop default
};


} // namespace Experimental
} // namespace ROOT
#endif

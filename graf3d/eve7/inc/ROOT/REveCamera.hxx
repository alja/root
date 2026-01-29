// @(#)root/eve7:$Id$
// Authors: Matevz Tadel & Alja Mrak-Tadel: 2006, 2007, 2018

/*************************************************************************
 * Copyright (C) 1995-2019, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT7_REveCamera
#define ROOT7_REveCamera

#include <ROOT/REveElement.hxx>
#include <ROOT/REveVector.hxx>
#include <ROOT/REveTrans.hxx>

#include <string>

namespace ROOT {
namespace Experimental {

class REveCamera : public REveElement
{
public:
   enum ECameraType {
      // Perspective
      kCameraPerspXOZ,  // XOZ floor
      kCameraPerspYOZ,  // YOZ floor
      kCameraPerspXOY,  // XOY floor
      // Orthographic
      kCameraOrthoXOY,  // Looking down Z axis, X horz, Y vert
      kCameraOrthoXOZ,  // Looking along Y axis, X horz, Z vert
      kCameraOrthoZOY,  // Looking along X axis, Z horz, Y vert
      kCameraOrthoZOX,  // Looking along Y axis, Z horz, X vert
      // Orthographic negative
      kCameraOrthoXnOY, // Looking along Z axis, -X horz, Y vert
      kCameraOrthoXnOZ, // Looking down Y axis, -X horz, Z vert
      kCameraOrthoZnOY, // Looking down X axis, -Z horz, Y vert
      kCameraOrthoZnOX  // Looking down Y axis, -Z horz, X vert
   };

private:
   ECameraType fType;
   std::string fName;
   
   // Camera transformation matrices
   REveTrans   fCamBase;   // Base camera matrix (main positioning)
   REveTrans   fCamTrans;
   
   // Original direction vectors (for Setup)
   // REveVector  fV1;  // Camera direction vector
   // REveVector  fV2;  // Camera up vector

public:
   REveCamera();
   REveCamera(const std::string &name);
   virtual ~REveCamera() {}

   void Setup(ECameraType type, const std::string &name, const REveVector &v1, const REveVector &v2);

   ECameraType GetType() const { return fType; }
   const std::string &GetCameraName() const { return fName; }
   // const REveVector &GetDir() const { return fV1; }
   // const REveVector &GetUp() const { return fV2; }

   // void SetDir(const REveVector &v) { fV1 = v; StampObjProps(); }
   // void SetUp(const REveVector &v) { fV2 = v; StampObjProps(); }
   
   // Camera matrix accessors
   REveTrans &RefCamBase() { return fCamBase; }
   const REveTrans &GetCamBase() const { return fCamBase; }

   REveTrans &RefCamTrans() { return fCamTrans; }
   const REveTrans &GetCamTrans() const { return fCamTrans; }
   
   void SetCamBase(const REveTrans &base) { fCamBase = base; StampObjProps(); }
   
   // receive mtx from client
   void SetCamBaseMtx(const std::vector<Double_t> &arr);

   void BuildRenderData() override{};

   Int_t WriteCoreJson(nlohmann::json &j, Int_t rnr_offset) override;
};

} // namespace Experimental
} // namespace ROOT

#endif
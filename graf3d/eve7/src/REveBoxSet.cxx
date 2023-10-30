// @(#)root/eve:$Id$
// Authors: Matevz Tadel & Alja Mrak-Tadel: 2006, 2007

/*************************************************************************
 * Copyright (C) 1995-2007, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "ROOT/REveBoxSet.hxx"
#include "ROOT/REveShape.hxx"
#include "ROOT/REveRenderData.hxx"
#include "ROOT/REveRGBAPalette.hxx"
#include "ROOT/REveManager.hxx"

#include "TRandom.h"
#include <cassert>

#include <nlohmann/json.hpp>

using namespace::ROOT::Experimental;

/** \class REveBoxSet
\ingroup REve
Collection of 3D primitives (fixed-size boxes, boxes of different
sizes, or arbitrary sexto-epipeds, cones). Each primitive can be assigned
a signal value and a TRef.

A collection of 3D-markers. The way how they are defined depends
on the fBoxType data-member.
  - kBT_FreeBox         arbitrary box: specify 8*(x,y,z) box corners
  - kBT_AABox           axis-aligned box: specify (x,y,z) and (w, h, d)
  - kBT_AABoxFixedDim   axis-aligned box w/ fixed dimensions: specify (x,y,z)
                         also set fDefWidth, fDefHeight and fDefDepth
  - kBT_Cone            cone defined with position, axis-vector and radius
  - EllipticCone        cone with elliptic base (specify another radius and angle in deg)

Each primitive can be assigned:

  1. Color or signal value. Thresholds and signal-to-color mapping
     can then be set dynamically via the REveRGBAPalette class.
  2. External TObject* (stored as TRef).

See also base-class REveDigitSet for more information.
Tutorial: tutorials/eve/boxset_test.C
*/

////////////////////////////////////////////////////////////////////////////////

REveBoxSet::REveBoxSet(const char* n, const char* t) :
   REveDigitSet  (n, t),

   fBoxType      (kBT_Undef),

   fBoxSkip      (0),

   fDrawConeCap  (kFALSE)
{
   // Constructor.

   // Override from REveDigitSet.
   fDisableLighting = kFALSE;
}

////////////////////////////////////////////////////////////////////////////////
/// Return size of data-structure describing a box of type bt.

Int_t REveBoxSet::SizeofAtom(REveBoxSet::EBoxType_e bt)
{
   static const REveException eH("REveBoxSet::SizeofAtom ");

   switch (bt) {
      case kBT_Undef:                return 0;
      case kBT_FreeBox:              return sizeof(BFreeBox_t);
      case kBT_AABox:                return sizeof(BAABox_t);
      case kBT_AABoxFixedDim:        return sizeof(BAABoxFixedDim_t);
      case kBT_Mat4Box:              return sizeof(BMat4Box_t);
      case kBT_Cone:                 return sizeof(BCone_t);
      case kBT_EllipticCone:         return sizeof(BEllipticCone_t);
      case kBT_Hex:                  return sizeof(BHex_t);
      default:                       throw(eH + "unexpected atom type.");
   }
   return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// Reset the data containers to zero size.
/// The arguments describe the basic parameters of data storage.

void REveBoxSet::Reset(REveBoxSet::EBoxType_e boxType, Bool_t valIsCol, Int_t chunkSize)
{
   fBoxType      = boxType;
   fValueIsColor = valIsCol;
   fDefaultValue = valIsCol ? 0 : kMinInt;
   ReleaseIds();
   fPlex.Reset(SizeofAtom(fBoxType), chunkSize);

   printf("reset boxtype %d \n", fBoxType);
}

////////////////////////////////////////////////////////////////////////////////
/// Reset the data containers to zero size.
/// Keep the old data-storage parameters.

void REveBoxSet::Reset()
{
   ReleaseIds();
   fPlex.Reset(SizeofAtom(fBoxType), TMath::Max(fPlex.N(), 64));
}

////////////////////////////////////////////////////////////////////////////////
/// Create a new box from a set of 8 vertices.
/// To be used for box-type kBT_FreeBox.

void REveBoxSet::AddBox(const Float_t* verts)
{
   static const REveException eH("REveBoxSet::AddBox ");

   if (fBoxType != kBT_FreeBox)
      throw(eH + "expect free box-type.");

   BFreeBox_t* b = (BFreeBox_t*) NewDigit();
   memcpy(b->fVertices, verts, sizeof(b->fVertices));
   REveShape::CheckAndFixBoxOrientationFv(b->fVertices);
}

////////////////////////////////////////////////////////////////////////////////
/// Create a new axis-aligned box from at a given position and with
/// specified dimensions.
/// To be used for box-type kBT_AABox.

void REveBoxSet::AddBox(Float_t a, Float_t b, Float_t c, Float_t w, Float_t h, Float_t d)
{
   static const REveException eH("REveBoxSet::AddBox ");

   if (fBoxType != kBT_AABox)
      throw(eH + "expect axis-aligned box-type.");

   BAABox_t* box = (BAABox_t*) NewDigit();
   box->fA = a; box->fB = b; box->fC = c;
   box->fW = w; box->fH = h; box->fD = d;
}

////////////////////////////////////////////////////////////////////////////////
/// Create a new axis-aligned box from at a given position.
/// To be used for box-type kBT_AABoxFixedDim.

void REveBoxSet::AddBox(Float_t a, Float_t b, Float_t c)
{
   static const REveException eH("REveBoxSet::AddBox ");

   if (fBoxType != kBT_AABoxFixedDim)
      throw(eH + "expect axis-aligned fixed-dimension box-type.");

   BAABoxFixedDim_t* box = (BAABoxFixedDim_t*) NewDigit();
   box->fA = a; box->fB = b; box->fC = c;
}

////////////////////////////////////////////////////////////////////////////////
/// Create shape with arbitrary transformtaion
///
void REveBoxSet::AddMat4Box(const Float_t* arr)
{
   static const REveException eH("REveBoxSet::AddMat4Box ");
   if (fBoxType != kBT_Mat4Box)
      throw(eH + "expect Mat4 box-type.");

   BMat4Box_t* b = (BMat4Box_t*) NewDigit();
   memcpy(b->fMat, arr, sizeof(b->fMat));
}

////////////////////////////////////////////////////////////////////////////////
/// Create a cone with apex at pos, axis dir and radius r.
/// To be used for box-type kBT_Cone.

void REveBoxSet::AddCone(const REveVector& pos, const REveVector& dir, Float_t r)
{
   static const REveException eH("REveBoxSet::AddCone ");

   if (fBoxType != kBT_Cone)
      throw(eH + "expect cone box-type.");

   BCone_t* cone = (BCone_t*) NewDigit();
   cone->fPos = pos;
   cone->fDir = dir;
   cone->fR   = r;
}

////////////////////////////////////////////////////////////////////////////////
/// Create a cone with apex at pos, axis dir and radius r.
/// To be used for box-type kBT_EllipticCone.

void REveBoxSet::AddEllipticCone(const REveVector& pos, const REveVector& dir,
                                 Float_t r, Float_t r2, Float_t angle)
{
   static const REveException eH("REveBoxSet::AddEllipticCone ");

   if (fBoxType != kBT_EllipticCone)
      throw(eH + "expect elliptic-cone box-type.");

   BEllipticCone_t* cone = (BEllipticCone_t*) NewDigit();
   cone->fPos = pos;
   cone->fDir = dir;
   cone->fR   = r;
   cone->fR2  = r2;
   cone->fAngle = angle;
}

////////////////////////////////////////////////////////////////////////////////
/// Create a hexagonal prism with center of one hexagon at pos, radius of
/// hexagon vertices r, rotation angle angle (in degrees), and length along z
/// of depth. To be used for box-type kBT_Hex.

void REveBoxSet::AddHex(const REveVector& pos, Float_t r, Float_t angle, Float_t depth)
{
   static const REveException eH("REveBoxSet::AddEllipticCone ");

   if (fBoxType != kBT_Hex)
      throw(eH + "expect hex box-type.");

   BHex_t* hex = (BHex_t*) NewDigit();
   hex->fPos   = pos;
   hex->fR     = r;
   hex->fAngle = angle;
   hex->fDepth = depth;
}

////////////////////////////////////////////////////////////////////////////////
/// Fill bounding-box information of the base-class TAttBBox (virtual method).
/// If member 'REveFrameBox* fFrame' is set, frame's corners are used as bbox.

void REveBoxSet::ComputeBBox()
{
   static const REveException eH("REveBoxSet::ComputeBBox ");

   if (fFrame != 0)
   {
      BBoxInit();
      Int_t    n    = fFrame->GetFrameSize() / 3;
      Float_t *bbps = fFrame->GetFramePoints();
      for (int i=0; i<n; ++i, bbps+=3)
         BBoxCheckPoint(bbps);
      return;
   }

   if(fPlex.Size() == 0)
   {
      BBoxZero();
      return;
   }

   BBoxInit();

   REveChunkManager::iterator bi(fPlex);
   switch (fBoxType)
   {

      case kBT_FreeBox:
      {
         while (bi.next()) {
            BFreeBox_t& b = * (BFreeBox_t*) bi();
            for (Int_t i = 0; i < 8; ++i)
               BBoxCheckPoint(b.fVertices[i]);
         }
         break;
      }

      case kBT_AABox:
      {
         while (bi.next()) {
            BAABox_t& b = * (BAABox_t*) bi();
            BBoxCheckPoint(b.fA, b.fB, b.fC);
            BBoxCheckPoint(b.fA + b.fW, b.fB + b.fH , b.fC + b.fD);
         }
         break;
      }

      case kBT_AABoxFixedDim:
      {
         while (bi.next()) {
            BAABoxFixedDim_t& b = * (BAABoxFixedDim_t*) bi();
            BBoxCheckPoint(b.fA, b.fB, b.fC);
            BBoxCheckPoint(b.fA + fDefWidth, b.fB + fDefHeight , b.fC + fDefDepth);
         }
         break;
      }

      case kBT_Cone:
      {
         Float_t mag2=0, mag2Max=0, rMax=0;
         while (bi.next()) {
            BCone_t& b = * (BCone_t*) bi();
            BBoxCheckPoint(b.fPos.fX, b.fPos.fY, b.fPos.fZ);
            mag2 = b.fDir.Mag2();
            if (mag2>mag2Max) mag2Max=mag2;
            if (b.fR>rMax)    rMax=b.fR;
         }
         Float_t off = TMath::Sqrt(mag2Max + rMax*rMax);
         fBBox[0] -= off; fBBox[2] -= off; fBBox[4] -= off;
         fBBox[1] += off; fBBox[3] += off; fBBox[5] += off;
         break;
      }

      case kBT_EllipticCone:
      {
         Float_t mag2=0, mag2Max=0, rMax=0;
         while (bi.next()) {
            BEllipticCone_t& b = * (BEllipticCone_t*) bi();
            BBoxCheckPoint(b.fPos.fX, b.fPos.fY, b.fPos.fZ);
            mag2 = b.fDir.Mag2();
            if (mag2>mag2Max) mag2Max=mag2;
            if (b.fR  > rMax) rMax = b.fR;
            if (b.fR2 > rMax) rMax = b.fR2;
         }
         Float_t off = TMath::Sqrt(mag2Max + rMax*rMax);
         fBBox[0] -= off;fBBox[2] -= off;fBBox[4] -= off;
         fBBox[1] += off;fBBox[3] += off;fBBox[5] += off;
         break;
      }

      case kBT_Hex:
      {
         while (bi.next()) {
            BHex_t& h = * (BHex_t*) bi();
            BBoxCheckPoint(h.fPos.fX - h.fR, h.fPos.fY - h.fR, h.fPos.fZ);
            BBoxCheckPoint(h.fPos.fX + h.fR, h.fPos.fY - h.fR, h.fPos.fZ);
            BBoxCheckPoint(h.fPos.fX + h.fR, h.fPos.fY + h.fR, h.fPos.fZ);
            BBoxCheckPoint(h.fPos.fX - h.fR, h.fPos.fY + h.fR, h.fPos.fZ);
            BBoxCheckPoint(h.fPos.fX - h.fR, h.fPos.fY - h.fR, h.fPos.fZ + h.fDepth);
            BBoxCheckPoint(h.fPos.fX + h.fR, h.fPos.fY - h.fR, h.fPos.fZ + h.fDepth);
            BBoxCheckPoint(h.fPos.fX + h.fR, h.fPos.fY + h.fR, h.fPos.fZ + h.fDepth);
            BBoxCheckPoint(h.fPos.fX - h.fR, h.fPos.fY + h.fR, h.fPos.fZ + h.fDepth);
         }
         break;
      }

      case kBT_Mat4Box:
      {
         while (bi.next()) {
            BMat4Box_t& b = * (BMat4Box_t*) bi();
            float* a = b.fMat;
            BBoxCheckPoint(a[12], a[13], a[14]);
         }
         break;
      }

      default:
      {
         throw(eH + "unsupported box-type.");
      }

   } // end switch box-type
}

////////////////////////////////////////////////////////////////////////////////
/// Fill core part of JSON representation.

Int_t REveBoxSet::WriteCoreJson(nlohmann::json &j, Int_t rnr_offset)
{
   j["boxType"] = int(fBoxType);
   j["instanced"] = Instanced();
   if (Instanced())
   {
      int  N = fPlex.N();
      int  N_tex = 0;
      std::string instanceFlag;
      switch (fBoxType)
      {
         case kBT_AABoxFixedDim:
            instanceFlag = "FixedDimension";
            N_tex = N;
            break;
         case kBT_AABox:
            instanceFlag = "ScalePerDigit";
            N_tex = 2*N;
            break;
         case kBT_Mat4Box:
           instanceFlag = "Mat4Trans";
           N_tex = 4*N;
           break;
         default:
           std::cout << "Ereor:: Unhandled instanced case\n";
      }

      REveRenderData::CalcTextureSize(N_tex, 4, fTexX, fTexY);

      j["N"] = N;
      j["texX"] = fTexX;
      j["texY"] = fTexY;
      j["instanceFlag"] = instanceFlag;
      j["defWidth"] = fDefWidth;
      j["defHeight"] = fDefHeight;
      j["defDepth"] = fDefDepth;


      std::cout << "TEXTURE SIZE X " << fTexX << "\n";
   }

   // AMT:: the base class WroteCoreJson needs to be called after
   // setting the texture value
   Int_t ret = REveDigitSet::WriteCoreJson(j, rnr_offset);
   return ret;
}

////////////////////////////////////////////////////////////////////////////////
/// Creates 3D point array for rendering.

void REveBoxSet::BuildRenderData()
{
   fRenderData = std::make_unique<REveRenderData>("makeBoxSet", fPlex.Size() * 24, 0, fPlex.Size());

   REveChunkManager::iterator bi(fPlex);
   while (bi.next()) {
      REveDigitSet::DigitBase_t *b = (REveDigitSet::DigitBase_t *)bi();
      if (IsDigitVisible(b)) {
         WriteShapeData(*b);
         if (fSingleColor == false) {

            if (fValueIsColor) {
               fRenderData->PushI(int(b->fValue));
            } else if (fSingleColor == false) {
               UChar_t c[4] = {0, 0, 0, 0};
               fPalette->ColorFromValue(b->fValue, fDefaultValue, c);

               int value = c[0] + c[1] * 256 + c[2] * 256 * 256;
               // printf("box val [%d] values (%d, %d, %d) -> int <%d>\n", b.fValue, c[0], c[1], c[2],  value);
               fRenderData->PushI(value);
            }
         }
      }
   }
   if (Instanced()) {
      // printf(" >>> resize render data %d \n", 4 * fTexX * fTexY);
      fRenderData->ResizeV(4 * fTexX * fTexY);
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Get int value for color. Used for case of instancing.
///
unsigned int REveBoxSet::GetColorFromDigit(REveDigitSet::DigitBase_t &digi)
{
   if (fSingleColor == false) {
      if (fValueIsColor) {
         // printf("value is color %d\n", digi.fValue);
         return int(digi.fValue);
      } else {
         // printf("palette\n");
                  UChar_t c[4] = {0, 0, 0, 0};
         fPalette->ColorFromValue(digi.fValue, fDefaultValue, c);
         unsigned int value = c[0] + c[1] * 256 + c[2] * 256 * 256;
         return value;
      }
   }
   // printf("main color %d\n", GetMainColor());
   UChar_t c[4] = {0, 0, 0, 0};
   REveUtil::ColorFromIdx(GetMainColor(), c);
   // printf("rgb %d %d %d\n", c[0], c[1], c[2]);
   return (c[0] << 16) + (c[1] << 8) + c[2]; // AMT perhaps this can be ignored
}

float REveBoxSet::GetColorFromDigitAsFloat(REveDigitSet::DigitBase_t &digit)
{
   unsigned int c = GetColorFromDigit(digit);
   return *reinterpret_cast<float*>(&c);
}


////////////////////////////////////////////////////////////////////////////////
/// Write shape data for different cases
///
void REveBoxSet::WriteShapeData(REveDigitSet::DigitBase_t &digit)
{
   switch (fBoxType) {
   case REveBoxSet::kBT_FreeBox: {
      REveBoxSet::BFreeBox_t &b = (REveBoxSet::BFreeBox_t &)(digit);
      // vertices
      for (int c = 0; c < 8; c++) {
         for (int j = 0; j < 3; j++)
            fRenderData->PushV(b.fVertices[c][j]);
      }
      break;
   }

   case REveBoxSet::kBT_AABox: {
      REveBoxSet::BAABox_t &b = (REveBoxSet::BAABox_t &)(digit);
      // position
      fRenderData->PushV(b.fA, b.fB, b.fC);
      fRenderData->PushV(GetColorFromDigitAsFloat(b)); // color ?
      fRenderData->PushV(b.fW, b.fH, b.fD);
      fRenderData->PushV(2.f); // trasp ?
      break;
   }
   case REveBoxSet::kBT_Mat4Box: {
      REveBoxSet::BMat4Box_t &b = (REveBoxSet::BMat4Box_t &)(digit);
      float* a = b.fMat;
      fRenderData->PushV(a[12], a[13], a[14]);
      fRenderData->PushV(GetColorFromDigitAsFloat(b));
      // write the first three columns
      for (int i = 0; i < 12; i++) {
         fRenderData->PushV(a[i]);
      }
      break;
   }

    case REveBoxSet::kBT_Hex: {
      REveBoxSet::BHex_t  &b = (REveBoxSet::BHex_t &)(digit);
      fRenderData->PushV(b.fPos);
      fRenderData->PushV(GetColorFromDigitAsFloat(b));
      fRenderData->PushV(b.fR, b.fR, b.fDepth);
      fRenderData->PushV(2.f);// trasp ?

      break;
   }

   default: assert(false && "REveBoxSet::BuildRenderData only kBT_FreeBox type supported");
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Fill the structure with a random set of boxes.

void REveBoxSet::Test(Int_t nboxes)
{
   Reset(kBT_AABox, kTRUE, nboxes);
   TRandom rnd(0);
   const Float_t origin = 10, size = 2;
   Int_t color;
   for(Int_t i=0; i<nboxes; ++i)
   {
      AddBox(origin * rnd.Uniform(-1, 1),
             origin * rnd.Uniform(-1, 1),
             origin * rnd.Uniform(-1, 1),
             size   * rnd.Uniform(0.1, 1),
             size   * rnd.Uniform(0.1, 1),
             size   * rnd.Uniform(0.1, 1));

      REveUtil::ColorFromIdx(rnd.Integer(256), (UChar_t*)&color);
      DigitValue(color);
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Use instancing in RenderCore.

bool REveBoxSet::Instanced()
{
   return gEve->IsRCore() && (fBoxType != kBT_FreeBox);
}


/*
////////////////////////////////////////////////////////////////////////////////
/// Set DigitShape

bool REveBoxSet::SetDigitShape(const std::vector<float>& vrtBuff, const std::vector<int>& idxBuff)
{
   fGeoShape.v = v;

}
*/

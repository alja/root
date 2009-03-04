// @(#)root/meta:$Id$
// Author: Rene Brun   07/01/95

/*************************************************************************
 * Copyright (C) 1995-2000, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

//////////////////////////////////////////////////////////////////////////
//                                                                      //
//  The ROOT global object gROOT contains a list of all defined         //
//  classes. This list is build when a reference to a class dictionary  //
//  is made. When this happens, the static "class"::Dictionary()        //
//  function is called to create a TClass object describing the         //
//  class. The Dictionary() function is defined in the ClassDef         //
//  macro and stored (at program startup or library load time) together //
//  with the class name in the TClassTable singleton object.            //
//  For a description of all dictionary classes see TDictionary.        //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//*-*x7.5 macros/layout_class

#include "TClass.h"

#include "Riostream.h"
#include "TBaseClass.h"
#include "TBrowser.h"
#include "TBuffer.h"
#include "TClassGenerator.h"
#include "TClassEdit.h"
#include "TClassMenuItem.h"
#include "TClassRef.h"
#include "TClassTable.h"
#include "TDataMember.h"
#include "TDataType.h"
#include "TError.h"
#include "TExMap.h"
#include "TInterpreter.h"
#include "TMemberInspector.h"
#include "TMethod.h"
#include "TMethodArg.h"
#include "TMethodCall.h"
#include "TObjArray.h"
#include "TROOT.h"
#include "TRealData.h"
#include "TStreamer.h"
#include "TStreamerElement.h"
#include "TVirtualStreamerInfo.h"
#include "TVirtualCollectionProxy.h"
#include "TVirtualIsAProxy.h"
#include "TVirtualRefProxy.h"
#include "TVirtualMutex.h"
#include "TVirtualPad.h"
#include "THashTable.h"
#include "TSchemaRuleSet.h"
#include "TGenericClassInfo.h"
#include "TIsAProxy.h"

#include <cstdio>
#include <cctype>
#include <set>
#include <sstream>
#include <string>
#include <map>
#include <cmath>
#include <assert.h>

using namespace std;

// Mutex to protect CINT operations
// (exported to be used for similar cases in related classes)

TVirtualMutex* gCINTMutex = 0;

void *gMmallocDesc = 0; //is used and set in TMapFile

Int_t TClass::fgClassCount;
TClass::ENewType TClass::fgCallingNew = kRealNew;

static std::multimap<void*, Version_t> gObjectVersionRepository;

static void RegisterAddressInRepository(const char * /*where*/, void *location, const TClass *what)
{
   // Register the object for special handling in the destructor.

   Version_t version = what->GetClassVersion();
//    if (!gObjectVersionRepository.count(location)) {
//       Info(where, "Registering address %p of class '%s' version %d", location, what->GetName(), version);
//    } else {
//       Warning(where, "Registering address %p again of class '%s' version %d", location, what->GetName(), version);
//    }
   gObjectVersionRepository.insert(std::pair<void* const,Version_t>(location, version));

#if 0
   // This code could be used to prevent an address to be registered twice.
   std::pair<std::map<void*, Version_t>::iterator, Bool_t> tmp = gObjectVersionRepository.insert(std::pair<void*,Version_t>(location, version));
   if (!tmp.second) {
      Warning(where, "Reregistering an object of class '%s' version %d at address %p", what->GetName(), version, p);
      gObjectVersionRepository.erase(tmp.first);
      tmp = gObjectVersionRepository.insert(std::pair<void*,Version_t>(location, version));
      if (!tmp.second) {
         Warning(where, "Failed to reregister an object of class '%s' version %d at address %p", what->GetName(), version, location);
      }
   }
#endif
}

static void UnregisterAddressInRepository(const char * /*where*/, void *location, const TClass *what)
{
   std::multimap<void*, Version_t>::iterator cur = gObjectVersionRepository.find(location);
   for (; cur != gObjectVersionRepository.end();) {
      std::multimap<void*, Version_t>::iterator tmp = cur++;
      if ((tmp->first == location) && (tmp->second == what->GetClassVersion())) {
         // -- We still have an address, version match.
         // Info(where, "Unregistering address %p of class '%s' version %d", location, what->GetName(), what->GetClassVersion());
         gObjectVersionRepository.erase(tmp);
      } else {
         // -- No address, version match, we've reached the end.
         break;
      }
   }
}

static void MoveAddressInRepository(const char *where, void *oldadd, void *newadd, const TClass *what)
{
   UnregisterAddressInRepository(where,oldadd,what);
   RegisterAddressInRepository(where,newadd,what);
}

//______________________________________________________________________________
//______________________________________________________________________________
namespace ROOT {
#define R__USE_STD_MAP
   class TMapTypeToTClass {
#if defined R__USE_STD_MAP
     // This wrapper class allow to avoid putting #include <map> in the
     // TROOT.h header file.
   public:
#ifdef R__GLOBALSTL
      typedef map<string,TClass*>                 IdMap_t;
#else
      typedef std::map<std::string,TClass*>       IdMap_t;
#endif
      typedef IdMap_t::key_type                   key_type;
      typedef IdMap_t::const_iterator             const_iterator;
      typedef IdMap_t::size_type                  size_type;
#ifdef R__WIN32
     // Window's std::map does NOT defined mapped_type
      typedef TClass*                             mapped_type;
#else
      typedef IdMap_t::mapped_type                mapped_type;
#endif

   private:
      IdMap_t fMap;

   public:
      void Add(const key_type &key, mapped_type &obj) {
         fMap[key] = obj;
      }
      mapped_type Find(const key_type &key) const {
         IdMap_t::const_iterator iter = fMap.find(key);
         mapped_type cl = 0;
         if (iter != fMap.end()) cl = iter->second;
         return cl;
      }
      void Remove(const key_type &key) { fMap.erase(key); }
#else
   private:
      TMap fMap;

   public:
      void Add(const char *key, TClass *&obj) {
         TObjString *realkey = new TObjString(key);
         fMap.Add(realkey, obj);
      }
      TClass* Find(const char *key) const {
         const TPair *a = (const TPair *)fMap.FindObject(key);
         if (a) return (TClass*) a->Value();
         return 0;
      }
      void Remove(const char *key) {
         TObjString realkey(key);
         TObject *actual = fMap.Remove(&realkey);
         delete actual;
      }
#endif
   };
}
IdMap_t *TClass::fgIdMap = new IdMap_t;

//______________________________________________________________________________
void TClass::AddClass(TClass *cl)
{
   // static: Add a class to the list and map of classes.

   if (!cl) return;
   gROOT->GetListOfClasses()->Add(cl);
   if (cl->GetTypeInfo()) {
      fgIdMap->Add(cl->GetTypeInfo()->name(),cl);
   }
}


//______________________________________________________________________________
void TClass::RemoveClass(TClass *oldcl)
{
   // static: Remove a class from the list and map of classes

   if (!oldcl) return;
   gROOT->GetListOfClasses()->Remove(oldcl);
   if (oldcl->GetTypeInfo()) {
      fgIdMap->Remove(oldcl->GetTypeInfo()->name());
   }
}

//______________________________________________________________________________
//______________________________________________________________________________

class TDumpMembers : public TMemberInspector {

public:
   TDumpMembers() { }
   void Inspect(TClass *cl, const char *parent, const char *name, const void *addr);
};

//______________________________________________________________________________
void TDumpMembers::Inspect(TClass *cl, const char *pname, const char *mname, const void *add)
{
   // Print value of member mname.
   //
   // This method is called by the ShowMembers() method for each
   // data member when object.Dump() is invoked.
   //
   //    cl    is the pointer to the current class
   //    pname is the parent name (in case of composed objects)
   //    mname is the data member name
   //    add   is the data member address

   const Int_t kvalue = 30;
#ifdef R__B64
   const Int_t ktitle = 50;
#else
   const Int_t ktitle = 42;
#endif
   const Int_t kline  = 1024;
   Int_t cdate = 0;
   Int_t ctime = 0;
   UInt_t *cdatime = 0;
   char line[kline];
   TDataMember *member = cl->GetDataMember(mname);
   if (!member) return;
   TDataType *membertype = member->GetDataType();
   Bool_t isdate = kFALSE;
   if (strcmp(member->GetName(),"fDatime") == 0 && strcmp(member->GetTypeName(),"UInt_t") == 0) {
      isdate = kTRUE;
   }
   Bool_t isbits = kFALSE;
   if (strcmp(member->GetName(),"fBits") == 0 && strcmp(member->GetTypeName(),"UInt_t") == 0) {
      isbits = kTRUE;
   }

   Int_t i;
   for (i = 0;i < kline; i++) line[i] = ' ';
   line[kline-1] = 0;
   sprintf(line,"%s%s ",pname,mname);
   i = strlen(line); line[i] = ' ';

   // Encode data value or pointer value
   char *pointer = (char*)add;
   char **ppointer = (char**)(pointer);

   if (member->IsaPointer()) {
      char **p3pointer = (char**)(*ppointer);
      if (!p3pointer)
         sprintf(&line[kvalue],"->0");
      else if (!member->IsBasic())
         sprintf(&line[kvalue],"->%lx ", (Long_t)p3pointer);
      else if (membertype) {
         if (!strcmp(membertype->GetTypeName(), "char")) {
            i = strlen(*ppointer);
            if (kvalue+i >= kline) i=kline-kvalue;
            Bool_t isPrintable = kTRUE;
            for (Int_t j = 0; j < i; j++) {
               if (!std::isprint((*ppointer)[j])) {
                  isPrintable = kFALSE;
                  break;
               }
            }
            if (isPrintable) {
               strncpy(line + kvalue, *ppointer, i);
               line[kvalue+i] = 0;
            } else {
               line[kvalue] = 0;
            }
         } else {
            strcpy(&line[kvalue], membertype->AsString(p3pointer));
         }
      } else if (!strcmp(member->GetFullTypeName(), "char*") ||
                 !strcmp(member->GetFullTypeName(), "const char*")) {
         i = strlen(*ppointer);
         if (kvalue+i >= kline) i=kline-kvalue;
         Bool_t isPrintable = kTRUE;
         for (Int_t j = 0; j < i; j++) {
            if (!std::isprint((*ppointer)[j])) {
               isPrintable = kFALSE;
               break;
            }
         }
         if (isPrintable) {
            strncpy(line + kvalue, *ppointer, i);
            line[kvalue+i] = 0;
         } else {
            line[kvalue] = 0;
         }
      } else {
         sprintf(&line[kvalue],"->%lx ", (Long_t)p3pointer);
      }
   } else if (membertype)
      if (isdate) {
         cdatime = (UInt_t*)pointer;
         TDatime::GetDateTime(cdatime[0],cdate,ctime);
         sprintf(&line[kvalue],"%d/%d",cdate,ctime);
      } else if (isbits) {
         sprintf(&line[kvalue],"0x%08x", *(UInt_t*)pointer);
      } else {
         strcpy(&line[kvalue], membertype->AsString(pointer));
      }
   else
      sprintf(&line[kvalue],"->%lx ", (Long_t)pointer);

   // Encode data member title
   if (isdate == kFALSE && strcmp(member->GetFullTypeName(), "char*") &&
       strcmp(member->GetFullTypeName(), "const char*")) {
      i = strlen(&line[0]); line[i] = ' ';
      Int_t lentit = strlen(member->GetTitle());
      if (lentit > 250-ktitle) lentit = 250-ktitle;
      strncpy(&line[ktitle],member->GetTitle(),lentit);
      line[ktitle+lentit] = 0;
   }
   Printf("%s", line);
}

THashTable* TClass::fgClassTypedefHash = 0;
THashTable* TClass::fgClassShortTypedefHash = 0;

//______________________________________________________________________________
//______________________________________________________________________________
//______________________________________________________________________________
TClass::TNameMapNode::TNameMapNode (const char* typedf, const char* orig)
  : TObjString (typedf),
    fOrigName (orig)
{
}

//______________________________________________________________________________

class TBuildRealData : public TMemberInspector {

private:
   void    *fRealDataObject;
   TClass  *fRealDataClass;

public:
   TBuildRealData(void *obj, TClass *cl)  {
      fRealDataObject = obj;
      fRealDataClass = cl;
   }
   void Inspect(TClass *cl, const char *parent, const char *name, const void *addr);
};

//______________________________________________________________________________
void TBuildRealData::Inspect(TClass* cl, const char* pname, const char* mname, const void* add)
{
   // This method is called from ShowMembers() via BuildRealdata().

   TDataMember* dm = cl->GetDataMember(mname);
   if (!dm) {
      return;
   }

   Bool_t isTransient = kFALSE;

   if (!dm->IsPersistent()) {
      // For the DataModelEvolution we need access to the transient member.
      // so we now record them in the list of RealData.
      isTransient = kTRUE;
   }

   char rname[512];
   strcpy(rname, pname);
   // Take into account cases like TPaveStats->TPaveText->TPave->TBox.
   // Check that member is in a derived class or an object in the class.
   if (cl != fRealDataClass) {
      if (!fRealDataClass->InheritsFrom(cl)) {
         char* dot = strchr(rname, '.');
         if (!dot) {
            return;
         }
         *dot = 0;
         if (!fRealDataClass->GetDataMember(rname)) {
            //could be a data member in a base class like in this example
            // class Event : public Data {
            //   class Data : public TObject {
            //     EventHeader fEvtHdr;
            //     class EventHeader {
            //       Int_t     fEvtNum;
            //       Int_t     fRun;
            //       Int_t     fDate;
            //       EventVertex fVertex;
            //       class EventVertex {
            //         EventTime  fTime;
            //         class EventTime {
            //           Int_t     fSec;
            //           Int_t     fNanoSec;
            if (!fRealDataClass->GetBaseDataMember(rname)) {
               return;
            }
         }
         *dot = '.';
      }
   }
   strcat(rname, mname);
   Long_t offset = Long_t(((Long_t) add) - ((Long_t) fRealDataObject));

   if (dm->IsaPointer()) {
      // Data member is a pointer.
      if (!dm->IsBasic()) {
         // Pointer to class object.
         TRealData* rd = new TRealData(rname, offset, dm);
         if (isTransient) { rd->SetBit(TRealData::kTransient); };
         fRealDataClass->GetListOfRealData()->Add(rd);
      } else {
         // Pointer to basic data type.
         TRealData* rd = new TRealData(rname, offset, dm);
         if (isTransient) { rd->SetBit(TRealData::kTransient); };
         fRealDataClass->GetListOfRealData()->Add(rd);
      }
   } else {
      // Data Member is a basic data type.
      TRealData* rd = new TRealData(rname, offset, dm);
      if (isTransient) { rd->SetBit(TRealData::kTransient); };
      if (!dm->IsBasic()) {
         rd->SetIsObject(kTRUE);

         // Make sure that BuildReadData is called for any abstract
         // bases classes involved in this object, i.e for all the
         // classes composing this object (base classes, type of
         // embedded object and same for their data members).
         //
         TClass* dmclass = TClass::GetClass(dm->GetTypeName(), kTRUE, isTransient);
         if (!dmclass) {
            dmclass = TClass::GetClass(dm->GetTrueTypeName(), kTRUE, isTransient);
         }
         if (dmclass) {
            if (dmclass->Property()) {
               if (dmclass->Property() & kIsAbstract) {
                  fprintf(stderr, "TBuildRealDataRecursive::Inspect(): data member class: '%s'  is abstract.\n", dmclass->GetName());
               }
            }
            if ((dmclass != cl) && !dm->IsaPointer()) {
               if (dmclass->GetCollectionProxy()) {
                  TClass* valcl = dmclass->GetCollectionProxy()->GetValueClass();
                  if (valcl) valcl->BuildRealData();
               } else {
                  dmclass->BuildRealData(const_cast<void*>(add), isTransient);
               }
            }
         }
      }
      fRealDataClass->GetListOfRealData()->Add(rd);
   }
}

//______________________________________________________________________________
//______________________________________________________________________________
//______________________________________________________________________________

//______________________________________________________________________________
class TAutoInspector : public TMemberInspector {

public:
   Int_t     fCount;
   TBrowser *fBrowser;

   TAutoInspector(TBrowser *b) { fBrowser = b; fCount = 0; }
   virtual ~TAutoInspector() { }
   virtual void Inspect(TClass *cl, const char *parent, const char *name, const void *addr);
};

//______________________________________________________________________________
void TAutoInspector::Inspect(TClass *cl, const char *tit, const char *name,
                             const void *addr)
{
   // This method is called from ShowMembers() via AutoBrowse().

   if(tit && strchr(tit,'.'))    return ;
   if (fCount && !fBrowser) return;

   TString ts;

   if (!cl) return;
   //if (*(cl->GetName()) == 'T') return;
   if (*name == '*') name++;
   int ln = strcspn(name,"[ ");
   TString iname(name,ln);

   ClassInfo_t *classInfo = cl->GetClassInfo();
   if (!classInfo)               return;

   //              Browse data members
   DataMemberInfo_t *m = gCint->DataMemberInfo_Factory(classInfo);
   TString mname;

   int found=0;
   while (gCint->DataMemberInfo_Next(m)) {    // MemberLoop
      mname = gCint->DataMemberInfo_Name(m);
      mname.ReplaceAll("*","");
      if ((found = (iname==mname))) break;
   }
   assert(found);

   // we skip: non static members and non objects
   //  - the member G__virtualinfo inserted by the CINT RTTI system

   //Long_t prop = m.Property() | m.Type()->Property();
   Long_t prop = gCint->DataMemberInfo_Property(m) | gCint->DataMemberInfo_TypeProperty(m);
   if (prop & G__BIT_ISSTATIC)           return;
   if (prop & G__BIT_ISFUNDAMENTAL)      return;
   if (prop & G__BIT_ISENUM)             return;
   if (mname == "G__virtualinfo")        return;

   int  size = sizeof(void*);

   int nmax = 1;
   if (prop & G__BIT_ISARRAY) {
      for (int dim = 0; dim < gCint->DataMemberInfo_ArrayDim(m); dim++) nmax *= gCint->DataMemberInfo_MaxIndex(m,dim);
   }

   std::string clmName(TClassEdit::ShortType(gCint->DataMemberInfo_TypeName(m),
                                             TClassEdit::kDropTrailStar) );
   TClass * clm = TClass::GetClass(clmName.c_str());
   R__ASSERT(clm);
   if (!(prop&G__BIT_ISPOINTER)) {
      size = clm->Size();
      if (size==0) size = gCint->DataMemberInfo_TypeSize(m);
   }


   gCint->DataMemberInfo_Delete(m);
   TVirtualCollectionProxy *proxy = clm->GetCollectionProxy();

   for(int i=0; i<nmax; i++) {

      char *ptr = (char*)addr + i*size;

      void *obj = (prop&G__BIT_ISPOINTER) ? *((void**)ptr) : (TObject*)ptr;

      if (!obj)           continue;

      fCount++;
      if (!fBrowser)      return;

      TString bwname;
      TClass *actualClass = clm->GetActualClass(obj);
      if (clm->IsTObject()) {
         TObject *tobj = (TObject*)clm->DynamicCast(TObject::Class(),obj);
         bwname = tobj->GetName();
      } else {
         bwname = actualClass->GetName();
         bwname += "::";
         bwname += mname;
      }

      if (!clm->IsTObject() ||
          bwname.Length()==0 ||
          strcmp(bwname.Data(),actualClass->GetName())==0) {
         bwname = name;
         int l = strcspn(bwname.Data(),"[ ");
         if (l<bwname.Length() && bwname[l]=='[') {
            char cbuf[12]; sprintf(cbuf,"[%02d]",i);
            ts.Replace(0,999,bwname,l);
            ts += cbuf;
            bwname = (const char*)ts;
         }
      }

      if (proxy==0) {

         fBrowser->Add(obj,clm,bwname);

      } else {
         TClass *valueCl = proxy->GetValueClass();

         if (valueCl==0) {

            fBrowser->Add( obj, clm, bwname );

         } else {
            TVirtualCollectionProxy::TPushPop env(proxy, obj);
            TClass *actualCl = 0;

            int sz = proxy->Size();

            char fmt[] = {"#%09d"};
            fmt[3]  = '0'+(int)log10(double(sz))+1;
            char buf[20];
            for (int ii=0;ii<sz;ii++) {
               void *p = proxy->At(ii);

               if (proxy->HasPointers()) {
                  p = *((void**)p);
                  if(!p) continue;
                  actualCl = valueCl->GetActualClass(p);
                  p = actualCl->DynamicCast(valueCl,p,0);
               }
               fCount++;
               sprintf(buf,fmt,ii);
               ts = bwname;
               ts += buf;
               fBrowser->Add( p, actualCl, ts );
            }
         }
      }
   }
}

//______________________________________________________________________________
//______________________________________________________________________________
//______________________________________________________________________________

ClassImp(TClass)

//______________________________________________________________________________
TClass::TClass() :
   TDictionary(),
   fStreamerInfo(0), fConversionStreamerInfo(0), fRealData(0),
   fBase(0), fData(0), fMethod(0), fAllPubData(0), fAllPubMethod(0),
   fClassMenuList(0),
   fDeclFileName(""), fImplFileName(""), fDeclFileLine(0), fImplFileLine(0),
   fInstanceCount(0), fOnHeap(0),
   fCheckSum(0), fCollectionProxy(0), fClassVersion(0), fClassInfo(0),
   fTypeInfo(0), fShowMembers(0), fInterShowMembers(0),
   fStreamer(0), fIsA(0), fGlobalIsA(0), fIsAMethod(0),
   fNew(0), fNewArray(0), fDelete(0), fDeleteArray(0),
   fDestructor(0), fDirAutoAdd(0), fSizeof(-1),
   fVersionUsed(kFALSE), fProperty(0),
   fInterStreamer(0), fOffsetStreamer(0), fStreamerType(kNone),
   fCurrentInfo(0), fRefStart(0), fRefProxy(0),
   fSchemaRules(0)
{
   // Default ctor.

   fDeclFileLine   = -2;    // -2 for standalone TClass (checked in dtor)
}

//______________________________________________________________________________
TClass::TClass(const char *name, Bool_t silent) :
   TDictionary(name),
   fStreamerInfo(0), fConversionStreamerInfo(0), fRealData(0),
   fBase(0), fData(0), fMethod(0), fAllPubData(0), fAllPubMethod(0),
   fClassMenuList(0),
   fDeclFileName(""), fImplFileName(""), fDeclFileLine(0), fImplFileLine(0),
   fInstanceCount(0), fOnHeap(0),
   fCheckSum(0), fCollectionProxy(0), fClassVersion(0), fClassInfo(0),
   fTypeInfo(0), fShowMembers(0), fInterShowMembers(0),
   fStreamer(0), fIsA(0), fGlobalIsA(0), fIsAMethod(0),
   fNew(0), fNewArray(0), fDelete(0), fDeleteArray(0),
   fDestructor(0), fDirAutoAdd(0), fSizeof(-1),
   fVersionUsed(kFALSE), fProperty(0),
   fInterStreamer(0), fOffsetStreamer(0), fStreamerType(kNone),
   fCurrentInfo(0), fRefStart(0), fRefProxy(0),
   fSchemaRules(0)
{
   // Create a TClass object. This object contains the full dictionary
   // of a class. It has list to baseclasses, datamembers and methods.
   // Use this ctor to create a standalone TClass object. Most useful
   // to get a TClass interface to an interpreted class. Used by TTabCom.
   // Normally you would use TClass::GetClass("class") to get access to a
   // TClass object for a certain class.

   if (!gROOT)
      ::Fatal("TClass::TClass", "ROOT system not initialized");

   fDeclFileLine   = -2;    // -2 for standalone TClass (checked in dtor)

   SetBit(kLoading);
   if (!gInterpreter)
      ::Fatal("TClass::TClass", "gInterpreter not initialized");

   gInterpreter->SetClassInfo(this);   // sets fClassInfo pointer
   if (!fClassInfo) {
      gInterpreter->InitializeDictionaries();
      gInterpreter->SetClassInfo(this);
   }
   if (!silent && !fClassInfo && fName.First('@')==kNPOS) 
      ::Warning("TClass::TClass", "no dictionary for class %s is available", name);
   ResetBit(kLoading);

   if (fClassInfo) SetTitle(gCint->ClassInfo_Title(fClassInfo));
   fConversionStreamerInfo = 0;
}

//______________________________________________________________________________
TClass::TClass(const char *name, Version_t cversion,
               const char *dfil, const char *ifil, Int_t dl, Int_t il, Bool_t silent) :
   TDictionary(name),
   fStreamerInfo(0), fConversionStreamerInfo(0), fRealData(0),
   fBase(0), fData(0), fMethod(0), fAllPubData(0), fAllPubMethod(0),
   fClassMenuList(0),
   fDeclFileName(""), fImplFileName(""), fDeclFileLine(0), fImplFileLine(0),
   fInstanceCount(0), fOnHeap(0),
   fCheckSum(0), fCollectionProxy(0), fClassVersion(0), fClassInfo(0),
   fTypeInfo(0), fShowMembers(0), fInterShowMembers(0),
   fStreamer(0), fIsA(0), fGlobalIsA(0), fIsAMethod(0),
   fNew(0), fNewArray(0), fDelete(0), fDeleteArray(0),
   fDestructor(0), fDirAutoAdd(0), fSizeof(-1),
   fVersionUsed(kFALSE), fProperty(0),
   fInterStreamer(0), fOffsetStreamer(0), fStreamerType(kNone),
   fCurrentInfo(0), fRefStart(0), fRefProxy(0),
   fSchemaRules(0)
{
   // Create a TClass object. This object contains the full dictionary
   // of a class. It has list to baseclasses, datamembers and methods.
   Init(name,cversion, 0, 0, 0, dfil, ifil, dl, il,silent);
   SetBit(kUnloaded);
}

//______________________________________________________________________________
TClass::TClass(const char *name, Version_t cversion,
               const type_info &info, TVirtualIsAProxy *isa,
               ShowMembersFunc_t showmembers,
               const char *dfil, const char *ifil, Int_t dl, Int_t il,
               Bool_t silent) :
   TDictionary(name),
   fStreamerInfo(0), fConversionStreamerInfo(0), fRealData(0),
   fBase(0), fData(0), fMethod(0), fAllPubData(0), fAllPubMethod(0),
   fClassMenuList(0),
   fDeclFileName(""), fImplFileName(""), fDeclFileLine(0), fImplFileLine(0),
   fInstanceCount(0), fOnHeap(0),
   fCheckSum(0), fCollectionProxy(0), fClassVersion(0), fClassInfo(0),
   fTypeInfo(0), fShowMembers(0), fInterShowMembers(0),
   fStreamer(0), fIsA(0), fGlobalIsA(0), fIsAMethod(0),
   fNew(0), fNewArray(0), fDelete(0), fDeleteArray(0),
   fDestructor(0), fDirAutoAdd(0), fSizeof(-1),
   fVersionUsed(kFALSE), fProperty(0),
   fInterStreamer(0), fOffsetStreamer(0), fStreamerType(kNone),
   fCurrentInfo(0), fRefStart(0), fRefProxy(0),
   fSchemaRules(0)
{
   // Create a TClass object. This object contains the full dictionary
   // of a class. It has list to baseclasses, datamembers and methods.

   // use info
   Init(name, cversion, &info, isa, showmembers, dfil, ifil, dl, il, silent);
}

//______________________________________________________________________________
void TClass::ForceReload (TClass* oldcl)
{
   // we found at least one equivalent.
   // let's force a reload

   TClass::RemoveClass(oldcl);

   if (oldcl->CanIgnoreTObjectStreamer()) {
      IgnoreTObjectStreamer();
   }

   TVirtualStreamerInfo *info;
   TIter next(oldcl->GetStreamerInfos());
   while ((info = (TVirtualStreamerInfo*)next())) {
      info->Clear("build");
      info->SetClass(this);
      fStreamerInfo->AddAtAndExpand(info,info->GetClassVersion());
   }
   oldcl->GetStreamerInfos()->Clear();

   oldcl->ReplaceWith(this);
   delete oldcl;
}
//______________________________________________________________________________
void TClass::Init(const char *name, Version_t cversion,
                  const type_info *typeinfo, TVirtualIsAProxy *isa,
                  ShowMembersFunc_t showmembers,
                  const char *dfil, const char *ifil, Int_t dl, Int_t il,
                  Bool_t silent)
{
   // Initialize a TClass object. This object contains the full dictionary
   // of a class. It has list to baseclasses, datamembers and methods.
   if (!gROOT)
      ::Fatal("TClass::TClass", "ROOT system not initialized");

   SetName(name);
   fClassVersion   = cversion;
   fDeclFileName   = dfil ? dfil : "";
   fImplFileName   = ifil ? ifil : "";
   fDeclFileLine   = dl;
   fImplFileLine   = il;
   fTypeInfo       = typeinfo;
   fIsA            = isa;
   if ( fIsA ) fIsA->SetClass(this);
   fShowMembers    = showmembers;
   fStreamerInfo   = new TObjArray(fClassVersion+2+10,-1); // +10 to read new data by old
   fProperty       = -1;

   ResetInstanceCount();

   TClass *oldcl = (TClass*)gROOT->GetListOfClasses()->FindObject(name);

   if (oldcl && oldcl->TestBit(kLoading)) {
      // Do not recreate a class while it is already being created!
      return;
   }

   if (oldcl) {
      gROOT->RemoveClass(oldcl);
      // move the StreamerInfo immediately so that there are
      // properly updated!

      if (oldcl->CanIgnoreTObjectStreamer()) {
         IgnoreTObjectStreamer();
      }
      TVirtualStreamerInfo *info;

      TIter next(oldcl->GetStreamerInfos());
      while ((info = (TVirtualStreamerInfo*)next())) {
         // We need to force a call to BuildOld
         info->Clear("build");
         info->SetClass(this);
         fStreamerInfo->AddAtAndExpand(info,info->GetClassVersion());
      }
      oldcl->GetStreamerInfos()->Clear();

   }

   SetBit(kLoading);
   // Advertise ourself as the loading class for this class name
   TClass::AddClass(this);

   Bool_t isStl = kFALSE;

   if (!fClassInfo) {
      Bool_t shouldLoad = kFALSE;
      isStl = TClassEdit::IsSTLCont(name);

      if (gInterpreter->CheckClassInfo(name)) shouldLoad = kTRUE;
      else if (fImplFileLine>=0) {
         // If the TClass is being generated from a ROOT dictionary,
         // eventhough we do not seem to have a CINT dictionary for
         // the class, we will will try to load it anyway UNLESS
         // the class is an STL container (or string).
         // This is because we do not expect the CINT dictionary
         // to be present for all STL classes (and we can handle
         // the lack of CINT dictionary in that cases).

         shouldLoad = ! isStl;
      }

      if (shouldLoad) {
         if (!gInterpreter)
            ::Fatal("TClass::TClass", "gInterpreter not initialized");

         gInterpreter->SetClassInfo(this);   // sets fClassInfo pointer
         if (!fClassInfo) {
            gInterpreter->InitializeDictionaries();
            gInterpreter->SetClassInfo(this);
            if (IsZombie()) {
               TClass::RemoveClass(this);
               return;
            }
         }
      }
   }
   if (!silent && !fClassInfo && !isStl && fName.First('@')==kNPOS)
      ::Warning("TClass::TClass", "no dictionary for class %s is available", name);

   fgClassCount++;
   SetUniqueID(fgClassCount);

   // Make the typedef-expanded -> original hash table entries.
   // There may be several entries for any given key.
   // We only make entries if the typedef-expanded name
   // is different from the original name.
   TString resolvedThis;
   if (strchr (name, '<')) {
      resolvedThis = TClassEdit::ResolveTypedef (name, kTRUE);
      if (resolvedThis != name) {
         if (!fgClassTypedefHash) {
            fgClassTypedefHash = new THashTable (100, 5);
            fgClassTypedefHash->SetOwner (kTRUE);
         }

         fgClassTypedefHash->Add (new TNameMapNode (resolvedThis, name));
         SetBit (kHasNameMapNode);
      }

      TString resolvedShort =
       TClassEdit::ResolveTypedef
         (TClassEdit::ShortType(name,
                                TClassEdit::kDropStlDefault).c_str(),
          kTRUE);
      if (resolvedShort != name) {
         if (!fgClassShortTypedefHash) {
            fgClassShortTypedefHash = new THashTable (100, 5);
            fgClassShortTypedefHash->SetOwner (kTRUE);
         }

         fgClassShortTypedefHash->Add (new TNameMapNode (resolvedShort, name));
         SetBit (kHasNameMapNode);
      }
   }

   //In case a class with the same name had been created by TVirtualStreamerInfo
   //we must delete the old class, importing only the StreamerInfo structure
   //from the old dummy class.
   if (oldcl) {

      oldcl->ReplaceWith(this);
      delete oldcl;

   } else if (resolvedThis.Length() > 0 && fgClassTypedefHash) {

      // Check for existing equivalent.

      if (resolvedThis != name) {
         oldcl = (TClass*)gROOT->GetListOfClasses()->FindObject(resolvedThis);
         if (oldcl && oldcl != this)
            ForceReload (oldcl);
      }
      TIter next( fgClassTypedefHash->GetListForObject (resolvedThis) );
      while ( TNameMapNode* htmp = static_cast<TNameMapNode*> (next()) ) {
         if (resolvedThis != htmp->String()) continue;
         oldcl = (TClass*)gROOT->GetListOfClasses()->FindObject(htmp->fOrigName); // gROOT->GetClass (htmp->fOrigName, kFALSE);
         if (oldcl && oldcl != this) {
            ForceReload (oldcl);
         }
      }
   }
   if (fClassInfo) SetTitle(gCint->ClassInfo_Title(fClassInfo));

   ResetBit(kLoading);

   Int_t stl = TClassEdit::IsSTLCont(GetName(), 0);

   if ( stl || !strncmp(GetName(),"stdext::hash_",13) || !strncmp(GetName(),"__gnu_cxx::hash_",16) ) {
      fCollectionProxy = TVirtualStreamerInfo::Factory()->GenEmulatedProxy( GetName() );
      fSizeof = fCollectionProxy->Sizeof();
      if (fStreamer==0) {
         fStreamer =  TVirtualStreamerInfo::Factory()->GenEmulatedClassStreamer( GetName() );
      }
   }

}

//______________________________________________________________________________
TClass::TClass(const TClass& cl) :
  TDictionary(cl),
  fStreamerInfo(cl.fStreamerInfo),
  fConversionStreamerInfo(cl.fConversionStreamerInfo),
  fRealData(cl.fRealData),
  fBase(cl.fBase),
  fData(cl.fData),
  fMethod(cl.fMethod),
  fAllPubData(cl.fAllPubData),
  fAllPubMethod(cl.fAllPubMethod),
  fClassMenuList(0),
  fDeclFileName(cl.fDeclFileName),
  fImplFileName(cl.fImplFileName),
  fDeclFileLine(cl.fDeclFileLine),
  fImplFileLine(cl.fImplFileLine),
  fInstanceCount(cl.fInstanceCount),
  fOnHeap(cl.fOnHeap),
  fCheckSum(cl.fCheckSum),
  fCollectionProxy(cl.fCollectionProxy),
  fClassVersion(cl.fClassVersion),
  fClassInfo(cl.fClassInfo),
  fContextMenuTitle(cl.fContextMenuTitle),
  fTypeInfo(cl.fTypeInfo),
  fShowMembers(cl.fShowMembers),
  fInterShowMembers(cl.fInterShowMembers),
  fStreamer(cl.fStreamer),
  fSharedLibs(cl.fSharedLibs),
  fIsA(cl.fIsA),
  fGlobalIsA(cl.fGlobalIsA),
  fIsAMethod(cl.fIsAMethod),
  fNew(cl.fNew),
  fNewArray(cl.fNewArray),
  fDelete(cl.fDelete),
  fDeleteArray(cl.fDeleteArray),
  fDestructor(cl.fDestructor),
  fDirAutoAdd(cl.fDirAutoAdd),
  fSizeof(cl.fSizeof),
  fVersionUsed(cl.fVersionUsed),
  fProperty(cl.fProperty),
  fInterStreamer(cl.fInterStreamer),
  fOffsetStreamer(cl.fOffsetStreamer),
  fStreamerType(cl.fStreamerType),
  fCurrentInfo(cl.fCurrentInfo),
  fRefStart(cl.fRefStart),
  fRefProxy(cl.fRefProxy),
  fSchemaRules(cl.fSchemaRules)
{
   //copy constructor
   
   R__ASSERT(0 /* TClass Object are not copyable */ );
}

//______________________________________________________________________________
TClass& TClass::operator=(const TClass& cl)
{
   //assignement operator
   if(this!=&cl) {
      R__ASSERT(0 /* TClass Object are not copyable */ );
   }
   return *this;
}

//______________________________________________________________________________
TClass::~TClass()
{
   // TClass dtor. Deletes all list that might have been created.

   // Remove from the typedef hashtables.
   if (fgClassTypedefHash && TestBit (kHasNameMapNode)) {
      TString resolvedThis = TClassEdit::ResolveTypedef (GetName(), kTRUE);
      TIter next (fgClassTypedefHash->GetListForObject (resolvedThis));
      while ( TNameMapNode* htmp = static_cast<TNameMapNode*> (next()) ) {
         if (resolvedThis == htmp->String() && htmp->fOrigName == GetName()) {
            fgClassTypedefHash->Remove (htmp);
            delete htmp;
            break;
         }
      }
   }
   if (fgClassShortTypedefHash && TestBit (kHasNameMapNode)) {
      TString resolvedShort =
       TClassEdit::ResolveTypedef
         (TClassEdit::ShortType(GetName(),
                                TClassEdit::kDropStlDefault).c_str(),
          kTRUE);
      TIter next (fgClassShortTypedefHash->GetListForObject (resolvedShort));
      while ( TNameMapNode* htmp = static_cast<TNameMapNode*> (next()) ) {
         if (resolvedShort == htmp->String() && htmp->fOrigName == GetName()) {
            fgClassShortTypedefHash->Remove (htmp);
            delete htmp;
            break;
         }
      }
   }
   

   // Not owning lists, don't call Delete()
   // But this still need to be done first because the TList desctructor
   // does access the object contained (via GetObject()->TestBit(kCanDelete))
   delete fStreamer;       fStreamer    =0;
   delete fAllPubData;     fAllPubData  =0;
   delete fAllPubMethod;   fAllPubMethod=0;

   if (fRefStart) {
      // Inform the TClassRef object that we are going away.
      fRefStart->ListReset();
      fRefStart = 0;
   }
   if (fBase)
      fBase->Delete();
   delete fBase;   fBase=0;

   if (fData)
      fData->Delete();
   delete fData;   fData = 0;

   if (fMethod)
      fMethod->Delete();
   delete fMethod;   fMethod=0;

   if (fRealData)
      fRealData->Delete();
   delete fRealData;  fRealData=0;

   if (fStreamerInfo)
      fStreamerInfo->Delete();
   delete fStreamerInfo; fStreamerInfo=0;

   if (fDeclFileLine >= -1)
      TClass::RemoveClass(this);

   gCint->ClassInfo_Delete(fClassInfo);  
   fClassInfo=0;

   if (fClassMenuList)
      fClassMenuList->Delete();
   delete fClassMenuList; fClassMenuList=0;

   if ( fInterStreamer ) gCint->CallFunc_Delete((CallFunc_t*)fInterStreamer);
   fInterStreamer=0;

   if (fInterShowMembers) gCint->CallFunc_Delete(fInterShowMembers);

   if ( fIsA ) delete fIsA;

   if ( fRefProxy ) fRefProxy->Release();
   fRefProxy = 0;

   delete fStreamer;
   delete fCollectionProxy;
   delete fIsAMethod;
   delete fSchemaRules;
   delete fConversionStreamerInfo;
}

//------------------------------------------------------------------------------
void TClass::AdoptSchemaRules( ROOT::TSchemaRuleSet *rules )
{
   // Adopt a new set of Data Model Evolution rules.

   delete fSchemaRules;
   fSchemaRules = rules;
   fSchemaRules->SetClass( this );
}

//------------------------------------------------------------------------------
const ROOT::TSchemaRuleSet* TClass::GetSchemaRules() const
{
   // Return the set of the schema rules if any.
   return fSchemaRules;
}

//------------------------------------------------------------------------------
ROOT::TSchemaRuleSet* TClass::GetSchemaRules(Bool_t create)
{
   // Return the set of the schema rules if any.
   // If create is true, create an empty set
   if (create && fSchemaRules == 0) {
      fSchemaRules = new ROOT::TSchemaRuleSet();
      fSchemaRules->SetClass( this );
   }
   return fSchemaRules;
}

//______________________________________________________________________________
void TClass::AddImplFile(const char* filename, int line) {

   // Currently reset the implementation file and line.
   // In the close future, it will actually add this file and line
   // to a "list" of implementation files.

   fImplFileName = filename;
   fImplFileLine = line;
}

//______________________________________________________________________________
void TClass::AddRef(TClassRef *ref)
{
   // Register a TClassRef object which points to this TClass object.
   // When this TClass object is deleted, 'ref' will be 'Reset'.

   if (fRefStart==0) {
      fRefStart = ref;
   } else {
      fRefStart->fPrevious = ref;
      ref->fNext = fRefStart;
      fRefStart = ref;
   }
}

//______________________________________________________________________________
Int_t TClass::AutoBrowse(TObject *obj, TBrowser *b)
{
   // Browse external object inherited from TObject.
   // It passes through inheritance tree and calls TBrowser::Add
   // in appropriate cases. Static function.

   if (!obj) return 0;

   char cbuf[1000]; *cbuf=0;

   TAutoInspector insp(b);
   obj->ShowMembers(insp,cbuf);
   return insp.fCount;
}

//______________________________________________________________________________
Int_t TClass::Browse(void *obj, TBrowser *b) const
{
   // Browse objects of of the class described by this TClass object.

   if (!obj) return 0;

   TClass *actual = GetActualClass(obj);
   if (IsTObject()) {
      // Call TObject::Browse.

      if (!fInterStreamer) {
         CalculateStreamerOffset();
      }
      TObject* realTObject = (TObject*)((size_t)obj + fOffsetStreamer);
      realTObject->Browse(b);
      return 1;
   } else if (actual != this) {
      return actual->Browse(obj, b);
   } else if (GetCollectionProxy()) {

      // do something useful.

   } else {
      char cbuf[1000]; *cbuf=0;

      TAutoInspector insp(b);
      CallShowMembers(obj,insp,cbuf,0);
      return insp.fCount;
   }

   return 0;
}

//______________________________________________________________________________
void TClass::Browse(TBrowser *b)
{
   // This method is called by a browser to get the class information.

   if (!fClassInfo) return;

   if (b) {
      if (!fRealData) BuildRealData();

      b->Add(GetListOfDataMembers(), "Data Members");
      b->Add(GetListOfRealData(), "Real Data Members");
      b->Add(GetListOfMethods(), "Methods");
      b->Add(GetListOfBases(), "Base Classes");
   }
}

//______________________________________________________________________________
void TClass::BuildRealData(void* pointer, Bool_t isTransient)
{
   // Build a full list of persistent data members.
   // Scans the list of all data members in the class itself and also
   // in all base classes. For each persistent data member, inserts a
   // TRealData object in the list fRealData.
   //
   // If pointer is not 0, uses the object at pointer
   // otherwise creates a temporary object of this class.

   // Only do this once.
   if (fRealData) {
      return;
   }

   // Handle emulated classes and STL containers specially.
   if (!fClassInfo || TClassEdit::IsSTLCont(GetName(), 0)) {
      // We are an emulated class or an STL container.
      fRealData = new TList;
      BuildEmulatedRealData("", 0, this);
      return;
   }

   void* realDataObject = pointer;

   // If we are not given an object, and the class
   // is abstract (so that we cannot make one), give up.
   if ((!pointer) && (Property() & kIsAbstract)) {
      return;
   }

   // If we are not given an object, make one.
   // Note: We handle singletons carefully.
   if (!realDataObject) {
      if (!strcmp(GetName(), "TROOT")) {
         realDataObject = gROOT;
      } else if (!strcmp(GetName(), "TGWin32")) {
         realDataObject = gVirtualX;
      } else if (!strcmp(GetName(), "TGQt")) {
         realDataObject = gVirtualX;
      } else {
         realDataObject = New();
      }
   }

   // The following statement will recursively call
   // all the subclasses of this class.
   if (realDataObject) {
      char parent[256];
      parent[0] = 0;
      fRealData = new TList;

      TBuildRealData brd(realDataObject, this);

      // CallShowMember will force a call to InheritsFrom, which indirectly
      // calls TClass::GetClass.  It forces the loading of new typedefs in 
      // case some of them were not yet loaded.
      if ( ! CallShowMembers(realDataObject, brd, parent) ) {
         if ( isTransient ) {
            // This is a transient data member, so it is probably fine to not have 
            // access to its content.  However let's no mark it as definitively setup,
            // since another class might use this class for a persistent data member and
            // in this case we really want the error message.
            delete fRealData;
            fRealData = 0;            
         } else {
            Error("BuildRealData", "Cannot find any ShowMembers function for %s!", GetName());
         }
      }

      // Take this opportunity to build the real data for base classes.
      // In case one base class is abstract, it would not be possible later
      // to create the list of real data for this abstract class.
      TBaseClass* base = 0;
      TIter next(GetListOfBases());
      while ((base = (TBaseClass*) next())) {
         if (base->IsSTLContainer()) {
            continue;
         }
         TClass* c = base->GetClassPointer();
         if (c) {
            c->BuildRealData(((char*) realDataObject) + base->GetDelta());
         }
      }
   }

   // Clean up any allocated temporary object.
   if (!pointer && realDataObject && (realDataObject != gROOT) && (realDataObject != gVirtualX)) {
      Int_t delta = GetBaseClassOffset(TObject::Class());
      if (delta >= 0) {
         TObject* tobj = (TObject*) (((char*) realDataObject) + delta);
         tobj->SetBit(kZombie); //this info useful in object destructor
         delete tobj;
         tobj = 0;
      } else {
         Destructor(realDataObject);
         realDataObject = 0;
      }
   }
}

//______________________________________________________________________________
void TClass::BuildEmulatedRealData(const char *name, Long_t offset, TClass *cl)
{
   // Build the list of real data for an emulated class

   TIter next(GetStreamerInfo()->GetElements());
   TStreamerElement *element;
   while ((element = (TStreamerElement*)next())) {
      Int_t etype    = element->GetType();
      Long_t eoffset = element->GetOffset();
      TClass *cle    = element->GetClassPointer();
      if (etype == TVirtualStreamerInfo::kTObject ||
          etype == TVirtualStreamerInfo::kTNamed ||
          etype == TVirtualStreamerInfo::kBase) {
         //base class
         if (cle) cle->BuildEmulatedRealData(name,offset+eoffset,cl);
      } else if (etype == TVirtualStreamerInfo::kObject || etype == TVirtualStreamerInfo::kAny) {
         //member class
         TRealData *rd = new TRealData(Form("%s%s",name,element->GetFullName()),offset+eoffset,0);
         if (gDebug > 0) printf(" Class: %s, adding TRealData=%s, offset=%ld\n",cl->GetName(),rd->GetName(),rd->GetThisOffset());
         cl->GetListOfRealData()->Add(rd);
         TString rdname(Form("%s%s.",name,element->GetFullName()));
         if (cle) cle->BuildEmulatedRealData(rdname,offset+eoffset,cl);
      } else {
         //others
         TString rdname(Form("%s%s",name,element->GetFullName()));
         TRealData *rd = new TRealData(rdname,offset+eoffset,0);
         if (gDebug > 0) printf(" Class: %s, adding TRealData=%s, offset=%ld\n",cl->GetName(),rd->GetName(),rd->GetThisOffset());
         cl->GetListOfRealData()->Add(rd);
      }
      //if (fClassInfo==0 && element->IsBase()) {
      //   if (fBase==0) fBase = new TList;
      //   TClass *base = element->GetClassPointer();
      //   fBase->Add(new TBaseClass(this, cl, eoffset));
      //}
   }
}


//______________________________________________________________________________
void TClass::CalculateStreamerOffset() const
{
   // Calculate the offset between an object of this class to
   // its base class TObject. The pointer can be adjusted by
   // that offset to access any virtual method of TObject like
   // Streamer() and ShowMembers().
   if (!fInterStreamer && fClassInfo) {
      CallFunc_t *f  = gCint->CallFunc_Factory();
      gCint->CallFunc_SetFuncProto(f,fClassInfo,"Streamer","TBuffer&",&fOffsetStreamer);
      fInterStreamer = f;
      fOffsetStreamer = const_cast<TClass*>(this)->GetBaseClassOffset(TObject::Class());
   }
}


//______________________________________________________________________________
Bool_t TClass::CallShowMembers(void* obj, TMemberInspector &insp, char *parent,
                               Int_t isATObject) const
{
   // Call ShowMembers() on the obj of this class type, passing insp and parent.
   // isATObject is -1 if unknown, 0 if it is not a TObject, and 1 if it is a TObject.
   // The function returns whether it was able to call ShowMembers().

   if (fShowMembers) {
      // This should always works since 'pointer' should be pointing
      // to an object of the actual type of this TClass object.
      fShowMembers(obj, insp, parent);
      return kTRUE;
   } else {

      if (isATObject == -1) {
         // Force a call to InheritsFrom. This function indirectly
         // calls TClass::GetClass.  It forces the loading of new
         // typedefs in case some of them were not yet loaded.
         isATObject = (Int_t) (InheritsFrom(TObject::Class()));
      }

      if (isATObject == 1) {
         // We have access to the TObject interface, so let's use it.
         if (!fInterStreamer) {
            CalculateStreamerOffset();
         }
         TObject* realTObject = (TObject*)((size_t)obj + fOffsetStreamer);
         realTObject->ShowMembers(insp, parent);
         return kTRUE;
      } else if (fClassInfo) {
         
         // Always call ShowMembers via the interpreter. A direct call
         // like:
         //
         //      realDataObject->ShowMembers(brd, parent);
         //
         // will not work if the class derives from TObject but does not
         // have TObject as the leftmost base class.
         //

         if (!fInterShowMembers) {
            CallFunc_t* ism = gCint->CallFunc_Factory();
            Long_t offset = 0;
            
            R__LOCKGUARD2(gCINTMutex);
            gCint->CallFunc_SetFuncProto(ism,fClassInfo,"ShowMembers", "TMemberInspector&,char*", &offset);
            if (fInterStreamer && offset != fOffsetStreamer) {
               Error("CallShowMembers", "Logic Error: offset for Streamer() and ShowMembers() differ!");
               fInterShowMembers = 0;
               return kFALSE;
            }

            fInterShowMembers = ism;
         }
         if (!gCint->CallFunc_IsValid(fInterShowMembers)) {
            if (strcmp(GetName(), "string") == 0) {
               // For std::string we know that we do not have a ShowMembers
               // function and that it's okay.
               return kTRUE;
            }
            // Let the caller protest:
            // Error("CallShowMembers", "Cannot find any ShowMembers function for %s!", GetName());
            return kFALSE;
         } else {
            R__LOCKGUARD2(gCINTMutex);
            gCint->CallFunc_ResetArg(fInterShowMembers);
            gCint->CallFunc_SetArg(fInterShowMembers,(long) &insp);
            gCint->CallFunc_SetArg(fInterShowMembers,(long) parent);
            void* address = (void*) (((long) obj) + fOffsetStreamer);
            gCint->CallFunc_Exec((CallFunc_t*)fInterShowMembers,address);
            return kTRUE;
         }
      } // isATObject
   } // fShowMembers is set

   return kFALSE;
}

//______________________________________________________________________________
Bool_t TClass::CanSplit() const
{
   // Return true if the data member of this TClass can be saved separately.

   // Note: add the possibility to set it for the class and the derived class.
   // save the info in TVirtualStreamerInfo
   // deal with the info in MakeProject
   if (fRefProxy)                 return kFALSE;
   if (InheritsFrom("TRef"))      return kFALSE;
   if (InheritsFrom("TRefArray")) return kFALSE;
   if (InheritsFrom("TArray"))    return kFALSE;
   if (fName.BeginsWith("TVectorT<")) return kFALSE;
   if (fName.BeginsWith("TMatrixT<")) return kFALSE;
   if (InheritsFrom("TCollection") && !InheritsFrom("TClonesArray")) return kFALSE;

   // If we do not have a showMembers and we have a streamer,
   // we are in the case of class that can never be split since it is
   // opaque to us.
   if (GetShowMembersWrapper()==0 && GetStreamer()!=0) {

      // the exception are the STL containers.
      if (GetCollectionProxy()==0) {
         // We do NOT have a collection.  The class is true opaque
         return kFALSE;

      } else {

         // However we do not split collections of collections
         // nor collections of strings
         // nor collections of pointers
         // (actually we __could__ split collection of pointers to non-virtual class,
         //  but we dont for now).

         if (GetCollectionProxy()->HasPointers()) return kFALSE;

         TClass *valueClass = GetCollectionProxy()->GetValueClass();
         if (valueClass == 0) return kFALSE;
         if (valueClass==TString::Class() || valueClass==TClass::GetClass("string"))
            return kFALSE;
         if (!valueClass->CanSplit()) return kFALSE;
         if (valueClass->GetCollectionProxy() != 0) return kFALSE;

         Int_t stl = -TClassEdit::IsSTLCont(GetName(), 0);
         if ((stl==TClassEdit::kMap || stl==TClassEdit::kMultiMap)
              && valueClass->GetClassInfo()==0)
         {
            return kFALSE;
         }
      }
   }

   TClass *ncThis = const_cast<TClass*>(this);
   TIter nextb(ncThis->GetListOfBases());
   TBaseClass *base;
   while((base = (TBaseClass*)nextb())) {
      if (!TClass::GetClass(base->GetName())) return kFALSE;
   }

   return kTRUE;
}

//______________________________________________________________________________
TObject *TClass::Clone(const char *new_name) const
{
   // Create a Clone of this TClass object, in particular using the same 'dictionary'.
   // This effectively creates an alias for the class name

   if (new_name == 0 || new_name[0]=='\0' || fName == new_name) {
      Error("Clone","The name of the class must be changed when cloning a TClass object.");
      return 0;
   }
   // Temporarily remove the original from the list of classes.
   TClass::RemoveClass(const_cast<TClass*>(this));
 
   TClass *copy;
   if (fTypeInfo) {
      copy = new TClass(GetName(),
                        fClassVersion,
                        *fTypeInfo,
                        new TIsAProxy(*fTypeInfo),
                        fShowMembers,
                        GetDeclFileName(),
                        GetImplFileName(),
                        GetDeclFileLine(),
                        GetImplFileLine());
   } else {
      copy = new TClass(GetName(),
                        fClassVersion,
                        GetDeclFileName(),
                        GetImplFileName(),
                        GetDeclFileLine(),
                        GetImplFileLine());
      copy->fShowMembers = fShowMembers;
   }
   // Remove the copy before renaming it
   TClass::RemoveClass(copy);
   copy->SetName(new_name);
   TClass::AddClass(copy);

   copy->SetNew(fNew);
   copy->SetNewArray(fNewArray);
   copy->SetDelete(fDelete);
   copy->SetDeleteArray(fDeleteArray);
   copy->SetDestructor(fDestructor);
   copy->SetDirectoryAutoAdd(fDirAutoAdd);
   if (fStreamer) {
      copy->AdoptStreamer(fStreamer->Generate());
   }
   // If IsZombie is true, something went wront and we will not be
   // able to properly copy the collection proxy
   if (fCollectionProxy && !copy->IsZombie()) {
      copy->CopyCollectionProxy(*fCollectionProxy);
   }
   copy->SetClassSize(fSizeof);
   if (fRefProxy) {
      copy->AdoptReferenceProxy( fRefProxy->Clone() );
   }
   TClass::AddClass(const_cast<TClass*>(this));
   return copy;
}   

//______________________________________________________________________________
void TClass::CopyCollectionProxy(const TVirtualCollectionProxy &orig)
{
   // Copy the argument.

//     // This code was used too quickly test the STL Emulation layer
//    Int_t k = TClassEdit::IsSTLCont(GetName());
//    if (k==1||k==-1) return;

   delete fCollectionProxy;
   fCollectionProxy = orig.Generate();
}


//______________________________________________________________________________
void TClass::Draw(Option_t *option)
{
   // Draw detailed class inheritance structure.
   // If a class B inherits from a class A, the description of B is drawn
   // on the right side of the description of A.
   // Member functions overridden by B are shown in class A with a blue line
   // erasing the corresponding member function

   if (!fClassInfo) return;

   TVirtualPad *padsav = gPad;

   // Should we create a new canvas?
   TString opt=option;
   if (!padsav || !opt.Contains("same")) {
      TVirtualPad *padclass = (TVirtualPad*)(gROOT->GetListOfCanvases())->FindObject("R__class");
      if (!padclass) {
         gROOT->ProcessLine("new TCanvas(\"R__class\",\"class\",20,20,1000,750);");
      } else {
         padclass->cd();
      }
   }

   if (gPad) gPad->DrawClassObject(this,option);

   if (padsav) padsav->cd();
}

//______________________________________________________________________________
void TClass::Dump(void *obj) const
{
   // Dump contents of object on stdout.
   // Using the information in the object dictionary
   // each data member is interpreted.
   // If a data member is a pointer, the pointer value is printed
   // 'obj' is assume to point to an object of the class describe by this TClass
   //
   // The following output is the Dump of a TArrow object:
   //   fAngle                   0           Arrow opening angle (degrees)
   //   fArrowSize               0.2         Arrow Size
   //   fOption.*fData
   //   fX1                      0.1         X of 1st point
   //   fY1                      0.15        Y of 1st point
   //   fX2                      0.67        X of 2nd point
   //   fY2                      0.83        Y of 2nd point
   //   fUniqueID                0           object unique identifier
   //   fBits                    50331648    bit field status word
   //   fLineColor               1           line color
   //   fLineStyle               1           line style
   //   fLineWidth               1           line width
   //   fFillColor               19          fill area color
   //   fFillStyle               1001        fill area style

   Printf("==>Dumping object at:%lx, class=%s\n",(Long_t)obj,GetName());
   char parent[256];
   parent[0] = 0;
   TDumpMembers dm;
   if (!CallShowMembers(obj, dm, parent)) {
      Info("Dump", "No ShowMembers function, dumping disabled");
   }
}

//______________________________________________________________________________
char *TClass::EscapeChars(const char *text) const
{
   // Introduce an escape character (@) in front of a special chars.
   // You need to use the result immediately before it is being overwritten.

   static char name[128];
   Int_t nch = strlen(text);
   if (nch > 127) nch = 127;
   Int_t icur = -1;
   for (Int_t i = 0; i < nch; i++) {
      icur++;
      if (text[i] == '\"' || text[i] == '[' || text[i] == '~' ||
          text[i] == ']'  || text[i] == '&' || text[i] == '#' ||
          text[i] == '!'  || text[i] == '^' || text[i] == '<' ||
          text[i] == '?'  || text[i] == '>') {
         name[icur] = '@';
         icur++;
      }
      name[icur] = text[i];
   }
   name[icur+1] = 0;
   return name;
}

//______________________________________________________________________________
TClass *TClass::GetActualClass(const void *object) const
{
   // Return a pointer the the real class of the object.
   // This is equivalent to object->IsA() when the class has a ClassDef.
   // It is REQUIRED that object is coming from a proper pointer to the
   // class represented by 'this'.
   // Example: Special case:
   //    class MyClass : public AnotherClass, public TObject
   // then on return, one must do:
   //    TObject *obj = (TObject*)((void*)myobject)directory->Get("some object of MyClass");
   //    MyClass::Class()->GetActualClass(obj); // this would be wrong!!!
   // Also if the class represented by 'this' and NONE of its parents classes
   // have a virtual ptr table, the result will be 'this' and NOT the actual
   // class.

   if (object==0 || !IsLoaded() ) return (TClass*)this;

   if (fIsA) {
      return (*fIsA)(object); // ROOT::IsA((ThisClass*)object);
   } else if (fGlobalIsA) {
      return fGlobalIsA(this,object);
   } else {
      //Always call IsA via the interpreter. A direct call like
      //      object->IsA(brd, parent);
      //will not work if the class derives from TObject but not as primary
      //inheritance.
      if (fIsAMethod==0) {
         fIsAMethod = new TMethodCall((TClass*)this, "IsA", "");

         if (!fIsAMethod->GetMethod()) {
            delete fIsAMethod;
            fIsAMethod = 0;
            Error("IsA","Can not find any IsA function for %s!",GetName());
            return (TClass*)this;
         }

      }
      char * char_result = 0;
      fIsAMethod->Execute((void*)object, &char_result);
      return (TClass*)char_result;
   }
}

//______________________________________________________________________________
TClass *TClass::GetBaseClass(const char *classname)
{
   // Return pointer to the base class "classname". Returns 0 in case
   // "classname" is not a base class. Takes care of multiple inheritance.

   // check if class name itself is equal to classname
   if (strcmp(GetName(), classname) == 0) return this;

   if (!fClassInfo) return 0;

   TObjLink *lnk = GetListOfBases() ? fBase->FirstLink() : 0;

   // otherwise look at inheritance tree
   while (lnk) {
      TClass     *c, *c1;
      TBaseClass *base = (TBaseClass*) lnk->GetObject();
      c = base->GetClassPointer();
      if (c) {
         if (strcmp(c->GetName(), classname) == 0) return c;
         c1 = c->GetBaseClass(classname);
         if (c1) return c1;
      }
      lnk = lnk->Next();
   }
   return 0;
}

//______________________________________________________________________________
TClass *TClass::GetBaseClass(const TClass *cl)
{
   // Return pointer to the base class "cl". Returns 0 in case "cl"
   // is not a base class. Takes care of multiple inheritance.

   // check if class name itself is equal to classname
   if (cl == this) return this;

   if (!fClassInfo) return 0;

   TObjLink *lnk = GetListOfBases() ? fBase->FirstLink() : 0;

   // otherwise look at inheritance tree
   while (lnk) {
      TClass     *c, *c1;
      TBaseClass *base = (TBaseClass*) lnk->GetObject();
      c = base->GetClassPointer();
      if (c) {
         if (cl == c) return c;
         c1 = c->GetBaseClass(cl);
         if (c1) return c1;
      }
      lnk = lnk->Next();
   }
   return 0;
}

//______________________________________________________________________________
Int_t TClass::GetBaseClassOffsetRecurse(const TClass *cl)
{
   // Return data member offset to the base class "cl".
   // Returns -1 in case "cl" is not a base class.
   // Returns -2 if cl is a base class, but we can't find the offset
   // because it's virtual.
   // Takes care of multiple inheritance.

   // check if class name itself is equal to classname
   if (cl == this) return 0;

   if (!fClassInfo) {
      TVirtualStreamerInfo *sinfo = GetCurrentStreamerInfo();
      if (!sinfo) return -1;
      TStreamerElement *element;
      Int_t offset = 0;

      TObjArray &elems = *(sinfo->GetElements());
      Int_t size = elems.GetLast()+1;
      for(Int_t i=0; i<size; i++) {
         element = (TStreamerElement*)elems[i];
         if (element->IsA() == TStreamerBase::Class()) {
            TStreamerBase *base = (TStreamerBase*)element;
            TClass *baseclass = base->GetClassPointer();
            if (!baseclass) return -1;
            Int_t subOffset = baseclass->GetBaseClassOffsetRecurse(cl);
            if (subOffset == -2) return -2;
            if (subOffset != -1) return offset+subOffset;
            offset += baseclass->Size();
         }
      }
      return -1;
   }

   TClass     *c;
   Int_t      off;
   TBaseClass *inh;
   TObjLink *lnk = 0;
   if (fBase==0) lnk = GetListOfBases()->FirstLink();
   else lnk = fBase->FirstLink();

   // otherwise look at inheritance tree
   while (lnk) {
      inh = (TBaseClass *)lnk->GetObject();
      //use option load=kFALSE to avoid a warning like:
      //"Warning in <TClass::TClass>: no dictionary for class TRefCnt is available"
      //We can not afford to not have the class if it exist, so we
      //use kTRUE.
      c = inh->GetClassPointer(kTRUE); // kFALSE);
      if (c) {
         if (cl == c) {
            if ((inh->Property() & G__BIT_ISVIRTUALBASE) != 0)
               return -2;
            return inh->GetDelta();
         }
         off = c->GetBaseClassOffsetRecurse(cl);
         if (off == -2) return -2;
         if (off != -1) return off + inh->GetDelta();
      }
      lnk = lnk->Next();
   }
   return -1;
}

//______________________________________________________________________________
Int_t TClass::GetBaseClassOffset(const TClass *cl)
{
   // Return data member offset to the base class "cl".
   // Returns -1 in case "cl" is not a base class.
   // Takes care of multiple inheritance.

   Int_t offset = GetBaseClassOffsetRecurse (cl);
   if (offset == -2) {
      // Can we get the offset from CINT?
      if (cl->GetClassInfo()) {
         Long_t base_tagnum = gCint->ClassInfo_Tagnum(cl->GetClassInfo());
         BaseClassInfo_t *t = gCint->BaseClassInfo_Factory(GetClassInfo());
         while (gCint->BaseClassInfo_Next(t,0)) {
            if (gCint->BaseClassInfo_Tagnum(t) == base_tagnum) {
               if ((gCint->BaseClassInfo_Property(t) & G__BIT_ISVIRTUALBASE) != 0) {
                  break;
               }
               int off = gCint->BaseClassInfo_Offset(t);
               gCint->BaseClassInfo_Delete(t);
               return off;
            }
         }
         gCint->BaseClassInfo_Delete(t);
      }
      offset = -1;
   }
   return offset;
}

//______________________________________________________________________________
TClass *TClass::GetBaseDataMember(const char *datamember)
{
   // Return pointer to (base) class that contains datamember.

   if (!fClassInfo) return 0;

   // Check if data member exists in class itself
   TDataMember *dm = GetDataMember(datamember);
   if (dm) return this;

   // if datamember not found in class, search in next base classes
   TBaseClass *inh;
   TIter       next(GetListOfBases());
   while ((inh = (TBaseClass *) next())) {
      TClass *c = inh->GetClassPointer();
      if (c) {
         TClass *cdm = c->GetBaseDataMember(datamember);
         if (cdm) return cdm;
      }
   }

   return 0;
}

namespace {
   // A local Helper class used to keep 2 pointer (the collection proxy
   // and the class streamer) in the thread local storage.

   struct TClassLocalStorage {
      TClassLocalStorage() : fCollectionProxy(0), fStreamer(0) {};

      TVirtualCollectionProxy *fCollectionProxy;
      TClassStreamer          *fStreamer;

      static TClassLocalStorage *GetStorage(const TClass *cl) {
         void **thread_ptr = (*gThreadTsd)(0,1);
         if (thread_ptr) {
            if (*thread_ptr==0) *thread_ptr = new TExMap();
            TExMap *lmap = (TExMap*)(*thread_ptr);
            ULong_t hash = TString::Hash(&cl, sizeof(void*));
            ULong_t local = 0;
            UInt_t slot;
            if ((local = (ULong_t)lmap->GetValue(hash, (Long_t)cl, slot)) != 0) {
            } else {
               local = (ULong_t) new TClassLocalStorage();
               lmap->AddAt(slot, hash, (Long_t)cl, local);
            }
            return (TClassLocalStorage*)local;
         }
         return 0;
      }
   };
}

//______________________________________________________________________________
TVirtualCollectionProxy *TClass::GetCollectionProxy() const
{
   // Return the proxy describinb the collection (if any).

   if (gThreadTsd && fCollectionProxy) {
      TClassLocalStorage *local = TClassLocalStorage::GetStorage(this);
      if (local == 0) return fCollectionProxy;
      if (local->fCollectionProxy==0) local->fCollectionProxy = fCollectionProxy->Generate();
      return local->fCollectionProxy;
   }
   return fCollectionProxy;
}

//______________________________________________________________________________
TClassStreamer *TClass::GetStreamer() const
{
   // Return the proxy describinb the collection (if any).

   if (gThreadTsd && fStreamer) {
      TClassLocalStorage *local = TClassLocalStorage::GetStorage(this);
      if (local==0) return fStreamer;
      if (local->fStreamer==0) {
         local->fStreamer = fStreamer->Generate();
         const type_info &orig = ( typeid(*fStreamer) );
         const type_info &copy = ( typeid(*local->fStreamer) );
         if (strcmp(orig.name(),copy.name())!=0) {
            Warning("GetStreamer","For %s, the TClassStreamer passed does not properly implement the Generate method (%s vs %s\n",GetName(),orig.name(),copy.name());
         }
      }
      return local->fStreamer;
   }
   return fStreamer;
}

//______________________________________________________________________________
TVirtualIsAProxy* TClass::GetIsAProxy() const
{
   // Return the proxy implementing the IsA functionality.

   return fIsA;
}


//______________________________________________________________________________
TClass *TClass::GetClass(const char *name, Bool_t load, Bool_t silent)
{
   // Static method returning pointer to TClass of the specified class name.
   // If load is true an attempt is made to obtain the class by loading
   // the appropriate shared library (directed by the rootmap file).
   // Returns 0 in case class is not found.

   if (!name || !strlen(name)) return 0;
   if (!gROOT->GetListOfClasses())    return 0;

   TClass *cl = (TClass*)gROOT->GetListOfClasses()->FindObject(name);

   if (!cl) {
      TString resolvedName = TClassEdit::ResolveTypedef(name,kTRUE).c_str();
      cl = (TClass*)gROOT->GetListOfClasses()->FindObject(resolvedName);
   }

   if (cl) {

      if (cl->IsLoaded()) return cl;

      //we may pass here in case of a dummy class created by TVirtualStreamerInfo
      load = kTRUE;

      if (TClassEdit::IsSTLCont(name)) {

         const char *altname = gCint->GetInterpreterTypeName(name);
         if (altname && strcmp(altname,name)!=0) {

            // Remove the existing (soon to be invalid) TClass object to
            // avoid an infinite recursion.
            gROOT->GetListOfClasses()->Remove(cl);
            TClass *newcl = GetClass(altname,load);

            // since the name are different but we got a TClass, we assume
            // we need to replace and delete this class.
            assert(newcl!=cl);
            cl->ReplaceWith(newcl);
            delete cl;
            return newcl;
         }
      }

   } else {

      if (!TClassEdit::IsSTLCont(name)) {

         // If the name is actually an STL container we prefer the
         // short name rather than the true name (at least) in
         // a first try!

         TDataType *objType = gROOT->GetType(name, load);
         if (objType) {
            const char *typdfName = objType->GetTypeName();
            if (typdfName && strcmp(typdfName, name)) {
               cl = TClass::GetClass(typdfName, load);
               return cl;
            }
         }

      } else {

         cl = gROOT->FindSTLClass(name,kFALSE);

         if (cl) {
            if (cl->IsLoaded()) return cl;

            //we may pass here in case of a dummy class created by TVirtualStreamerInfo
            //return TClass::GetClass(cl->GetName(),kTRUE);
            return TClass::GetClass(cl->GetName(),kTRUE);
         }

      }

   }

   if (!load) return 0;

   TClass *loadedcl = 0;
   if (cl) loadedcl = gROOT->LoadClass(cl->GetName());
   else    loadedcl = gROOT->LoadClass(name);

   if (loadedcl) return loadedcl;

   if (cl) return cl;  // If we found the class but we already have a dummy class use it.

   static const char *full_string_name = "basic_string<char,char_traits<char>,allocator<char> >";
   if (strcmp(name,full_string_name)==0
      || ( strncmp(name,"std::",5)==0 && ((strcmp(name+5,"string")==0)||(strcmp(name+5,full_string_name)==0)))) {
      return TClass::GetClass("string");
   }
   if (TClassEdit::IsSTLCont(name)) {

      return gROOT->FindSTLClass(name,kTRUE);

   } else if ( strncmp(name,"std::",5)==0 ) {

      return TClass::GetClass(name+5,load);

   } else if ( strstr(name,"std::") != 0 ) {

      // Let's try without the std:: in the template parameters.
      TString rname( TClassEdit::ResolveTypedef(name,kTRUE) );
      if (rname != name) {
         return TClass::GetClass( rname, load );
      }
   }

   if (!strcmp(name, "long long")||!strcmp(name,"unsigned long long"))
      return 0; // reject long longs

   //last attempt. Look in CINT list of all (compiled+interpreted) classes

   // CheckClassInfo might modify the content of its parameter if it is
   // a template and has extra or missing space (eg. one<two<tree>> becomes
   // one<two<three> >
   char *modifiable_name = new char[strlen(name)*2];
   strcpy(modifiable_name,name);
   if (gInterpreter->CheckClassInfo(modifiable_name)) {
      const char *altname = gInterpreter->GetInterpreterTypeName(modifiable_name,kTRUE);
      if (strcmp(altname,name)!=0) {
         // altname now contains the full name of the class including a possible
         // namespace if there has been a using namespace statement.
         delete [] modifiable_name;
         return GetClass(altname,load);
      }
      TClass *ncl = new TClass(name, 1, 0, 0, -1, -1, silent);
      if (!ncl->IsZombie()) {
         delete [] modifiable_name;
         return ncl;
      }
      delete ncl;
   }
   delete [] modifiable_name;
   return 0;
}

//______________________________________________________________________________
TClass *TClass::GetClass(const type_info& typeinfo, Bool_t load, Bool_t /* silent */)
{
   // Return pointer to class with name.

   if (!gROOT->GetListOfClasses())    return 0;

//printf("TClass::GetClass called, typeinfo.name=%s\n",typeinfo.name());
   TClass* cl = fgIdMap->Find(typeinfo.name());

   if (cl) {
      if (cl->IsLoaded()) return cl;
      //we may pass here in case of a dummy class created by TVirtualStreamerInfo
      load = kTRUE;
   } else {
     // Note we might need support for typedefs and simple types!

     //      TDataType *objType = GetType(name, load);
     //if (objType) {
     //    const char *typdfName = objType->GetTypeName();
     //    if (typdfName && strcmp(typdfName, name)) {
     //       cl = GetClass(typdfName, load);
     //       return cl;
     //    }
     // }
   }

   if (!load) return 0;

   VoidFuncPtr_t dict = TClassTable::GetDict(typeinfo);
   if (dict) {
      (dict)();
      cl = GetClass(typeinfo,kFALSE);
      if (cl) cl->PostLoadCheck();
      return cl;
   }
   if (cl) return cl;

   TIter next(gROOT->GetListOfClassGenerators());
   TClassGenerator *gen;
   while( (gen = (TClassGenerator*) next()) ) {
      cl = gen->GetClass(typeinfo,load);
      if (cl) {
         cl->PostLoadCheck();
         return cl;
      }
   }

   //last attempt. Look in CINT list of all (compiled+interpreted) classes
   //   if (gInterpreter->CheckClassInfo(name)) {
   //      TClass *ncl = new TClass(name, 1, 0, 0, 0, -1, -1);
   //      if (!ncl->IsZombie()) return ncl;
   //      delete ncl;
   //   }
   return 0;
}

//______________________________________________________________________________
VoidFuncPtr_t  TClass::GetDict (const char *cname)
{
   // Return a pointer to the dictionary loading function generated by
   // rootcint

   return TClassTable::GetDict(cname);
}

//______________________________________________________________________________
VoidFuncPtr_t  TClass::GetDict (const type_info& info)
{
   // Return a pointer to the dictionary loading function generated by
   // rootcint

   return TClassTable::GetDict(info);
}

//______________________________________________________________________________
TDataMember *TClass::GetDataMember(const char *datamember) const
{
   // Return pointer to datamember object with name "datamember".

   if (!fClassInfo) return 0;

   // Strip off leading *'s and trailing [
   const Int_t size_buffer = 256;
   char memb[size_buffer];
   char *s = (char*)datamember;
   while (*s == '*') s++;

   size_t len = strlen(s);
   if (len > size_buffer - 2)
      len = size_buffer - 2;
   strncpy(memb, s, len);
   memb[len] = 0;

   if ((s = strchr(memb, '['))) {
      *s = 0;
      len = strlen(memb);
   }

   TDataMember *dm;
   TIter   next(((TClass*)this)->GetListOfDataMembers());

   while ((dm = (TDataMember *) next()))
      if (len >= size_buffer - 2) {
         if (strncmp(memb, dm->GetName(), len) == 0)
            return dm;
      } else
         if (strcmp(memb, dm->GetName()) == 0)
            return dm;
   return 0;
}

//______________________________________________________________________________
Long_t TClass::GetDataMemberOffset(const char *name) const
{
   // return offset for member name. name can be a data member in
   // the class itself, one of its base classes, or one member in
   // one of the aggregated classes.
   //
   // In case of an emulated class, the list of emulated TRealData is built

   TRealData *rd = GetRealData(name);
   if (rd) return rd->GetThisOffset();

   return 0;
}

//______________________________________________________________________________
TRealData* TClass::GetRealData(const char* name) const
{
   // -- Return pointer to TRealData element with name "name".
   //
   // Name can be a data member in the class itself,
   // one of its base classes, or a member in
   // one of the aggregated classes.
   //
   // In case of an emulated class, the list of emulated TRealData is built.
   //

   if (!fRealData) {
      const_cast<TClass*>(this)->BuildRealData();
   }

   if (!fRealData) {
      return 0;
   }

   if (!name) {
      return 0;
   }

   // First try just the whole name.
   TRealData* rd = (TRealData*) fRealData->FindObject(name);
   if (rd) {
      return rd;
   }

   std::string givenName(name);

   // Try ignoring the array dimensions.
   std::string::size_type firstBracket = givenName.find_first_of("[");
   if (firstBracket != std::string::npos) {
      // -- We are looking for an array data member.
      std::string nameNoDim(givenName.substr(0, firstBracket));
      TObjLink* lnk = fRealData->FirstLink();
      while (lnk) {
         TObject* obj = lnk->GetObject();
         std::string objName(obj->GetName());
         std::string::size_type pos = objName.find_first_of("[");
         // Only match arrays to arrays for now.
         if (pos != std::string::npos) {
            objName.erase(pos);
            if (objName == nameNoDim) {
               return static_cast<TRealData*>(obj);
            }
         }
         lnk = lnk->Next();
      }
   }

   // Now try it as a pointer.
   std::ostringstream ptrname;
   ptrname << "*" << givenName;
   rd = (TRealData*) fRealData->FindObject(ptrname.str().c_str());
   if (rd) {
      return rd;
   }

   // Check for a dot in the name.
   std::string::size_type firstDot = givenName.find_first_of(".");
   if (firstDot == std::string::npos) {
      // -- Not found, a simple name, all done.
      return 0;
   }

   //
   //  At this point the name has a dot in it, so it is the name
   //  of some contained sub-object.
   //

   // May be a pointer like in TH1: fXaxis.fLabels (in TRealdata is named fXaxis.*fLabels)
   std::string::size_type lastDot = givenName.find_last_of(".");
   std::ostringstream starname;
   starname << givenName.substr(0, lastDot) << ".*" << givenName.substr(lastDot + 1);
   rd = (TRealData*) fRealData->FindObject(starname.str().c_str());
   if (rd) {
      return rd;
   }

   // Strip the first component, it may be the name of
   // the branch (old TBranchElement code), and try again.
   std::string firstDotName(givenName.substr(firstDot + 1));

   // New attempt starting after the first "." if any,
   // this allows for the case that the first component
   // may have been a branch name (for TBranchElement).
   rd = (TRealData*) fRealData->FindObject(firstDotName.c_str());
   if (rd) {
      return rd;
   }

   // New attempt starting after the first "." if any,
   // but this time try ignoring the array dimensions.
   // Again, we are allowing for the case that the first
   // component may have been a branch name (for TBranchElement).
   std::string::size_type firstDotBracket = firstDotName.find_first_of("[");
   if (firstDotBracket != std::string::npos) {
      // -- We are looking for an array data member.
      std::string nameNoDim(firstDotName.substr(0, firstDotBracket));
      TObjLink* lnk = fRealData->FirstLink();
      while (lnk) {
         TObject* obj = lnk->GetObject();
         std::string objName(obj->GetName());
         std::string::size_type pos = objName.find_first_of("[");
         // Only match arrays to arrays for now.
         if (pos != std::string::npos) {
            objName.erase(pos);
            if (objName == nameNoDim) {
               return static_cast<TRealData*>(obj);
            }
         }
         lnk = lnk->Next();
      }
   }

   // New attempt starting after the first "." if any,
   // but this time check for a pointer type.  Again, we
   // are allowing for the case that the first component
   // may have been a branch name (for TBranchElement).
   ptrname.str("");
   ptrname << "*" << firstDotName;
   rd = (TRealData*) fRealData->FindObject(ptrname.str().c_str());
   if (rd) {
      return rd;
   }

   // Last attempt in case a member has been changed from
   // a static array to a pointer, for example the member
   // was arr[20] and is now *arr.
   //
   // Note: In principle, one could also take into account
   // the opposite situation where a member like *arr has
   // been converted to arr[20].
   //
   // FIXME: What about checking after the first dot as well?
   //
   std::string::size_type bracket = starname.str().find_first_of("[");
   if (bracket == std::string::npos) {
      return 0;
   }
   rd = (TRealData*) fRealData->FindObject(starname.str().substr(0, bracket).c_str());
   if (rd) {
      return rd;
   }

   // Not found;
   return 0;
}

//______________________________________________________________________________
const char *TClass::GetSharedLibs()
{
   // Get the list of shared libraries containing the code for class cls.
   // The first library in the list is the one containing the class, the
   // others are the libraries the first one depends on. Returns 0
   // in case the library is not found.

   if (!gInterpreter) return 0;

   if (fSharedLibs.IsNull())
      fSharedLibs = gInterpreter->GetClassSharedLibs(fName);

   return !fSharedLibs.IsNull() ? fSharedLibs.Data() : 0;
}

//______________________________________________________________________________
TList *TClass::GetListOfBases()
{
   // Return list containing the TBaseClass(es) of a class.

   if (!fBase) {
      if (!fClassInfo) return 0;

      if (!gInterpreter)
         Fatal("GetListOfBases", "gInterpreter not initialized");

      gInterpreter->CreateListOfBaseClasses(this);
   }
   return fBase;
}

//______________________________________________________________________________
TList *TClass::GetListOfDataMembers()
{
   // Return list containing the TDataMembers of a class.

   if (!fClassInfo) {
      if (!fData) fData = new TList;
      return fData;
   }

   if (!fData) {
      if (!gInterpreter)
         Fatal("GetListOfDataMembers", "gInterpreter not initialized");

      gInterpreter->CreateListOfDataMembers(this);
   }
   return fData;
}

//______________________________________________________________________________
TList *TClass::GetListOfMethods()
{
   // Return list containing the TMethods of a class.

   if (!fClassInfo) {
      if (!fMethod) fMethod = new TList;
      return fMethod;
   }

   if (!fMethod) {
      if (!gInterpreter)
         Fatal("GetListOfMethods", "gInterpreter not initialized");

      gInterpreter->CreateListOfMethods(this);
   } else {
      gInterpreter->UpdateListOfMethods(this);
   }
   return fMethod;
}

//______________________________________________________________________________
TList *TClass::GetListOfAllPublicMethods()
{
   // Returns a list of all public methods of this class and its base classes.
   // Refers to a subset of the methods in GetListOfMethods() so don't do
   // GetListOfAllPublicMethods()->Delete().
   // Algorithm used to get the list is:
   // - put all methods of the class in the list (also protected and private
   //   ones).
   // - loop over all base classes and add only those methods not already in the
   //   list (also protected and private ones).
   // - once finished, loop over resulting list and remove all private and
   //   protected methods.

   if (!fAllPubMethod) {
      fAllPubMethod = new TList;

      // put all methods in the list
      fAllPubMethod->AddAll(GetListOfMethods());

      // loop over all base classes and add new methods
      TIter nextBaseClass(GetListOfBases());
      TBaseClass *pB;
      TMethod    *p;
      while ((pB = (TBaseClass*) nextBaseClass())) {
         if (!pB->GetClassPointer()) continue;
         TIter next(pB->GetClassPointer()->GetListOfAllPublicMethods());
         TList temp;
         while ((p = (TMethod*) next()))
            if (!fAllPubMethod->Contains(p->GetName()))
               temp.Add(p);
         fAllPubMethod->AddAll(&temp);
         temp.Clear();
      }

      // loop over list and remove all non-public methods
      TIter next(fAllPubMethod);
      while ((p = (TMethod*) next()))
         if (!(p->Property() & kIsPublic)) fAllPubMethod->Remove(p);
   }
   return fAllPubMethod;
}

//______________________________________________________________________________
TList *TClass::GetListOfAllPublicDataMembers()
{
   // Returns a list of all public data members of this class and its base
   // classes. Refers to a subset of the data members in GetListOfDatamembers()
   // so don't do GetListOfAllPublicDataMembers()->Delete().

   if (!fAllPubData) {
      fAllPubData = new TList;
      TIter next(GetListOfDataMembers());
      TDataMember *p;

      while ((p = (TDataMember*) next()))
         if (p->Property() & kIsPublic) fAllPubData->Add(p);

      TIter next_BaseClass(GetListOfBases());
      TBaseClass *pB;
      while ((pB = (TBaseClass*) next_BaseClass())) {
         if (!pB->GetClassPointer()) continue;
         fAllPubData->AddAll(pB->GetClassPointer()->GetListOfAllPublicDataMembers() );
      }
   }
   return fAllPubData;
}

//______________________________________________________________________________
void TClass::GetMenuItems(TList *list)
{
   // Returns list of methods accessible by context menu.

   if (!fClassInfo) return;

   // get the base class
   TIter nextBase(GetListOfBases(), kIterBackward);
   TBaseClass *baseClass;
   while ((baseClass = (TBaseClass *) nextBase())) {
      TClass *base = baseClass->GetClassPointer();
      if (base) base->GetMenuItems(list);
   }

   // remove methods redefined in this class with no menu
   TMethod *method, *m;
   TIter next(GetListOfMethods(), kIterBackward);
   while ((method = (TMethod*)next())) {
      m = (TMethod*)list->FindObject(method->GetName());
      if (method->IsMenuItem()) {
         if (!m)
            list->AddFirst(method);
      } else {
         if (m && m->GetNargs() == method->GetNargs())
            list->Remove(m);
      }
   }
}

//______________________________________________________________________________
Bool_t TClass::IsFolder(void *obj) const
{
   // Return kTRUE if the class has elements.

   return Browse(obj,(TBrowser*)0);
}

//______________________________________________________________________________
void TClass::RemoveRef(TClassRef *ref)
{
   // Unregister the TClassRef object.

   if (ref==fRefStart) {
      fRefStart = ref->fNext;
      if (fRefStart) fRefStart->fPrevious = 0;
      ref->fPrevious = ref->fNext = 0;
   } else {
      TClassRef *next = ref->fNext;
      ref->fPrevious->fNext = next;
      if (next) next->fPrevious = ref->fPrevious;
      ref->fPrevious = ref->fNext = 0;
   }
}

//______________________________________________________________________________
void TClass::ReplaceWith(TClass *newcl, Bool_t recurse) const
{
   // Inform the other objects to replace this object by the new TClass (newcl)

   //we must update the class pointers pointing to 'this' in all TStreamerElements
   TIter nextClass(gROOT->GetListOfClasses());
   TClass *acl;
   TVirtualStreamerInfo *info;
   TList tobedeleted;

   TString corename( TClassEdit::ResolveTypedef(newcl->GetName()) );

   if ( strchr( corename.Data(), '<' ) == 0 ) {
      // not a template, let's skip
      recurse = kFALSE;
   }

   while ((acl = (TClass*)nextClass())) {
      if (recurse && acl!=newcl && acl!=this) {

         TString aclCorename( TClassEdit::ResolveTypedef(acl->GetName()) );

         if (aclCorename == corename) {

            // 'acl' represents the same class as 'newcl' (and this object)

            acl->ReplaceWith(newcl, kFALSE);
            tobedeleted.Add(acl);
         }
      }

      TIter nextInfo(acl->GetStreamerInfos());
      while ((info = (TVirtualStreamerInfo*)nextInfo())) {

         info->Update(this, newcl);
      }

      if (acl->GetCollectionProxy() && acl->GetCollectionProxy()->GetValueClass()==this) {
         acl->GetCollectionProxy()->SetValueClass(newcl);
         // We should also inform all the TBranchElement :( but we do not have a master list :(
      }
   }

   TIter delIter( &tobedeleted );
   while ((acl = (TClass*)delIter())) {
      delete acl;
   }

}

//______________________________________________________________________________
void TClass::ResetClassInfo(Long_t tagnum)
{
   // Make sure that the current ClassInfo is up to date.

   if (fClassInfo && gCint->ClassInfo_Tagnum(fClassInfo) != tagnum) {
      gCint->ClassInfo_Init(fClassInfo,(Int_t)tagnum);
      if (fBase) {
         fBase->Delete();
         delete fBase; fBase = 0;
      }
   }
}

//______________________________________________________________________________
void TClass::ResetMenuList()
{
   // Resets the menu list to it's standard value.

   if (fClassMenuList)
      fClassMenuList->Delete();
   else
      fClassMenuList = new TList();
   fClassMenuList->Add(new TClassMenuItem(TClassMenuItem::kPopupStandardList, this));
}

//______________________________________________________________________________
void TClass::MakeCustomMenuList()
{
   // Makes a customizable version of the popup menu list, i.e. makes a list
   // of TClassMenuItem objects of methods accessible by context menu.
   // The standard (and different) way consists in having just one element
   // in this list, corresponding to the whole standard list.
   // Once the customizable version is done, one can remove or add elements.

   TClassMenuItem *menuItem;

   // Make sure fClassMenuList is initialized and empty.
   GetMenuList()->Delete();

   TList* methodList = new TList;
   GetMenuItems(methodList);

   TMethod *method;
   TMethodArg *methodArg;
   TClass  *classPtr = 0;
   TIter next(methodList);

   while ((method = (TMethod*) next())) {
      // if go to a mother class method, add separator
      if (classPtr != method->GetClass()) {
         menuItem = new TClassMenuItem(TClassMenuItem::kPopupSeparator, this);
         fClassMenuList->AddLast(menuItem);
         classPtr = method->GetClass();
      }
      // Build the signature of the method
      TString sig;
      TList* margsList = method->GetListOfMethodArgs();
      TIter nextarg(margsList);
      while ((methodArg = (TMethodArg*)nextarg())) {
         sig = sig+","+methodArg->GetFullTypeName();
      }
      if (sig.Length()!=0) sig.Remove(0,1);  // remove first comma
      menuItem = new TClassMenuItem(TClassMenuItem::kPopupUserFunction, this,
                                    method->GetName(), method->GetName(),0,
                                    sig.Data(),-1,TClassMenuItem::kIsSelf);
      if (method->IsMenuItem() == kMenuToggle) menuItem->SetToggle();
      fClassMenuList->Add(menuItem);
   }
   delete methodList;
}

//______________________________________________________________________________
void TClass::Move(void *arenaFrom, void *arenaTo) const
{
   // Register the fact that an object was moved from the memory location
   // 'arenaFrom' to the memory location 'arenaTo'.

   // If/when we have access to a copy constructor (or better to a move
   // constructor), this function should also perform the data move.
   // For now we just information the repository.

   if (!fClassInfo && !fCollectionProxy) {
      MoveAddressInRepository("TClass::Move",arenaFrom,arenaTo,this);
   }
}

//______________________________________________________________________________
TList *TClass::GetMenuList() const {
   // Return the list of menu items associated with the class.
   if (!fClassMenuList) {
      fClassMenuList = new TList();
      fClassMenuList->Add(new TClassMenuItem(TClassMenuItem::kPopupStandardList, const_cast<TClass*>(this)));
   }
   return fClassMenuList;
}


//______________________________________________________________________________
TMethod *TClass::GetMethodAny(const char *method)
{
   // Return pointer to method without looking at parameters.
   // Does not look in (possible) base classes.

   if (!fClassInfo) return 0;

   TMethod *m;
   TIter    next(GetListOfMethods());

   while ((m = (TMethod *) next())) {
      if (strcmp(method, m->GetName()) == 0) return m;
   }
   return 0;
}

//______________________________________________________________________________
TMethod *TClass::GetMethodAllAny(const char *method)
{
   // Return pointer to method without looking at parameters.
   // Does look in all base classes.

   if (!fClassInfo) return 0;

   TMethod *m;
   TIter    next(GetListOfMethods());

   while ((m = (TMethod *) next())) {
      if (strcmp(method, m->GetName()) == 0) return m;
   }

   TBaseClass *base;
   TIter       nextb(GetListOfBases());
   while ((base = (TBaseClass *) nextb())) {
      TClass *c = base->GetClassPointer();
      if (c) {
         m = c->GetMethodAllAny(method);
         if (m) return m;
      }
   }

   return 0;
}

//______________________________________________________________________________
TMethod *TClass::GetMethod(const char *method, const char *params)
{
   // Find the best method (if there is one) matching the parameters.
   // The params string must contain argument values, like "3189, \"aap\", 1.3".
   // The function invokes GetClassMethod to search for a possible method
   // in the class itself or in its base classes. Returns 0 in case method
   // is not found.

   if (!fClassInfo) return 0;

   if (!gInterpreter)
      Fatal("GetMethod", "gInterpreter not initialized");

   Long_t faddr = (Long_t)gInterpreter->GetInterfaceMethod(this, method,
                                                           params);
   if (!faddr) return 0;

   // loop over all methods in this class (and its baseclasses) till
   // we find a TMethod with the same faddr


   TMethod *m;

#if defined(R__WIN32)
   // On windows CINT G__exec_bytecode can (seemingly) have several values :(
   // So we can not easily determine whether something is interpreted or
   // so the optimization of not looking at the mangled name can not be
   // used
   m = GetClassMethod(method,params);

#else
   if (faddr == (Long_t)gCint->GetExecByteCode()) {
      // the method is actually interpreted, its address is
      // not a discriminant (it always point to the same
      // function pointed by CINT G__exec_bytecode.
      m = GetClassMethod(method,params);
   } else {
      m = GetClassMethod(faddr);
   }
#endif

   if (m) return m;

   TBaseClass *base;
   TIter       next(GetListOfBases());
   while ((base = (TBaseClass *) next())) {
      TClass *c = base->GetClassPointer();
      if (c) {
         m = c->GetMethod(method,params);
         if (m) return m;
      }
   }
   Error("GetMethod",
         "\nDid not find matching TMethod <%s> with \"%s\" for %s",
         method,params,GetName());
   return 0;
}

//______________________________________________________________________________
TMethod *TClass::GetMethodWithPrototype(const char *method, const char *proto)
{
   // Find the method with a given prototype. The proto string must be of the
   // form: "char*,int,double". Returns 0 in case method is not found.

   if (!fClassInfo) return 0;

   if (!gInterpreter)
      Fatal("GetMethod", "gInterpreter not initialized");

   Long_t faddr = (Long_t)gInterpreter->GetInterfaceMethodWithPrototype(this,
                                                              method, proto);
   if (!faddr) return 0;

   // loop over all methods in this class (and its baseclasses) till
   // we find a TMethod with the same faddr

   TMethod *m = GetClassMethod(faddr);
   if (m) return m;

   TBaseClass *base;
   TIter       next(GetListOfBases());
   while ((base = (TBaseClass *) next())) {
      TClass *c = base->GetClassPointer();
      if (c) {
         m = c->GetMethodWithPrototype(method,proto);
         if (m) return m;
      }
   }
   Error("GetMethod", "Did not find matching TMethod (should never happen)");
   return 0;
}

//______________________________________________________________________________
TMethod *TClass::GetClassMethod(Long_t faddr)
{
   // Look for a method in this class that has the interface function
   // address faddr.

   if (!fClassInfo) return 0;

   TMethod *m;
   TIter    next(GetListOfMethods());
   while ((m = (TMethod *) next())) {
      if (faddr == (Long_t)m->InterfaceMethod())
         return m;
   }
   return 0;
}

//______________________________________________________________________________
TMethod *TClass::GetClassMethod(const char *name, const char* params)
{
   // Look for a method in this class that has the name and
   // signature

   if (!fClassInfo) return 0;

   // Need to go through those loops to get the signature from
   // the valued params (i.e. from "1.0,3" to "double,int")

   R__LOCKGUARD2(gCINTMutex);
   CallFunc_t  *func = gCint->CallFunc_Factory();
   Long_t       offset;
   gCint->CallFunc_SetFunc(func,GetClassInfo(), name, params, &offset);
   MethodInfo_t *info = gCint->CallFunc_FactoryMethod(func);
   TMethod request(info,this);

   TMethod *m;
   TIter    next(GetListOfMethods());
   while ((m = (TMethod *) next())) {
      if (!strcmp(name,m->GetName())
          &&!strcmp(request.GetSignature(),m->GetSignature())) {
         gCint->CallFunc_Delete(func);
         return m;
      }
   }
   gCint->CallFunc_Delete(func);
   return 0;
}

//______________________________________________________________________________
Int_t TClass::GetNdata()
{
   // Return the number of data members of this class
   // Note that in case the list of data members is not yet created, it will be done
   // by GetListOfDataMembers().

   if (!fClassInfo) return 0;

   TList *lm = GetListOfDataMembers();
   if (lm)
      return lm->GetSize();
   else
      return 0;
}

//______________________________________________________________________________
Int_t TClass::GetNmethods()
{
   // Return the number of methods of this class
   // Note that in case the list of methods is not yet created, it will be done
   // by GetListOfMethods().

   if (!fClassInfo) return 0;

   TList *lm = GetListOfMethods();
   if (lm)
      return lm->GetSize();
   else
      return 0;
}

//______________________________________________________________________________
TVirtualStreamerInfo* TClass::GetStreamerInfo(Int_t version) const
{
   // returns a pointer to the TVirtualStreamerInfo object for version
   // If the object doest not exist, it is created
   //
   // Note: There are two special version numbers:
   //
   //       0: Use the class version from the currently loaded class library.
   //      -1: Assume no class library loaded (emulated class).
   //
   // Warning:  If we create a new streamer info, whether or not the build
   //           optimizes is controlled externally to us by a global variable!
   //           Don't call us unless you have set that variable properly
   //           with TStreamer::Optimize()!
   //

   // Handle special version, 0 means currently loaded version.
   // Warning:  This may be -1 for an emulated class.
   if (version == 0) {
      version = fClassVersion;
   }
   if (!fStreamerInfo) {
      fStreamerInfo = new TObjArray(version + 10, -1);
   } else {
      Int_t ninfos = fStreamerInfo->GetSize();
      if ((version < -1) || (version >= ninfos)) {
         Error("GetStreamerInfo", "class: %s, attempting to access a wrong version: %d", GetName(), version);
         // FIXME: Shouldn't we go to -1 here, or better just abort?
         version = 0;
      }
   }
   TVirtualStreamerInfo* sinfo = (TVirtualStreamerInfo*) fStreamerInfo->At(version);
   if (!sinfo && (version != fClassVersion)) {
      // When the requested version does not exist we return
      // the TVirtualStreamerInfo for the currently loaded class vesion.
      // FIXME: This arguably makes no sense, we should warn and return nothing instead.
      // Note: This is done for STL collactions
      // Note: fClassVersion could be -1 here (for an emulated class).
      sinfo = (TVirtualStreamerInfo*) fStreamerInfo->At(fClassVersion);
   }
   if (!sinfo) {
      // We just were not able to find a streamer info, we have to make a new one.
      sinfo = TVirtualStreamerInfo::Factory()->NewInfo(const_cast<TClass*>(this));
      fStreamerInfo->AddAtAndExpand(sinfo, fClassVersion);
      if (gDebug > 0) {
         printf("Creating StreamerInfo for class: %s, version: %d\n", GetName(), fClassVersion);
      }
      if (fClassInfo || fCollectionProxy) {
         // If we do not have a StreamerInfo for this version and we do not
         // have dictionary information nor a proxy, there is nothing to build!
         //
         // Warning:  Whether or not the build optimizes is controlled externally
         //           to us by a global variable!  Don't call us unless you have
         //           set that variable properly with TStreamer::Optimize()!
         //
         // FIXME: Why don't we call BuildOld() like we do below?  Answer: We are new and so don't have to do schema evolution?
         sinfo->Build();
      }
   } else {
      if (!sinfo->GetOffsets()) {
         // Streamer info has not been compiled, but exists.
         // Therefore it was read in from a file and we have to do schema evolution?
         // Or it didn't have a dictionary before, but does now?
         sinfo->BuildOld();
      }
      if (sinfo->IsOptimized() && !TVirtualStreamerInfo::CanOptimize()) {
         // Undo optimization if the global flag tells us to.
         sinfo->Compile();
      }
   }
   // Cache the current info if we now have it.
   if (version == fClassVersion) {
      fCurrentInfo = sinfo;
   }
   return sinfo;
}

//______________________________________________________________________________
void TClass::IgnoreTObjectStreamer(Bool_t ignore)
{
   //  When the class kIgnoreTObjectStreamer bit is set, the automatically
   //  generated Streamer will not call TObject::Streamer.
   //  This option saves the TObject space overhead on the file.
   //  However, the information (fBits, fUniqueID) of TObject is lost.
   //
   //  Note that this function must be called for the class deriving
   //  directly from TObject, eg, assuming that BigTrack derives from Track
   //  and Track derives from TObject, one must do:
   //     Track::Class()->IgnoreTObjectStreamer();
   //  and not:
   //     BigTrack::Class()->IgnoreTObjectStreamer();

   if ( ignore &&  TestBit(kIgnoreTObjectStreamer)) return;
   if (!ignore && !TestBit(kIgnoreTObjectStreamer)) return;
   TVirtualStreamerInfo *sinfo = GetCurrentStreamerInfo();
   if (sinfo) {
      if (sinfo->GetOffsets()) {
         // -- Warn the user that what he is doing cannot work.
         // Note: The reason is that TVirtualStreamerInfo::Build() examines
         // the kIgnoreTObjectStreamer bit and sets the TStreamerElement
         // type for the TObject base class streamer element it creates
         // to -1 as a flag.  Later on the TStreamerInfo::Compile()
         // member function sees the flag and does not insert the base
         // class element into the compiled streamer info.  None of this
         // machinery works correctly if we are called after the streamer
         // info has already been built and compiled.
         Error("IgnoreTObjectStreamer","Must be called before the creation of StreamerInfo");
         return;
      }
   }
   if (ignore) SetBit  (kIgnoreTObjectStreamer);
   else        ResetBit(kIgnoreTObjectStreamer);
}

//______________________________________________________________________________
Bool_t TClass::InheritsFrom(const char *classname) const
{
   // Return kTRUE if this class inherits from a class with name "classname".
   // note that the function returns KTRUE in case classname is the class itself

   if (strcmp(GetName(), classname) == 0) return kTRUE;

   if (!fClassInfo) return InheritsFrom(TClass::GetClass("classname"));

   // cast const away (only for member fBase which can be set in GetListOfBases())
   if (((TClass *)this)->GetBaseClass(classname)) return kTRUE;
   return kFALSE;
}

//______________________________________________________________________________
Bool_t TClass::InheritsFrom(const TClass *cl) const
{
   // Return kTRUE if this class inherits from class cl.
   // note that the function returns KTRUE in case cl is the class itself

   if (cl == this) return kTRUE;

   if (!fClassInfo) {
      TVirtualStreamerInfo *sinfo = ((TClass *)this)->GetCurrentStreamerInfo();
      if (sinfo==0) sinfo = GetStreamerInfo();
      TIter next(sinfo->GetElements());
      TStreamerElement *element;
      while ((element = (TStreamerElement*)next())) {
         if (element->IsA() == TStreamerBase::Class()) {
            TClass *clbase = element->GetClassPointer();
            if (!clbase) return kFALSE; //missing class
            if (clbase->InheritsFrom(cl)) return kTRUE;
         }
      }
      return kFALSE;
   }
   // cast const away (only for member fBase which can be set in GetListOfBases())
   if (((TClass *)this)->GetBaseClass(cl)) return kTRUE;
   return kFALSE;
}

//______________________________________________________________________________
void *TClass::DynamicCast(const TClass *cl, void *obj, Bool_t up)
{
   // Cast obj of this class type up to baseclass cl if up is true.
   // Cast obj of this class type down from baseclass cl if up is false.
   // If this class is not a baseclass of cl return 0, else the pointer
   // to the cl part of this (up) or to this (down).

   if (cl == this) return obj;

   if (!fClassInfo) return 0;

   Int_t off;
   if ((off = GetBaseClassOffset(cl)) != -1) {
      if (up)
         return (void*)((Long_t)obj+off);
      else
         return (void*)((Long_t)obj-off);
   }
   return 0;
}

//______________________________________________________________________________
void *TClass::New(ENewType defConstructor) const
{
   // Return a pointer to a newly allocated object of this class.
   // The class must have a default constructor. For meaning of
   // defConstructor, see TClass::IsCallingNew().
   //
   // The constructor actually called here can be customized by
   // using the rootcint pragma:
   //    #pragma link C++ ioctortype UserClass;
   // For example, with this pragma and a class named MyClass,
   // this method will called the first of the following 3
   // constructors which exists and is public:
   //    MyClass(UserClass*);
   //    MyClass(TRootIOCtor*);
   //    MyClass(); // Or a constructor with all its arguments defaulted.
   //
   // When more than one pragma ioctortype is used, the first seen as priority
   // For example with:
   //    #pragma link C++ ioctortype UserClass1;
   //    #pragma link C++ ioctortype UserClass2;
   // We look in the following order:
   //    MyClass(UserClass1*);
   //    MyClass(UserClass2*);
   //    MyClass(TRootIOCtor*);
   //    MyClass(); // Or a constructor with all its arguments defaulted.
   //

   void* p = 0;

   if (fNew) {
      // We have the new operator wrapper function,
      // so there is a dictionary and it was generated
      // by rootcint, so there should be a default
      // constructor we can call through the wrapper.
      fgCallingNew = defConstructor;
      p = fNew(0);
      fgCallingNew = kRealNew;
      if (!p) {
         //Error("New", "cannot create object of class %s version %d", GetName(), fClassVersion);
         Error("New", "cannot create object of class %s", GetName());
      }
   } else if (fClassInfo) {
      // We have the dictionary but do not have the
      // constructor wrapper, so the dictionary was
      // not generated by rootcint.  Let's try to
      // create the object by having the interpreter
      // call the new operator, hopefully the class
      // library is loaded and there will be a default
      // constructor we can call.
      // [This is very unlikely to work, but who knows!]
      fgCallingNew = defConstructor;
      R__LOCKGUARD2(gCINTMutex);
      p = gCint->ClassInfo_New(GetClassInfo());
      fgCallingNew = kRealNew;
      if (!p) {
         //Error("New", "cannot create object of class %s version %d", GetName(), fClassVersion);
         Error("New", "cannot create object of class %s", GetName());
      }
   } else if (!fClassInfo && fCollectionProxy) {
      // There is no dictionary at all, so this is an emulated
      // class; however we do have the services of a collection proxy,
      // so this is an emulated STL class.
      fgCallingNew = defConstructor;
      p = fCollectionProxy->New();
      fgCallingNew = kRealNew;
      if (!p) {
         //Error("New", "cannot create object of class %s version %d", GetName(), fClassVersion);
         Error("New", "cannot create object of class %s", GetName());
      }
   } else if (!fClassInfo && !fCollectionProxy) {
      // There is no dictionary at all and we do not have
      // the services of a collection proxy available, so
      // use the streamer info to approximate calling a
      // constructor (basically we just make sure that the
      // pointer data members are null, unless they are marked
      // as preallocated with the "->" comment, in which case
      // we default-construct an object to point at).

      // Do not register any TObject's that we create
      // as a result of creating this object.
      // FIXME: Why do we do this?
      // FIXME: Partial Answer: Is this because we may never actually deregister them???

      Bool_t statsave = GetObjectStat();
      SetObjectStat(kFALSE);

      TVirtualStreamerInfo* sinfo = GetStreamerInfo();
      if (!sinfo) {
         Error("New", "Cannot construct class '%s' version %d, no streamer info available!", GetName(), fClassVersion);
         return 0;
      }

      fgCallingNew = defConstructor;
      p = sinfo->New();
      fgCallingNew = kRealNew;

      // FIXME: Mistake?  See note above at the GetObjectStat() call.
      // Allow TObject's to be registered again.
      SetObjectStat(statsave);

      // Register the object for special handling in the destructor.
      if (p) {
         RegisterAddressInRepository("New",p,this);
      }
   } else {
      Error("New", "This cannot happen!");
   }

   return p;
}

//______________________________________________________________________________
void *TClass::New(void *arena, ENewType defConstructor) const
{
   // Return a pointer to a newly allocated object of this class.
   // The class must have a default constructor. For meaning of
   // defConstructor, see TClass::IsCallingNew().

   void* p = 0;

   if (fNew) {
      // We have the new operator wrapper function,
      // so there is a dictionary and it was generated
      // by rootcint, so there should be a default
      // constructor we can call through the wrapper.
      fgCallingNew = defConstructor;
      p = fNew(arena);
      fgCallingNew = kRealNew;
      if (!p) {
         Error("New with placement", "cannot create object of class %s version %d at address %p", GetName(), fClassVersion, arena);
      }
   } else if (fClassInfo) {
      // We have the dictionary but do not have the
      // constructor wrapper, so the dictionary was
      // not generated by rootcint.  Let's try to
      // create the object by having the interpreter
      // call the new operator, hopefully the class
      // library is loaded and there will be a default
      // constructor we can call.
      // [This is very unlikely to work, but who knows!]
      fgCallingNew = defConstructor;
      R__LOCKGUARD2(gCINTMutex);
      p = gCint->ClassInfo_New(GetClassInfo(),arena);
      fgCallingNew = kRealNew;
      if (!p) {
         Error("New with placement", "cannot create object of class %s version %d at address %p", GetName(), fClassVersion, arena);
      }
   } else if (!fClassInfo && fCollectionProxy) {
      // There is no dictionary at all, so this is an emulated
      // class; however we do have the services of a collection proxy,
      // so this is an emulated STL class.
      fgCallingNew = defConstructor;
      p = fCollectionProxy->New(arena);
      fgCallingNew = kRealNew;
   } else if (!fClassInfo && !fCollectionProxy) {
      // There is no dictionary at all and we do not have
      // the services of a collection proxy available, so
      // use the streamer info to approximate calling a
      // constructor (basically we just make sure that the
      // pointer data members are null, unless they are marked
      // as preallocated with the "->" comment, in which case
      // we default-construct an object to point at).

      // ???BUG???  ???WHY???
      // Do not register any TObject's that we create
      // as a result of creating this object.
      Bool_t statsave = GetObjectStat();
      SetObjectStat(kFALSE);

      TVirtualStreamerInfo* sinfo = GetStreamerInfo();
      if (!sinfo) {
         Error("New with placement", "Cannot construct class '%s' version %d at address %p, no streamer info available!", GetName(), fClassVersion, arena);
         return 0;
      }

      fgCallingNew = defConstructor;
      p = sinfo->New(arena);
      fgCallingNew = kRealNew;

      // ???BUG???
      // Allow TObject's to be registered again.
      SetObjectStat(statsave);

      // Register the object for special handling in the destructor.
      if (p) {
         RegisterAddressInRepository("TClass::New with placement",p,this);
      }
   } else {
      Error("New with placement", "This cannot happen!");
   }

   return p;
}

//______________________________________________________________________________
void *TClass::NewArray(Long_t nElements, ENewType defConstructor) const
{
   // Return a pointer to a newly allocated array of objects
   // of this class.
   // The class must have a default constructor. For meaning of
   // defConstructor, see TClass::IsCallingNew().

   void* p = 0;

   if (fNewArray) {
      // We have the new operator wrapper function,
      // so there is a dictionary and it was generated
      // by rootcint, so there should be a default
      // constructor we can call through the wrapper.
      fgCallingNew = defConstructor;
      p = fNewArray(nElements, 0);
      fgCallingNew = kRealNew;
      if (!p) {
         Error("NewArray", "cannot create object of class %s version %d", GetName(), fClassVersion);
      }
   } else if (fClassInfo) {
      // We have the dictionary but do not have the
      // constructor wrapper, so the dictionary was
      // not generated by rootcint.  Let's try to
      // create the object by having the interpreter
      // call the new operator, hopefully the class
      // library is loaded and there will be a default
      // constructor we can call.
      // [This is very unlikely to work, but who knows!]
      fgCallingNew = defConstructor;
      R__LOCKGUARD2(gCINTMutex);
      p = gCint->ClassInfo_New(GetClassInfo(),nElements);
      fgCallingNew = kRealNew;
      if (!p) {
         Error("NewArray", "cannot create object of class %s version %d", GetName(), fClassVersion);
      }
   } else if (!fClassInfo && fCollectionProxy) {
      // There is no dictionary at all, so this is an emulated
      // class; however we do have the services of a collection proxy,
      // so this is an emulated STL class.
      fgCallingNew = defConstructor;
      p = fCollectionProxy->NewArray(nElements);
      fgCallingNew = kRealNew;
   } else if (!fClassInfo && !fCollectionProxy) {
      // There is no dictionary at all and we do not have
      // the services of a collection proxy available, so
      // use the streamer info to approximate calling a
      // constructor (basically we just make sure that the
      // pointer data members are null, unless they are marked
      // as preallocated with the "->" comment, in which case
      // we default-construct an object to point at).

      // ???BUG???  ???WHY???
      // Do not register any TObject's that we create
      // as a result of creating this object.
      Bool_t statsave = GetObjectStat();
      SetObjectStat(kFALSE);

      TVirtualStreamerInfo* sinfo = GetStreamerInfo();
      if (!sinfo) {
         Error("NewArray", "Cannot construct class '%s' version %d, no streamer info available!", GetName(), fClassVersion);
         return 0;
      }

      fgCallingNew = defConstructor;
      p = sinfo->NewArray(nElements);
      fgCallingNew = kRealNew;

      // ???BUG???
      // Allow TObject's to be registered again.
      SetObjectStat(statsave);

      // Register the object for special handling in the destructor.
      if (p) {
         RegisterAddressInRepository("TClass::NewArray",p,this);
      }
   } else {
      Error("NewArray", "This cannot happen!");
   }

   return p;
}

//______________________________________________________________________________
void *TClass::NewArray(Long_t nElements, void *arena, ENewType defConstructor) const
{
   // Return a pointer to a newly allocated object of this class.
   // The class must have a default constructor. For meaning of
   // defConstructor, see TClass::IsCallingNew().

   void* p = 0;

   if (fNewArray) {
      // We have the new operator wrapper function,
      // so there is a dictionary and it was generated
      // by rootcint, so there should be a default
      // constructor we can call through the wrapper.
      fgCallingNew = defConstructor;
      p = fNewArray(nElements, arena);
      fgCallingNew = kRealNew;
      if (!p) {
         Error("NewArray with placement", "cannot create object of class %s version %d at address %p", GetName(), fClassVersion, arena);
      }
   } else if (fClassInfo) {
      // We have the dictionary but do not have the constructor wrapper,
      // so the dictionary was not generated by rootcint (it was made either
      // by cint or by some external mechanism).  Let's try to create the
      // object by having the interpreter call the new operator, either the
      // class library is loaded and there is a default constructor we can
      // call, or the class is interpreted and we will call the default
      // constructor that way, or no default constructor is available and
      // we fail.
      fgCallingNew = defConstructor;
      R__LOCKGUARD2(gCINTMutex);
      p = gCint->ClassInfo_New(GetClassInfo(),nElements, arena);
      fgCallingNew = kRealNew;
      if (!p) {
         Error("NewArray with placement", "cannot create object of class %s version %d at address %p", GetName(), fClassVersion, arena);
      }
   } else if (!fClassInfo && fCollectionProxy) {
      // There is no dictionary at all, so this is an emulated
      // class; however we do have the services of a collection proxy,
      // so this is an emulated STL class.
      fgCallingNew = defConstructor;
      p = fCollectionProxy->NewArray(nElements, arena);
      fgCallingNew = kRealNew;
   } else if (!fClassInfo && !fCollectionProxy) {
      // There is no dictionary at all and we do not have
      // the services of a collection proxy available, so
      // use the streamer info to approximate calling a
      // constructor (basically we just make sure that the
      // pointer data members are null, unless they are marked
      // as preallocated with the "->" comment, in which case
      // we default-construct an object to point at).

      // ???BUG???  ???WHY???
      // Do not register any TObject's that we create
      // as a result of creating this object.
      Bool_t statsave = GetObjectStat();
      SetObjectStat(kFALSE);

      TVirtualStreamerInfo* sinfo = GetStreamerInfo();
      if (!sinfo) {
         Error("NewArray with placement", "Cannot construct class '%s' version %d at address %p, no streamer info available!", GetName(), fClassVersion, arena);
         return 0;
      }

      fgCallingNew = defConstructor;
      p = sinfo->NewArray(nElements, arena);
      fgCallingNew = kRealNew;

      // ???BUG???
      // Allow TObject's to be registered again.
      SetObjectStat(statsave);

      if (fStreamerType & kEmulated) {
         // We always register emulated objects, we need to always
         // use the streamer info to destroy them.
      }

      // Register the object for special handling in the destructor.
      if (p) {
         RegisterAddressInRepository("TClass::NewArray with placement",p,this);
      }
   } else {
      Error("NewArray with placement", "This cannot happen!");
   }

   return p;
}

//______________________________________________________________________________
void TClass::Destructor(void *obj, Bool_t dtorOnly)
{
   // Explicitly call destructor for object.

   // Do nothing if passed a null pointer.
   if (obj == 0) return;

   void* p = obj;

   if (dtorOnly && fDestructor) {
      // We have the destructor wrapper, use it.
      fDestructor(p);
   } else if ((!dtorOnly) && fDelete) {
      // We have the delete wrapper, use it.
      fDelete(p);
   } else if (fClassInfo) {
      // We have the dictionary but do not have the
      // destruct/delete wrapper, so the dictionary was
      // not generated by rootcint (it could have been
      // created by cint or by some external mechanism).
      // Let's have the interpreter call the destructor,
      // either the code will be in a loaded library,
      // or it will be interpreted, otherwise we fail
      // because there is no destructor code at all.
      if (dtorOnly) {
         gCint->ClassInfo_Destruct(fClassInfo,p);
      } else {
         gCint->ClassInfo_Delete(fClassInfo,p);
      }
   } else if (!fClassInfo && fCollectionProxy) {
      // There is no dictionary at all, so this is an emulated
      // class; however we do have the services of a collection proxy,
      // so this is an emulated STL class.
      fCollectionProxy->Destructor(p, dtorOnly);
   } else if (!fClassInfo && !fCollectionProxy) {
      // There is no dictionary at all and we do not have
      // the services of a collection proxy available, so
      // use the streamer info to approximate calling a
      // destructor.

      Bool_t inRepo = kTRUE;
      Bool_t verFound = kFALSE;

      // Was this object allocated through TClass?
      std::multiset<Version_t> knownVersions;
      std::multimap<void*, Version_t>::iterator iter = gObjectVersionRepository.find(p);
      if (iter == gObjectVersionRepository.end()) {
         // No, it wasn't, skip special version handling.
         //Error("Destructor2", "Attempt to delete unregistered object of class '%s' at address %p!", GetName(), p);
         inRepo = kFALSE;
      } else {
         //objVer = iter->second;
         for (; (iter != gObjectVersionRepository.end()) && (iter->first == p); ++iter) {
            Version_t ver = iter->second;
            knownVersions.insert(ver);
            if (ver == fClassVersion) {
               verFound = kTRUE;
            }
         }
      }

      if (!inRepo || verFound) {
         // The object was allocated using code for the same class version
         // as is loaded now.  We may proceed without worry.
         TVirtualStreamerInfo* si = GetStreamerInfo();
         if (si) {
            si->Destructor(p, dtorOnly);
         } else {
            Error("Destructor", "No streamer info available for class '%s' version %d at address %p, cannot destruct emulated object!", GetName(), fClassVersion, p);
            Error("Destructor", "length of fStreamerInfo is %d", fStreamerInfo->GetSize());
            Int_t i = fStreamerInfo->LowerBound();
            for (Int_t v = 0; v < fStreamerInfo->GetSize(); ++v, ++i) {
               Error("Destructor", "fStreamerInfo->At(%d): %p", i, fStreamerInfo->At(i));
               if (fStreamerInfo->At(i) != 0) {
                  Error("Destructor", "Doing Dump() ...");
                  ((TVirtualStreamerInfo*)fStreamerInfo->At(i))->Dump();
               }
            }
         }
      } else {
         // The loaded class version is not the same as the version of the code
         // which was used to allocate this object.  The best we can do is use
         // the TVirtualStreamerInfo to try to free up some of the allocated memory.
         Error("Destructor", "Loaded class %s version %d is not registered for addr %p", GetName(), fClassVersion, p);
#if 0
         TVirtualStreamerInfo* si = (TVirtualStreamerInfo*) fStreamerInfo->At(objVer);
         if (si) {
            si->Destructor(p, dtorOnly);
         } else {
            Error("Destructor2", "No streamer info available for class '%s' version %d, cannot destruct object at addr: %p", GetName(), objVer, p);
            Error("Destructor2", "length of fStreamerInfo is %d", fStreamerInfo->GetSize());
            Int_t i = fStreamerInfo->LowerBound();
            for (Int_t v = 0; v < fStreamerInfo->GetSize(); ++v, ++i) {
               Error("Destructor2", "fStreamerInfo->At(%d): %p", i, fStreamerInfo->At(i));
               if (fStreamerInfo->At(i) != 0) {
                  // Do some debugging output.
                  Error("Destructor2", "Doing Dump() ...");
                  ((TVirtualStreamerInfo*)fStreamerInfo->At(i))->Dump();
               }
            }
         }
#endif
      }

      if (inRepo && verFound && p) {
         UnregisterAddressInRepository("TClass::Destructor",p,this);
      }
   } else {
      Error("Destructor", "This cannot happen! (class %s)", GetName());
   }
}

//______________________________________________________________________________
void TClass::DeleteArray(void *ary, Bool_t dtorOnly)
{
   // Explicitly call operator delete[] for an array.

   // Do nothing if passed a null pointer.
   if (ary == 0) return;

   // Make a copy of the address.
   void* p = ary;

   if (fDeleteArray) {
      if (dtorOnly) {
         Error("DeleteArray", "Destructor only is not supported!");
      } else {
         // We have the array delete wrapper, use it.
         fDeleteArray(ary);
      }
   } else if (fClassInfo) {
      // We have the dictionary but do not have the
      // array delete wrapper, so the dictionary was
      // not generated by rootcint.  Let's try to
      // delete the array by having the interpreter
      // call the array delete operator, hopefully
      // the class library is loaded and there will be
      // a destructor we can call.
      R__LOCKGUARD2(gCINTMutex);
      gCint->ClassInfo_DeleteArray(GetClassInfo(),ary, dtorOnly);
   } else if (!fClassInfo && fCollectionProxy) {
      // There is no dictionary at all, so this is an emulated
      // class; however we do have the services of a collection proxy,
      // so this is an emulated STL class.
      fCollectionProxy->DeleteArray(ary, dtorOnly);
   } else if (!fClassInfo && !fCollectionProxy) {
      // There is no dictionary at all and we do not have
      // the services of a collection proxy available, so
      // use the streamer info to approximate calling the
      // array destructor.

      Bool_t inRepo = kTRUE;
      Bool_t verFound = kFALSE;

      // Was this array object allocated through TClass?
      std::multiset<Version_t> knownVersions;
      std::multimap<void*, Version_t>::iterator iter = gObjectVersionRepository.find(p);
      if (iter == gObjectVersionRepository.end()) {
         // No, it wasn't, we cannot know what to do.
         //Error("DeleteArray", "Attempt to delete unregistered array object, element type '%s', at address %p!", GetName(), p);
         inRepo = kFALSE;
      } else {
         for (; (iter != gObjectVersionRepository.end()) && (iter->first == p); ++iter) {
            Version_t ver = iter->second;
            knownVersions.insert(ver);
            if (ver == fClassVersion) {
               verFound = kTRUE;
            }
         }
      }

      if (!inRepo || verFound) {
         // The object was allocated using code for the same class version
         // as is loaded now.  We may proceed without worry.
         TVirtualStreamerInfo* si = GetStreamerInfo();
         if (si) {
            si->DeleteArray(ary, dtorOnly);
         } else {
            Error("DeleteArray", "No streamer info available for class '%s' version %d at address %p, cannot destruct object!", GetName(), fClassVersion, ary);
            Error("DeleteArray", "length of fStreamerInfo is %d", fStreamerInfo->GetSize());
            Int_t i = fStreamerInfo->LowerBound();
            for (Int_t v = 0; v < fStreamerInfo->GetSize(); ++v, ++i) {
               Error("DeleteArray", "fStreamerInfo->At(%d): %p", v, fStreamerInfo->At(i));
               if (fStreamerInfo->At(i)) {
                  Error("DeleteArray", "Doing Dump() ...");
                  ((TVirtualStreamerInfo*)fStreamerInfo->At(i))->Dump();
               }
            }
         }
      } else {
         // The loaded class version is not the same as the version of the code
         // which was used to allocate this array.  The best we can do is use
         // the TVirtualStreamerInfo to try to free up some of the allocated memory.
         Error("DeleteArray", "Loaded class version %d is not registered for addr %p", fClassVersion, p);



#if 0
         TVirtualStreamerInfo* si = (TVirtualStreamerInfo*) fStreamerInfo->At(objVer);
         if (si) {
            si->DeleteArray(ary, dtorOnly);
         } else {
            Error("DeleteArray", "No streamer info available for class '%s' version %d at address %p, cannot destruct object!", GetName(), objVer, ary);
            Error("DeleteArray", "length of fStreamerInfo is %d", fStreamerInfo->GetSize());
            Int_t i = fStreamerInfo->LowerBound();
            for (Int_t v = 0; v < fStreamerInfo->GetSize(); ++v, ++i) {
               Error("DeleteArray", "fStreamerInfo->At(%d): %p", v, fStreamerInfo->At(i));
               if (fStreamerInfo->At(i)) {
                  // Print some debugging info.
                  Error("DeleteArray", "Doing Dump() ...");
                  ((TVirtualStreamerInfo*)fStreamerInfo->At(i))->Dump();
               }
            }
         }
#endif


      }

      // Deregister the object for special handling in the destructor.
      if (inRepo && verFound && p) {
         UnregisterAddressInRepository("TClass::DeleteArray",p,this);
      }
   } else {
      Error("DeleteArray", "This cannot happen! (class '%s')", GetName());
   }
}

//______________________________________________________________________________
void TClass::SetClassVersion(Version_t version) 
{ 
   // Private function.  Set the class version for the 'class' represented by
   // this TClass object.  See the public interface: 
   //    ROOT::ResetClassVersion 
   // defined in TClassTable.cxx
   //
   // Note on class version numbers:
   //   If no class number has been specified, TClass::GetVersion will return -1
   //   The Class Version 0 request the whole object to be transient
   //   The Class Version 1, unless specified via ClassDef indicates that the
   //      I/O should use the TClass checksum to distinguish the layout of the class   
   
   fClassVersion = version; 
   fCurrentInfo = 0; 
}

//______________________________________________________________________________
void TClass::SetCurrentStreamerInfo(TVirtualStreamerInfo *info)
{
   // Set pointer to current TVirtualStreamerInfo

   fCurrentInfo = info;
}

//______________________________________________________________________________
Int_t TClass::Size() const
{
   // Return size of object of this class.

   if (fSizeof!=-1) return fSizeof;
   if (fCollectionProxy) return fCollectionProxy->Sizeof();
   if (fClassInfo) return gCint->ClassInfo_Size(GetClassInfo());
   return GetStreamerInfo()->GetSize();
}

//______________________________________________________________________________
TClass *TClass::Load(TBuffer &b)
{
   // Load class description from I/O buffer and return class object.

   UInt_t maxsize = 256;
   char *s = new char[maxsize];

   Int_t pos = b.Length();

   b.ReadString(s, maxsize); // Reads at most maxsize - 1 characters, plus null at end.
   while (strlen(s) == (maxsize - 1)) {
      // The classname is too large, try again with a large buffer.
      b.SetBufferOffset(pos);
      maxsize = 2*maxsize;
      delete [] s;
      s = new char[maxsize];
      b.ReadString(s, maxsize); // Reads at most maxsize - 1 characters, plus null at end.
   }

   TClass *cl = TClass::GetClass(s, kTRUE);
   if (!cl)
      ::Error("TClass::Load", "dictionary of class %s not found", s);

   delete [] s;
   return cl;
}

//______________________________________________________________________________
void TClass::Store(TBuffer &b) const
{
   // Store class description on I/O buffer.

   b.WriteString(GetName());
}

//______________________________________________________________________________
TClass *ROOT::CreateClass(const char *cname, Version_t id,
                          const type_info &info, TVirtualIsAProxy *isa,
                          ShowMembersFunc_t show,
                          const char *dfil, const char *ifil,
                          Int_t dl, Int_t il)
{
   // Global function called by a class' static Dictionary() method
   // (see the ClassDef macro).

   // When called via TMapFile (e.g. Update()) make sure that the dictionary
   // gets allocated on the heap and not in the mapped file.
   if (gMmallocDesc) {
      void *msave  = gMmallocDesc;
      gMmallocDesc = 0;
      TClass *cl   = new TClass(cname, id, info, isa, show, dfil, ifil, dl, il);
      gMmallocDesc = msave;
      return cl;
   }
   return new TClass(cname, id, info, isa, show, dfil, ifil, dl, il);
}

//______________________________________________________________________________
TClass *ROOT::CreateClass(const char *cname, Version_t id,
                          const char *dfil, const char *ifil,
                          Int_t dl, Int_t il)
{
   // Global function called by a class' static Dictionary() method
   // (see the ClassDef macro).

   // When called via TMapFile (e.g. Update()) make sure that the dictionary
   // gets allocated on the heap and not in the mapped file.
   if (gMmallocDesc) {
      void *msave  = gMmallocDesc;
      gMmallocDesc = 0;
      TClass *cl   = new TClass(cname, id, dfil, ifil, dl, il);
      gMmallocDesc = msave;
      return cl;
   }
   return new TClass(cname, id, dfil, ifil, dl, il);
}

//______________________________________________________________________________
TClass::ENewType TClass::IsCallingNew()
{
   // Static method returning the defConstructor flag passed to TClass::New().
   // New type is either:
   //   TClass::kRealNew  - when called via plain new
   //   TClass::kClassNew - when called via TClass::New()
   //   TClass::kDummyNew - when called via TClass::New() but object is a dummy,
   //                       in which case the object ctor might take short cuts

   return fgCallingNew;
}

//______________________________________________________________________________
Bool_t TClass::IsLoaded() const
{
   // Return true if the shared library of this class is currently in the a
   // process's memory.  Return false, after the shared library has been
   // unloaded or if this is an 'emulated' class created from a file's StreamerInfo.

   return (GetImplFileLine()>=0 && !TestBit(kUnloaded));
}

//______________________________________________________________________________
Bool_t  TClass::IsStartingWithTObject() const
{
   // Returns true if this class inherits from TObject and if the start of
   // the TObject parts is at the very beginning of the objects.
   // Concretly this means that the following code is proper for this class:
   //     ThisClass *ptr;
   //     void *void_ptr = (void)ptr;
   //     TObject *obj = (TObject*)void_ptr;
   // This code would be wrong if 'ThisClass' did not inherit 'first' from
   // TObject.

   if (fProperty==(-1)) Property();
   return TestBit(kStartWithTObject);
}

//______________________________________________________________________________
Bool_t  TClass::IsTObject() const
{
   // Return kTRUE is the class inherits from TObject.

   if (fProperty==(-1)) Property();
   return TestBit(kIsTObject);
}

//______________________________________________________________________________
Bool_t  TClass::IsForeign() const
{
   // Return kTRUE is the class is Foreign (the class does not have a Streamer method).

   if (fProperty==(-1)) Property();
   return TestBit(kIsForeign);
}

//______________________________________________________________________________
void TClass::PostLoadCheck()
{
   // Do the initialization that can only be done after the CINT dictionary has
   // been fully populated and can not be delayed efficiently.

   // In the case of a Foreign class (loaded class without a Streamer function)
   // we reset fClassVersion to be -1 so that the current TVirtualStreamerInfo will not
   // be confused with a previously loaded streamerInfo.

   if (IsLoaded() && fClassInfo && fClassVersion==1 /*&& fStreamerInfo
       && fStreamerInfo->At(1)*/ && IsForeign() )
   {
      SetClassVersion(-1);
   }
   else if (IsLoaded() && fClassInfo && fStreamerInfo && (!IsForeign()||fClassVersion>1) )
   {
      TVirtualStreamerInfo *info = (TVirtualStreamerInfo*)(fStreamerInfo->At(fClassVersion));
      // Here we need to check whether this TVirtualStreamerInfo (which presumably has been
      // loaded from a file) is consisten with the definition in the library we just loaded.
      // BuildCheck is not appropriate here since it check a streamerinfo against the
      // 'current streamerinfo' which, at time point, would be the same as 'info'!
      if (info && GetListOfDataMembers() && !GetCollectionProxy()
          && (info->GetCheckSum()!=GetCheckSum() && info->GetCheckSum()!=GetCheckSum(1) && info->GetCheckSum()!=GetCheckSum(2)))
      {
         Bool_t warn = ! TestBit(kWarned);
         if (warn && info->GetOldVersion()<=2) {
            // Names of STL base classes was modified in vers==3. Allocators removed
            //
            TIter nextBC(GetListOfBases());
            TBaseClass *bc;
            while ((bc=(TBaseClass*)nextBC()))
            {if (TClassEdit::IsSTLCont(bc->GetName())) warn = kFALSE;}
         }

         if (warn) {
            if (info->GetOnFileClassVersion()==1 && fClassVersion>1) {
               Warning("PostLoadCheck","\n\
   The class %s transitioned from not having a specified class version\n\
   to having a specified class version (the current class version is %d).\n\
   However too many different non-versioned layouts of the class have\n\
   already been loaded so far.  To work around this problem you can\n\
   load fewer 'old' file in the same ROOT session or load the C++ library\n\
   describing the class %s before opening the files or increase the version\n\
   number of the class for example ClassDef(%s,%d).\n\
   Do not try to write objects with the current class definition,\n\
   the files might not be readable.\n",
                       GetName(), fClassVersion, GetName(), GetName(), fStreamerInfo->GetLast()+1);
            } else {
               Warning("PostLoadCheck","\n\
   The StreamerInfo version %d for the class %s which was read\n\
   from a file previously opened has the same version as the active class\n\
   but a different checksum. You should update the version to ClassDef(%s,%d).\n\
   Do not try to write objects with the current class definition,\n\
   the files will not be readable.\n"
                       , fClassVersion, GetName(), GetName(), fStreamerInfo->GetLast()+1);
            }
            SetBit(kWarned);
         }
      }
   }
}

//______________________________________________________________________________
Long_t TClass::Property() const
{
   // Set TObject::fBits and fStreamerType to cache information about the
   // class.  The bits are
   //    kIsTObject : the class inherits from TObject
   //    kStartWithTObject:  TObject is the left-most class in the inheritance tree
   //    kIsForeign : the class doe not have a Streamer method
   // The value of fStreamerType are
   //    kTObject : the class inherits from TObject
   //    kForeign : the class does not have a Streamer method
   //    kInstrumented: the class does have a Streamer method
   //    kExternal: the class has a free standing way of streaming itself
   //    kEmulated: the class is missing its shared library.

   if (fProperty!=(-1)) return fProperty;

   Long_t dummy;
   TClass *kl = const_cast<TClass*>(this);

   kl->fStreamerType = kNone;

   if (InheritsFrom(TObject::Class())) {
      kl->SetBit(kIsTObject);

      // Is it DIRECT inheritance from TObject?
      Int_t delta = kl->GetBaseClassOffset(TObject::Class());
      if (delta==0) kl->SetBit(kStartWithTObject);

      kl->fStreamerType  = kTObject;
   }

   if (fClassInfo) {

      kl->fProperty = gCint->ClassInfo_Property(fClassInfo);

      if (!gCint->ClassInfo_HasMethod(fClassInfo,"Streamer") ||
          !gCint->ClassInfo_IsValidMethod(fClassInfo,"Streamer","TBuffer&",&dummy) ) {

         kl->SetBit(kIsForeign);
         kl->fStreamerType  = kForeign;

      } else if ( kl->fStreamerType == kNone ) {
         if (strcmp( gCint->ClassInfo_FileName(fClassInfo),"{CINTEX dictionary translator}")==0) {
            kl->SetBit(kIsForeign);
         }
         kl->fStreamerType  = kInstrumented;
      }

      if (fStreamer)   kl->fStreamerType  = kExternal;

   } else {

      if (fStreamer)   kl->fStreamerType  = kExternal;

      kl->fStreamerType |= kEmulated;

      return 0;
   }

   return fProperty;
}

//_____________________________________________________________________________
void TClass::SetCollectionProxy(const ROOT::TCollectionProxyInfo &info)
{
   // Create the collection proxy object (and the streamer object) from
   // using the information in the TCollectionProxyInfo.

   delete fCollectionProxy;

   // We can not use GetStreamerInfo() instead of TVirtualStreamerInfo::Factory()
   // because GetStreamerInfo call TStreamerInfo::Build which need to have fCollectionProxy
   // set correctly.

   TVirtualCollectionProxy *p = TVirtualStreamerInfo::Factory()->GenExplicitProxy(info,this);
   fCollectionProxy = p;

   AdoptStreamer(TVirtualStreamerInfo::Factory()->GenExplicitClassStreamer(info,this));
}

//______________________________________________________________________________
void TClass::SetContextMenuTitle(const char *title)
{
   // Change (i.e. set) the title of the TNamed.

   fContextMenuTitle = title;
}

//______________________________________________________________________________
void TClass::SetGlobalIsA(IsAGlobalFunc_t func)
{
   // This function installs a global IsA function for this class.
   // The global IsA function will be used if there is no local IsA function (fIsA)
   //
   // A global IsA function has the signature:
   //
   //    TClass *func( TClass *cl, const void *obj);
   //
   // 'cl' is a pointer to the  TClass object that corresponds to the
   // 'pointer type' used to retrieve the value 'obj'
   //
   //  For example with:
   //    TNamed * m = new TNamed("example","test");
   //    TObject* o = m
   // and
   //    the global IsA function would be called with TObject::Class() as
   //    the first parameter and the exact numerical value in the pointer
   //    'o'.
   //
   //  In other word, inside the global IsA function. it is safe to C-style
   //  cast the value of 'obj' into a pointer to the class described by 'cl'.

   fGlobalIsA = func;
}

//______________________________________________________________________________
void TClass::SetUnloaded()
{
   // Call this method to indicate that the shared library containing this
   // class's code has been removed (unloaded) from the process's memory

   delete fIsA; fIsA = 0;
   // Disable the autoloader while calling SetClassInfo, to prevent
   // the library from being reloaded!
   int autoload_old = gCint->SetClassAutoloading(0);
   gInterpreter->SetClassInfo(this,kTRUE);
   gCint->SetClassAutoloading(autoload_old);
   fDeclFileName = 0;
   fDeclFileLine = 0;
   fImplFileName = 0;
   fImplFileLine = 0;
   fTypeInfo     = 0;

   if (fMethod) {
      fMethod->Delete();
      delete fMethod;
      fMethod=0;
   }
  
   SetBit(kUnloaded);
}

//______________________________________________________________________________
TVirtualStreamerInfo *TClass::SetStreamerInfo(Int_t /*version*/, const char * /*info*/)
{
   // Info is a string describing the names and types of attributes
   // written by the class Streamer function.
   // If info is an empty string (when called by TObject::StreamerInfo)
   // the default Streamer info string is build. This corresponds to
   // the case of an automatically generated Streamer.
   // In case of user defined Streamer function, it is the user responsability
   // to implement a StreamerInfo function (override TObject::StreamerInfo).
   // The user must call IsA()->SetStreamerInfo(info) from this function.

   // info is specified, nothing to do, except that we should verify
   // that it contains a valid descriptor.

/*
   TDataMember *dm;
   Int_t nch = strlen(info);
   Bool_t update = kTRUE;
   if (nch != 0) {
      //decode strings like "TObject;TAttLine;fA;fB;Int_t i,j,k;"
      char *save, *temp, *blank, *colon, *comma;
      save = new char[10000];
      temp = save;
      strcpy(temp,info);
      //remove heading and trailing blanks
      while (*temp == ' ') temp++;
      while (save[nch-1] == ' ') {nch--; save[nch] = 0;}
      if (nch == 0) {delete [] save; return;}
      if (save[nch-1] != ';') {save[nch] = ';'; save[nch+1] = 0;}
      //remove blanks around , or ;
      while ((blank = strstr(temp,"; "))) strcpy(blank+1,blank+2);
      while ((blank = strstr(temp," ;"))) strcpy(blank,  blank+1);
      while ((blank = strstr(temp,", "))) strcpy(blank+1,blank+2);
      while ((blank = strstr(temp," ,"))) strcpy(blank,  blank+1);
      while ((blank = strstr(temp,"  "))) strcpy(blank,  blank+1);
      //loop on tokens separated by ;
      char *final = new char[1000];
      char token[100];
      while ((colon=strchr(temp,';'))) {
         *colon = 0;
         strcpy(token,temp);
         blank = strchr(token,' ');
         if (blank) {
            *blank = 0;
            if (!gROOT->GetType(token)) {
               Error("SetStreamerInfo","Illegal type: %s in %s",token,info);
               return;
            }
            while (blank) {
               strcat(final,token);
               strcat(final," ");
               comma = strchr(blank+1,','); if (comma) *comma=0;
               strcat(final,blank+1);
               strcat(final,";");
               blank = comma;
            }

         } else {
            if (TClass::GetClass(token,update)) {
               //a class name
               strcat(final,token); strcat(final,";");
            } else {
               //a data member name
               dm = (TDataMember*)GetListOfDataMembers()->FindObject(token);
               if (dm) {
                  strcat(final,dm->GetFullTypeName());
                  strcat(final," ");
                  strcat(final,token); strcat(final,";");
               } else {
                  Error("SetStreamerInfo","Illegal name: %s in %s",token,info);
                  return;
               }
            }
            update = kFALSE;
         }
         temp = colon+1;
         if (*temp == 0) break;
      }
 ////     fStreamerInfo = final;
      delete [] final;
      delete [] save;
      return;
   }

   //info is empty. Let's build the default Streamer descriptor

   char *temp = new char[10000];
   temp[0] = 0;
   char local[100];

   //add list of base classes
   TIter nextb(GetListOfBases());
   TBaseClass *base;
   while ((base = (TBaseClass*) nextb())) {
      sprintf(local,"%s;",base->GetName());
      strcat(temp,local);
   }

   //add list of data members and types
   TIter nextd(GetListOfDataMembers());
   while ((dm = (TDataMember *) nextd())) {
      if (dm->IsEnum()) continue;
      if (!dm->IsPersistent()) continue;
      Long_t property = dm->Property();
      if (property & kIsStatic) continue;
      TClass *acl = TClass::GetClass(dm->GetTypeName(),update);
      update = kFALSE;
      if (acl) {
         if (acl->GetClassVersion() == 0) continue;
      }

      // dm->GetArrayIndex() returns an empty string if it does not
      // applies
      const char * index = dm->GetArrayIndex();
      if (strlen(index)==0)
         sprintf(local,"%s %s;",dm->GetFullTypeName(),dm->GetName());
      else
         sprintf(local,"%s %s[%s];",dm->GetFullTypeName(),dm->GetName(),index);
      strcat(temp,local);
   }
   //fStreamerInfo = temp;
   delete [] temp;
*/
   return 0;
}

//______________________________________________________________________________
UInt_t TClass::GetCheckSum(UInt_t code) const
{
   // Compute and/or return the class check sum.
   // The class ckecksum is used by the automatic schema evolution algorithm
   // to uniquely identify a class version.
   // The check sum is built from the names/types of base classes and
   // data members.
   // Algorithm from Victor Perevovchikov (perev@bnl.gov).
   //
   // if code==1 data members of type enum are not counted in the checksum
   // if code==2 return the checksum of data members and base classes, not including the ranges and array size found in comments.  
   //            This is needed for backward compatibility.
   //
   // WARNING: this function must be kept in sync with TClass::GetCheckSum.
   // They are both used to handle backward compatibility and should both return the same values.
   // TStreamerInfo uses the information in TStreamerElement while TClass uses the information
   // from TClass::GetListOfBases and TClass::GetListOfDataMembers.

   if (fCheckSum && code == 0) return fCheckSum;

   UInt_t id = 0;

   int il;
   TString name = GetName();
   TString type;
   il = name.Length();
   for (int i=0; i<il; i++) id = id*3+name[i];

   TList *tlb = ((TClass*)this)->GetListOfBases();
   if (tlb) {   // Loop over bases

      TIter nextBase(tlb);

      TBaseClass *tbc=0;
      while((tbc=(TBaseClass*)nextBase())) {
         name = tbc->GetName();
         il = name.Length();
         for (int i=0; i<il; i++) id = id*3+name[i];
      }/*EndBaseLoop*/
   }
   TList *tlm = ((TClass*)this)->GetListOfDataMembers();
   if (tlm) {   // Loop over members
      TIter nextMemb(tlm);
      TDataMember *tdm=0;
      Long_t prop = 0;
      while((tdm=(TDataMember*)nextMemb())) {
         if (!tdm->IsPersistent())        continue;
         //  combine properties
         prop = (tdm->Property());
         TDataType* tdt = tdm->GetDataType();
         if (tdt) prop |= tdt->Property();

         if ( prop&kIsStatic)             continue;
         name = tdm->GetName(); il = name.Length();
         if ( (code != 1) && prop&kIsEnum) id = id*3 + 1;

         int i;
         for (i=0; i<il; i++) id = id*3+name[i];
         type = tdm->GetFullTypeName();
         if (TClassEdit::IsSTLCont(type))
            type = TClassEdit::ShortType( type, TClassEdit::kDropStlDefault );

         il = type.Length();
         for (i=0; i<il; i++) id = id*3+type[i];

         int dim = tdm->GetArrayDim();
         if (prop&kIsArray) {
            for (int ii=0;ii<dim;ii++) id = id*3+tdm->GetMaxIndex(ii);
         }
         if (code != 2) {
            const char *left = strstr(tdm->GetTitle(),"[");
            if (left) {
               const char *right = strstr(left,"]");
               if (right) {
                  ++left;
                  while (left != right) {
                     id = id*3 + *left;
                     ++left;
                  }
               }
            }
         }
      }/*EndMembLoop*/
   }
   if (code==0) fCheckSum = id;
   return id;
}

//______________________________________________________________________________
void TClass::AdoptReferenceProxy(TVirtualRefProxy* proxy)
{
   // Adopt the Reference proxy pointer to indicate that this class
   // represents a reference.
   // When a new proxy is adopted, the old one is deleted.

   if ( fRefProxy )  {
      fRefProxy->Release();
   }
   fRefProxy = proxy;
   if ( fRefProxy )  {
      fRefProxy->SetClass(this);
   }
}

//______________________________________________________________________________
void TClass::AdoptMemberStreamer(const char *name, TMemberStreamer *p)
{
   // Adopt the TMemberStreamer pointer to by p and use it to Stream non basic
   // member name.

   if (!fRealData) return;
   TIter next(fRealData);
   TRealData *rd;
   while ((rd = (TRealData*)next())) {
      if (strcmp(rd->GetName(),name) == 0) {
         // If there is a TStreamerElement that took a pointer to the
         // streamer we should inform it!
         rd->AdoptStreamer(p);
         break;
      }
   }

//  NOTE: This alternative was proposed but not is not used for now,
//  One of the major difference with the code above is that the code below
//  did not require the RealData to have been built
//    if (!fData) return;
//    const char *n = name;
//    while (*n=='*') n++;
//    TString ts(n);
//    int i = ts.Index("[");
//    if (i>=0) ts.Remove(i,999);
//    TDataMember *dm = (TDataMember*)fData->FindObject(ts.Data());
//    if (!dm) {
//       Warning("SetStreamer","Can not find member %s::%s",GetName(),name);
//       return;
//    }
//    dm->SetStreamer(p);
   return;
}

//______________________________________________________________________________
void TClass::SetMemberStreamer(const char *name, MemberStreamerFunc_t p)
{
   // Install a new member streamer (p will be copied).

   AdoptMemberStreamer(name,new TMemberStreamer(p));
}

//______________________________________________________________________________
Int_t TClass::ReadBuffer(TBuffer &b, void *pointer, Int_t version, UInt_t start, UInt_t count)
{
   // Function called by the Streamer functions to deserialize information
   // from buffer b into object at p.
   // This function assumes that the class version and the byte count information
   // have been read.
   //   version  is the version number of the class
   //   start    is the starting position in the buffer b
   //   count    is the number of bytes for this object in the buffer

   return b.ReadClassBuffer(this,pointer,version,start,count);
}

//______________________________________________________________________________
Int_t TClass::ReadBuffer(TBuffer &b, void *pointer)
{
   // Function called by the Streamer functions to deserialize information
   // from buffer b into object at p.

   return b.ReadClassBuffer(this,pointer);
}

//______________________________________________________________________________
Int_t TClass::WriteBuffer(TBuffer &b, void *pointer, const char * /*info*/)
{
   // Function called by the Streamer functions to serialize object at p
   // to buffer b. The optional argument info may be specified to give an
   // alternative StreamerInfo instead of using the default StreamerInfo
   // automatically built from the class definition.
   // For more information, see class TVirtualStreamerInfo.

   b.WriteClassBuffer(this,pointer);
   return 0;
}

//______________________________________________________________________________
void TClass::Streamer(void *object, TBuffer &b, const TClass *onfile_class) const
{
   // Stream object of this class to or from buffer.

   switch (fStreamerType) {

      case kExternal:
      case kExternal|kEmulated: 
      {
         //There is special streamer for the class
         TClassStreamer *streamer = GetStreamer();
         streamer->SetOnFileClass( onfile_class );
         (*streamer)(b,object);

         return;
      }

      case kTObject:
      {
         if (!fInterStreamer) {
            CalculateStreamerOffset();
         }
         TObject *tobj = (TObject*)((Long_t)object + fOffsetStreamer);
         tobj->Streamer(b);
      }
      return;

      case kTObject|kEmulated : {
         b.ReadClassEmulated(this, object, onfile_class);
      }
      return;

      case kInstrumented: // Instrumented class with a library
      {
         CallFunc_t *func = (CallFunc_t*)fInterStreamer;

         if (!func)  {
            func  = gCint->CallFunc_Factory();
            gCint->CallFunc_SetFuncProto(func,fClassInfo,"Streamer","TBuffer&",&fOffsetStreamer);
            fInterStreamer = func;
         } else {
            // Reset the argument list!
            gCint->CallFunc_SetArgs(func,"");
         }

         // set arguments
         gCint->CallFunc_SetArg(func,(Long_t)&b);
         // call function
         gCint->CallFunc_Exec(func,(char*)((Long_t)object + fOffsetStreamer) );

      }
      return;

      case kForeign:
      case kForeign|kEmulated:
      case kInstrumented|kEmulated:
      case kEmulated:
      {
         if (b.IsReading()) {
            b.ReadClassBuffer(this, object, onfile_class);
            //ReadBuffer (b, object);
         } else {
            //WriteBuffer(b, object);
            b.WriteClassBuffer(this, object);
         }
      }
      return;

      default:
      {
         if (fProperty==(-1)) {
            Property();
            Streamer(object,b);
         } else {
            Fatal("Streamer", "fStreamerType not properly initialized (%d)", fStreamerType);
         }
      }
   }
}

//______________________________________________________________________________
void TClass::AdoptStreamer(TClassStreamer *str)
{
   // Adopt a TClassStreamer object.  Ownership is transfered to this TClass
   // object.

//    // This code can be used to quickly test the STL Emulation layer
//    Int_t k = TClassEdit::IsSTLCont(GetName());
//    if (k==1||k==-1) { delete str; return; }

   if (fStreamer) delete fStreamer;
   if (str) {
      fStreamerType = kExternal | ( fStreamerType&kEmulated );
      fStreamer = str;
   } else if (fStreamer) {
      // Case where there was a custom streamer and it is hereby removed,
      // we need to reset fStreamerType
      fStreamer = str;
      fStreamerType = kNone;
      fProperty = -1;
      Property();
   }
}

//______________________________________________________________________________
void TClass::SetNew(ROOT::NewFunc_t newFunc)
{
   // Install a new wrapper around 'new'.

   fNew = newFunc;
}

//______________________________________________________________________________
void TClass::SetNewArray(ROOT::NewArrFunc_t newArrayFunc)
{
   // Install a new wrapper around 'new []'.

   fNewArray = newArrayFunc;
}

//______________________________________________________________________________
void TClass::SetDelete(ROOT::DelFunc_t deleteFunc)
{
   // Install a new wrapper around 'delete'.

   fDelete = deleteFunc;
}

//______________________________________________________________________________
void TClass::SetDeleteArray(ROOT::DelArrFunc_t deleteArrayFunc)
{
   // Install a new wrapper around 'delete []'.

   fDeleteArray = deleteArrayFunc;
}

//______________________________________________________________________________
void TClass::SetDestructor(ROOT::DesFunc_t destructorFunc)
{
   // Install a new wrapper around the destructor.

   fDestructor = destructorFunc;
}

//______________________________________________________________________________
void TClass::SetDirectoryAutoAdd(ROOT::DirAutoAdd_t autoAddFunc)
{
   // Install a new wrapper around the directory auto add function..
   // The function autoAddFunc has the signature void (*)(void *obj, TDirectory dir)
   // and should register 'obj' to the directory if dir is not null
   // and unregister 'obj' from its current directory if dir is null

   fDirAutoAdd = autoAddFunc;
}

//______________________________________________________________________________
TVirtualStreamerInfo *TClass::FindStreamerInfo( UInt_t checksum) const
{
   // Find the TVirtualStreamerInfo in the StreamerInfos corresponding to checksum
   return FindStreamerInfo( GetStreamerInfos(), checksum );
}

//______________________________________________________________________________
TVirtualStreamerInfo *TClass::FindStreamerInfo( TObjArray* arr, UInt_t checksum) const
{
   // Find the TVirtualStreamerInfo in the StreamerInfos corresponding to checksum

   Int_t ninfos = arr->GetEntriesFast()-1;
   for (Int_t i=-1;i<ninfos;i++) {
      // TClass::fStreamerInfos has a lower bound not equal to 0,
      // so we have to use At and should not use UncheckedAt
      TVirtualStreamerInfo *info = (TVirtualStreamerInfo*)arr->UncheckedAt(i);
      if (!info) continue;
      if (info->GetCheckSum() == checksum) {
         R__ASSERT(i==info->GetClassVersion() || (i==-1&&info->GetClassVersion()==1));
         return info;
      }
   }
   return 0;
}

//______________________________________________________________________________
TVirtualStreamerInfo *TClass::GetConversionStreamerInfo( const char* classname, Int_t version ) const
{
   // Return a Conversion StreamerInfo from the class 'classname' for version number 'version' to this class, if any.
   
   TClass *cl = TClass::GetClass( classname );
   if( !cl )
      return 0;
   return GetConversionStreamerInfo( cl, version );
}

//______________________________________________________________________________
TVirtualStreamerInfo *TClass::GetConversionStreamerInfo( const TClass* cl, Int_t version ) const
{
   // Return a Conversion StreamerInfo from the class represened by cl for version number 'version' to this class, if any.

   //----------------------------------------------------------------------------
   // Check if the classname was specified correctly
   //----------------------------------------------------------------------------
   if( !cl )
      return 0;

   if( cl == this )
      return GetStreamerInfo( version );

   //----------------------------------------------------------------------------
   // Check if we already have it
   //----------------------------------------------------------------------------
   TObjArray* arr = 0;
   if (fConversionStreamerInfo) {
      std::map<std::string, TObjArray*>::iterator it;

      it = fConversionStreamerInfo->find( cl->GetName() );

      if( it != fConversionStreamerInfo->end() ) {
         arr = it->second;
      }

      if( version > -1 && version < arr->GetSize() && arr->At( version ) )
         return (TVirtualStreamerInfo*) arr->At( version );
   }

   //----------------------------------------------------------------------------
   // We don't have the streamer info so find it in other class
   //----------------------------------------------------------------------------
   TObjArray *clSI = cl->GetStreamerInfos();
   TVirtualStreamerInfo* info = 0;
   if( version > -1 && version < clSI->GetSize() )
      info = (TVirtualStreamerInfo*)clSI->At( version );

   if( !info )
      return 0;

   //----------------------------------------------------------------------------
   // We have the right info so we need to clone it to create new object with
   // non artificial streamer elements and we should build it for current class
   //----------------------------------------------------------------------------
   info = (TVirtualStreamerInfo*)info->Clone();

   if( !info->BuildFor( this ) ) {
      delete info;
      return 0;
   }

   if (!info->GetOffsets()) {
      // Streamer info has not been compiled, but exists.
      // Therefore it was read in from a file and we have to do schema evolution?
      // Or it didn't have a dictionary before, but does now?
      info->BuildOld();
   }
   if (info->IsOptimized() && !TVirtualStreamerInfo::CanOptimize()) {
      // Undo optimization if the global flag tells us to.
      info->Compile();
   }
   
   //----------------------------------------------------------------------------
   // Cache this treamer info
   //----------------------------------------------------------------------------
   if (!arr) {
      arr = new TObjArray(version+10, -1);
      if (!fConversionStreamerInfo) {
         fConversionStreamerInfo = new std::map<std::string, TObjArray*>();
      }
      (*fConversionStreamerInfo)[cl->GetName()] = arr;
   }
   arr->AddAtAndExpand( info, info->GetClassVersion() );
   return info;
}

//______________________________________________________________________________
TVirtualStreamerInfo *TClass::FindConversionStreamerInfo( const char* classname, UInt_t checksum ) const
{
   // Return a Conversion StreamerInfo from the class 'classname' for the layout represented by 'checksum' to this class, if any.

   TClass *cl = TClass::GetClass( classname );
   if( !cl )
      return 0;
   return FindConversionStreamerInfo( cl, checksum );
}

//______________________________________________________________________________
TVirtualStreamerInfo *TClass::FindConversionStreamerInfo( const TClass* cl, UInt_t checksum ) const
{
   // Return a Conversion StreamerInfo from the class represened by cl for the layout represented by 'checksum' to this class, if any.

   //---------------------------------------------------------------------------
   // Check if the classname was specified correctly
   //---------------------------------------------------------------------------
   if( !cl )
      return 0;

   if( cl == this )
      return FindStreamerInfo( checksum );

   //----------------------------------------------------------------------------
   // Check if we already have it
   //----------------------------------------------------------------------------
   TObjArray* arr = 0;
   TVirtualStreamerInfo* info = 0;
   if (fConversionStreamerInfo) {
      std::map<std::string, TObjArray*>::iterator it;
      
      it = fConversionStreamerInfo->find( cl->GetName() );
      
      if( it != fConversionStreamerInfo->end() ) {
         arr = it->second;
      }
      info = FindStreamerInfo( arr, checksum );
   }

   if( info )
      return info;

   //----------------------------------------------------------------------------
   // Get it from the foreign class
   //----------------------------------------------------------------------------
   info = cl->FindStreamerInfo( checksum );

   if( !info )
      return 0;

   //----------------------------------------------------------------------------
   // We have the right info so we need to clone it to create new object with
   // non artificial streamer elements and we should build it for current class
   //----------------------------------------------------------------------------
   info = (TVirtualStreamerInfo*)info->Clone();
   if( !info->BuildFor( this ) ) {
      delete info;
      return 0;
   }

   if (!info->GetOffsets()) {
      // Streamer info has not been compiled, but exists.
      // Therefore it was read in from a file and we have to do schema evolution?
      // Or it didn't have a dictionary before, but does now?
      info->BuildOld();
   }
   if (info->IsOptimized() && !TVirtualStreamerInfo::CanOptimize()) {
      // Undo optimization if the global flag tells us to.
      info->Compile();
   }
   
   //----------------------------------------------------------------------------
   // Cache this treamer info
   //----------------------------------------------------------------------------
   if (!arr) {
      arr = new TObjArray(16, -1);
      if (!fConversionStreamerInfo) {
         fConversionStreamerInfo = new std::map<std::string, TObjArray*>();
      }
      (*fConversionStreamerInfo)[cl->GetName()] = arr;
   }
   arr->AddAtAndExpand( info, info->GetClassVersion() );

   return info;
}

//______________________________________________________________________________
Bool_t TClass::HasDefaultConstructor() const
{
   // Return true if we have access to a default constructor.

   if (fNew) return kTRUE;

   if (!GetClassInfo()) return kFALSE;

   return gCint->ClassInfo_HasDefaultConstructor(GetClassInfo());
}

//______________________________________________________________________________
ROOT::NewFunc_t TClass::GetNew() const
{
   // Return the wrapper around new ThisClass().

   return fNew;
}

//______________________________________________________________________________
ROOT::NewArrFunc_t TClass::GetNewArray() const
{
   // Return the wrapper around new ThisClass[].

   return fNewArray;
}

//______________________________________________________________________________
ROOT::DelFunc_t TClass::GetDelete() const
{
   // Return the wrapper around delete ThiObject.

   return fDelete;
}

//______________________________________________________________________________
ROOT::DelArrFunc_t TClass::GetDeleteArray() const
{
   // Return the wrapper around delete [] ThiObject.

   return fDeleteArray;
}

//______________________________________________________________________________
ROOT::DesFunc_t TClass::GetDestructor() const
{
   // Return the wrapper around the destructor

   return fDestructor;
}

//______________________________________________________________________________
ROOT::DirAutoAdd_t TClass::GetDirectoryAutoAdd() const
{
   // Return the wrapper around the directory auto add function.

   return fDirAutoAdd;
}


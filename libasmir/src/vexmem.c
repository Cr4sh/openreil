//======================================================================
//
// This file contains the functions for memory management needed by 
// the binary to VEX IR translation interface. A large part of this
// file was copied from irdefs.c in VEX.
//
//======================================================================

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vexmem.h"


#include "config.h"
#if VEX_VERSION >= 1793
#define Ist_MFence Ist_MBE
#endif



static IRCallee* vx_mkIRCallee ( Int regparms, HChar* name, void* addr );
static IRRegArray* vx_mkIRRegArray ( Int base, IRType elemTy, Int nElems );
static IRExpr* vx_IRExpr_Get ( Int off, IRType ty ); 
static IRExpr* vx_IRExpr_GetI ( IRRegArray* descr, IRExpr* ix, Int bias ); 
static IRExpr* vx_IRExpr_Tmp ( IRTemp tmp ); 
static IRExpr* vx_IRExpr_Qop ( IROp op, IRExpr* arg1, IRExpr* arg2, 
			       IRExpr* arg3, IRExpr* arg4 );
static IRExpr* vx_IRExpr_Triop  ( IROp op, IRExpr* arg1, 
				  IRExpr* arg2, IRExpr* arg3 );
static IRExpr* vx_IRExpr_Binop ( IROp op, IRExpr* arg1, IRExpr* arg2 );
static IRExpr* vx_IRExpr_Unop ( IROp op, IRExpr* arg ); 
static IRExpr* vx_IRExpr_Load ( IREndness end, IRType ty, IRExpr* addr ); 
static IRExpr* vx_IRExpr_Const ( IRConst* con ); 
static IRExpr* vx_IRExpr_CCall ( IRCallee* cee, IRType retty, IRExpr** args ); 
static IRExpr* vx_IRExpr_Mux0X ( IRExpr* cond, IRExpr* expr0, IRExpr* exprX ); 
static IRDirty* vx_emptyIRDirty ( void ); 
static IRStmt* vx_IRStmt_NoOp ( void );
static IRStmt* vx_IRStmt_IMark ( Addr64 addr, Int len ); 
static IRStmt* vx_IRStmt_AbiHint ( IRExpr* base, Int len ); 
static IRStmt* vx_IRStmt_Put ( Int off, IRExpr* data ); 
static IRStmt* vx_IRStmt_PutI ( IRRegArray* descr, IRExpr* ix,
				Int bias, IRExpr* data );
static IRStmt* vx_IRStmt_Tmp ( IRTemp tmp, IRExpr* data ); 
static IRStmt* vx_IRStmt_Store ( IREndness end, IRExpr* addr, IRExpr* data ); 
static IRStmt* vx_IRStmt_Dirty ( IRDirty* d );
static IRStmt* vx_IRStmt_MFence ( void );
static IRStmt* vx_IRStmt_Exit ( IRExpr* guard, IRJumpKind jk, IRConst* dst ); 
static IRSB* vx_emptyIRSB ( void );
static IRExpr** vx_sopyIRExprVec ( IRExpr** vec );
static IRExpr** vx_dopyIRExprVec ( IRExpr** vec );
static IRConst* vx_dopyIRConst ( IRConst* c );
static IRCallee* vx_dopyIRCallee ( IRCallee* ce );
static IRRegArray* vx_dopyIRRegArray ( IRRegArray* d );
static IRExpr* vx_dopyIRExpr ( IRExpr* e );
static IRDirty* vx_dopyIRDirty ( IRDirty* d );
static IRStmt* vx_dopyIRStmt ( IRStmt* s );
static IRTypeEnv* vx_dopyIRTypeEnv ( IRTypeEnv* src );







//
// Function for panicking
//
void vx_panic ( HChar* str )
{
    printf("\nlibasmir-vex: the `impossible' happened:\n   %s\n", str);
    exit(-1);
}

//======================================================================
//
// Mem management functions
//
//======================================================================

//
// Currently 256 MB
//
#define HUGE_BLOCK_SIZE (1 << 28)

//
// Note:
//
// Because the memory allocated (via LibVEX_Alloc) in lib VEX is only
// live during translation and not usable afterwards, we have to write
// our own deep copy constructor to allocate memory from elsewhere and
// copy the translated IRSB from VEX memory to our memory before we can
// use it. 
//
// To avoid having to write matching destructors for every constructor
// (and there are a lot of them), we use a huge block of static memory
// for all allocations, and then free the entire block all at once when
// we're done with the IRSB. (Arena allocation style)
//
// TODO: Allocate this from the heap so that we can grow it at run time
//       if needed.
unsigned char huge_block[ HUGE_BLOCK_SIZE ];
unsigned char *next_free = huge_block;

void *vx_Alloc( Int nbytes )
{
    assert(nbytes > 0);

    void *this_block = next_free;

    next_free += nbytes;

    assert( next_free < huge_block + HUGE_BLOCK_SIZE );

    return this_block;
}

void vx_FreeAll()
{
  next_free = huge_block;    
}


//======================================================================
//
// Constructors
//
//======================================================================


/* Constructors -- IRCallee */

IRCallee* vx_mkIRCallee ( Int regparms, HChar* name, void* addr )
{
   IRCallee* ce = (IRCallee *)vx_Alloc(sizeof(IRCallee));
   ce->regparms = regparms;
   ce->name     = name;
   ce->addr     = addr;
   ce->mcx_mask = 0;
   assert(regparms >= 0 && regparms <= 3);
   assert(name != NULL);
   assert(addr != 0);
   return ce;
}


/* Constructors -- IRRegArray */

IRRegArray* vx_mkIRRegArray ( Int base, IRType elemTy, Int nElems )
{
   IRRegArray* arr = (IRRegArray *)vx_Alloc(sizeof(IRRegArray));
   arr->base    = base;
   arr->elemTy  = elemTy;
   arr->nElems  = nElems;
   assert(!(arr->base < 0 || arr->base > 10000 /* somewhat arbitrary */));
   assert(!(arr->elemTy == Ity_I1));
   assert(!(arr->nElems <= 0 || arr->nElems > 500 /* somewhat arbitrary */));
   return arr;
}


/* Constructors -- IRExpr */

IRExpr* vx_IRExpr_Get ( Int off, IRType ty ) {
   IRExpr* e         = (IRExpr *)vx_Alloc(sizeof(IRExpr));
   e->tag            = Iex_Get;
   e->Iex.Get.offset = off;
   e->Iex.Get.ty     = ty;
   return e;
}
IRExpr* vx_IRExpr_GetI ( IRRegArray* descr, IRExpr* ix, Int bias ) {
   IRExpr* e         = (IRExpr *)vx_Alloc(sizeof(IRExpr));
   e->tag            = Iex_GetI;
   e->Iex.GetI.descr = descr;
   e->Iex.GetI.ix    = ix;
   e->Iex.GetI.bias  = bias;
   return e;
}
IRExpr* vx_IRExpr_Tmp ( IRTemp tmp ) {
   IRExpr* e      = (IRExpr *)vx_Alloc(sizeof(IRExpr));
   e->tag         = Iex_RdTmp;
   e->Iex.RdTmp.tmp = tmp;
   return e;
}
IRExpr* vx_IRExpr_Qop ( IROp op, IRExpr* arg1, IRExpr* arg2, 
                              IRExpr* arg3, IRExpr* arg4 ) {
   IRExpr* e       = (IRExpr *)vx_Alloc(sizeof(IRExpr));
   e->tag          = Iex_Qop;
   e->Iex.Qop.op   = op;
   e->Iex.Qop.arg1 = arg1;
   e->Iex.Qop.arg2 = arg2;
   e->Iex.Qop.arg3 = arg3;
   e->Iex.Qop.arg4 = arg4;
   return e;
}
IRExpr* vx_IRExpr_Triop  ( IROp op, IRExpr* arg1, 
                                 IRExpr* arg2, IRExpr* arg3 ) {
   IRExpr* e         = (IRExpr *)vx_Alloc(sizeof(IRExpr));
   e->tag            = Iex_Triop;
   e->Iex.Triop.op   = op;
   e->Iex.Triop.arg1 = arg1;
   e->Iex.Triop.arg2 = arg2;
   e->Iex.Triop.arg3 = arg3;
   return e;
}
IRExpr* vx_IRExpr_Binop ( IROp op, IRExpr* arg1, IRExpr* arg2 ) {
   IRExpr* e         = (IRExpr *)vx_Alloc(sizeof(IRExpr));
   e->tag            = Iex_Binop;
   e->Iex.Binop.op   = op;
   e->Iex.Binop.arg1 = arg1;
   e->Iex.Binop.arg2 = arg2;
   return e;
}
IRExpr* vx_IRExpr_Unop ( IROp op, IRExpr* arg ) {
   IRExpr* e       = (IRExpr *)vx_Alloc(sizeof(IRExpr));
   e->tag          = Iex_Unop;
   e->Iex.Unop.op  = op;
   e->Iex.Unop.arg = arg;
   return e;
}
IRExpr* vx_IRExpr_Load ( IREndness end, IRType ty, IRExpr* addr ) {
   IRExpr* e        = (IRExpr *)vx_Alloc(sizeof(IRExpr));
   e->tag           = Iex_Load;
   e->Iex.Load.end  = end;
   e->Iex.Load.ty   = ty;
   e->Iex.Load.addr = addr;
   assert(end == Iend_LE || end == Iend_BE);
   return e;
}
IRExpr* vx_IRExpr_Const ( IRConst* con ) {
   IRExpr* e        = (IRExpr *)vx_Alloc(sizeof(IRExpr));
   e->tag           = Iex_Const;
   e->Iex.Const.con = con;
   return e;
}
IRExpr* vx_IRExpr_CCall ( IRCallee* cee, IRType retty, IRExpr** args ) {
   IRExpr* e          = (IRExpr *)vx_Alloc(sizeof(IRExpr));
   e->tag             = Iex_CCall;
   e->Iex.CCall.cee   = cee;
   e->Iex.CCall.retty = retty;
   e->Iex.CCall.args  = args;
   return e;
}
IRExpr* vx_IRExpr_Mux0X ( IRExpr* cond, IRExpr* expr0, IRExpr* exprX ) {
   IRExpr* e          = (IRExpr *)vx_Alloc(sizeof(IRExpr));
   e->tag             = Iex_Mux0X;
   e->Iex.Mux0X.cond  = cond;
   e->Iex.Mux0X.expr0 = expr0;
   e->Iex.Mux0X.exprX = exprX;
   return e;
}


/* Constructors -- IRDirty */

IRDirty* vx_emptyIRDirty ( void ) {
   IRDirty* d = (IRDirty *)vx_Alloc(sizeof(IRDirty));
   d->cee      = NULL;
   d->guard    = NULL;
   d->args     = NULL;
   d->tmp      = IRTemp_INVALID;
   d->mFx      = Ifx_None;
   d->mAddr    = NULL;
   d->mSize    = 0;
   d->needsBBP = False;
   d->nFxState = 0;
   return d;
}


/* Constructors -- IRStmt */

IRStmt* vx_IRStmt_NoOp ( void )
{
   /* Just use a single static closure. */
   static IRStmt static_closure;
   static_closure.tag = Ist_NoOp;
   return &static_closure;
}
IRStmt* vx_IRStmt_IMark ( Addr64 addr, Int len ) {
   IRStmt* s         = (IRStmt *)vx_Alloc(sizeof(IRStmt));
   s->tag            = Ist_IMark;
   s->Ist.IMark.addr = addr;
   s->Ist.IMark.len  = len;
   return s;
}
IRStmt* vx_IRStmt_AbiHint ( IRExpr* base, Int len ) {
   IRStmt* s           = (IRStmt *)vx_Alloc(sizeof(IRStmt));
   s->tag              = Ist_AbiHint;
   s->Ist.AbiHint.base = base;
   s->Ist.AbiHint.len  = len;
   return s;
}
IRStmt* vx_IRStmt_Put ( Int off, IRExpr* data ) {
   IRStmt* s         = (IRStmt *)vx_Alloc(sizeof(IRStmt));
   s->tag            = Ist_Put;
   s->Ist.Put.offset = off;
   s->Ist.Put.data   = data;
   return s;
}
IRStmt* vx_IRStmt_PutI ( IRRegArray* descr, IRExpr* ix,
                      Int bias, IRExpr* data ) {
   IRStmt* s         = (IRStmt *)vx_Alloc(sizeof(IRStmt));
   s->tag            = Ist_PutI;
   s->Ist.PutI.descr = descr;
   s->Ist.PutI.ix    = ix;
   s->Ist.PutI.bias  = bias;
   s->Ist.PutI.data  = data;
   return s;
}
IRStmt* vx_IRStmt_Tmp ( IRTemp tmp, IRExpr* data ) {
   IRStmt* s       = (IRStmt *)vx_Alloc(sizeof(IRStmt));
   s->tag          = Ist_WrTmp;
   s->Ist.WrTmp.tmp  = tmp;
   s->Ist.WrTmp.data = data;
   return s;
}
IRStmt* vx_IRStmt_Store ( IREndness end, IRExpr* addr, IRExpr* data ) {
   IRStmt* s         = (IRStmt *)vx_Alloc(sizeof(IRStmt));
   s->tag            = Ist_Store;
   s->Ist.Store.end  = end;
   s->Ist.Store.addr = addr;
   s->Ist.Store.data = data;
   assert(end == Iend_LE || end == Iend_BE);
   return s;
}
IRStmt* vx_IRStmt_Dirty ( IRDirty* d )
{
   IRStmt* s            = (IRStmt *)vx_Alloc(sizeof(IRStmt));
   s->tag               = Ist_Dirty;
   s->Ist.Dirty.details = d;
   return s;
}
IRStmt* vx_IRStmt_MFence ( void )
{
   /* Just use a single static closure. */
   static IRStmt static_closure;
   static_closure.tag = Ist_MFence;
   return &static_closure;
}
IRStmt* vx_IRStmt_Exit ( IRExpr* guard, IRJumpKind jk, IRConst* dst ) {
   IRStmt* s         = (IRStmt *)vx_Alloc(sizeof(IRStmt));
   s->tag            = Ist_Exit;
   s->Ist.Exit.guard = guard;
   s->Ist.Exit.jk    = jk;
   s->Ist.Exit.dst   = dst;
   return s;
}
IRStmt* vx_IRStmt_CAS (IRCAS* d) {
   IRStmt* s       = (IRStmt *)vx_Alloc(sizeof(IRStmt));
   s->tag          = Ist_CAS;
   s->Ist.CAS.details  = d;
   return s;
}
IRStmt* vx_IRStmt_LLSC ( IREndness end,
                      IRTemp result, IRExpr* addr, IRExpr* storedata ) {
    IRStmt* s = (IRStmt *)vx_Alloc(sizeof(IRStmt));
   s->tag                = Ist_LLSC;
   s->Ist.LLSC.end       = end;
   s->Ist.LLSC.result    = result;
   s->Ist.LLSC.addr      = addr;
   s->Ist.LLSC.storedata = storedata;
   return s;
}

/* Constructors -- IRSB */

IRSB* vx_emptyIRSB ( void )
{
   IRSB* bb       = (IRSB *)vx_Alloc(sizeof(IRSB));
   bb->tyenv      = NULL;
   bb->stmts_used = 0;
   bb->stmts_size = 0;
   bb->stmts      = NULL;
   bb->next       = NULL;
   bb->jumpkind   = Ijk_Boring;
   return bb;
}


/*---------------------------------------------------------------*/
/*--- (Deep) copy constructors.  These make complete copies   ---*/
/*--- the original, which can be modified without affecting   ---*/
/*--- the original.                                           ---*/
/*---------------------------------------------------------------*/

/* Copying IR Expr vectors (for call args). */

/* Shallow copy of an IRExpr vector */

IRExpr** vx_sopyIRExprVec ( IRExpr** vec )
{
   Int      i;
   IRExpr** newvec;
   for (i = 0; vec[i]; i++)
      ;
   newvec = (IRExpr **)vx_Alloc((i+1)*sizeof(IRExpr*));
   for (i = 0; vec[i]; i++)
      newvec[i] = vec[i];
   newvec[i] = NULL;
   return newvec;
}

/* Deep copy of an IRExpr vector */

IRExpr** vx_dopyIRExprVec ( IRExpr** vec )
{
   Int      i;
   IRExpr** newvec = vx_sopyIRExprVec( vec );
   for (i = 0; newvec[i]; i++)
      newvec[i] = vx_dopyIRExpr(newvec[i]);
   return newvec;
}

/* Deep copy constructors for all heap-allocated IR types follow. */

IRConst* vx_dopyIRConst ( IRConst* c )
{
  IRConst* c2;
  if ((c2 = vx_Alloc(sizeof(IRConst))))
    memcpy(c2, c, sizeof(IRConst));
  return c2;
}

IRCallee* vx_dopyIRCallee ( IRCallee* ce )
{
   IRCallee* ce2 = vx_mkIRCallee(ce->regparms, ce->name, ce->addr);
   ce2->mcx_mask = ce->mcx_mask;
   return ce2;
}

IRRegArray* vx_dopyIRRegArray ( IRRegArray* d )
{
   return vx_mkIRRegArray(d->base, d->elemTy, d->nElems);
}

IRExpr* vx_dopyIRExpr ( IRExpr* e )
{
   switch (e->tag) {
      case Iex_Get: 
         return vx_IRExpr_Get(e->Iex.Get.offset, e->Iex.Get.ty);
      case Iex_GetI: 
         return vx_IRExpr_GetI(vx_dopyIRRegArray(e->Iex.GetI.descr), 
                            vx_dopyIRExpr(e->Iex.GetI.ix),
                            e->Iex.GetI.bias);
      case Iex_RdTmp: 
        return vx_IRExpr_Tmp(e->Iex.RdTmp.tmp);
      case Iex_Qop: 
         return vx_IRExpr_Qop(e->Iex.Qop.op,
                           vx_dopyIRExpr(e->Iex.Qop.arg1),
                           vx_dopyIRExpr(e->Iex.Qop.arg2),
                           vx_dopyIRExpr(e->Iex.Qop.arg3),
                           vx_dopyIRExpr(e->Iex.Qop.arg4));
      case Iex_Triop: 
         return vx_IRExpr_Triop(e->Iex.Triop.op,
                             vx_dopyIRExpr(e->Iex.Triop.arg1),
                             vx_dopyIRExpr(e->Iex.Triop.arg2),
                             vx_dopyIRExpr(e->Iex.Triop.arg3));
      case Iex_Binop: 
         return vx_IRExpr_Binop(e->Iex.Binop.op,
                             vx_dopyIRExpr(e->Iex.Binop.arg1),
                             vx_dopyIRExpr(e->Iex.Binop.arg2));
      case Iex_Unop: 
         return vx_IRExpr_Unop(e->Iex.Unop.op,
                            vx_dopyIRExpr(e->Iex.Unop.arg));
      case Iex_Load: 
         return vx_IRExpr_Load(e->Iex.Load.end,
                            e->Iex.Load.ty,
                            vx_dopyIRExpr(e->Iex.Load.addr));
      case Iex_Const: 
         return vx_IRExpr_Const(vx_dopyIRConst(e->Iex.Const.con));
      case Iex_CCall:
         return vx_IRExpr_CCall(vx_dopyIRCallee(e->Iex.CCall.cee),
                             e->Iex.CCall.retty,
                             vx_dopyIRExprVec(e->Iex.CCall.args));

      case Iex_Mux0X: 
         return vx_IRExpr_Mux0X(vx_dopyIRExpr(e->Iex.Mux0X.cond),
                             vx_dopyIRExpr(e->Iex.Mux0X.expr0),
                                vx_dopyIRExpr(e->Iex.Mux0X.exprX));
   case Iex_Binder:
         vx_panic("vx_dopyIRExpr: case Iex_Binder (this should not be seen outside VEX)");
      default:
         vx_panic("vx_dopyIRExpr");
   }

   return NULL;
}

IRDirty* vx_dopyIRDirty ( IRDirty* d )
{
   Int      i;
   IRDirty* d2 = vx_emptyIRDirty();
   d2->cee   = vx_dopyIRCallee(d->cee);
   d2->guard = vx_dopyIRExpr(d->guard);
   d2->args  = vx_dopyIRExprVec(d->args);
   d2->tmp   = d->tmp;
   d2->mFx   = d->mFx;
   d2->mAddr = d->mAddr==NULL ? NULL : vx_dopyIRExpr(d->mAddr);
   d2->mSize = d->mSize;
   d2->needsBBP = d->needsBBP;
   d2->nFxState = d->nFxState;
   for (i = 0; i < d2->nFxState; i++)
      d2->fxState[i] = d->fxState[i];
   return d2;
}

IRCAS* vx_mkIRCAS ( IRTemp oldHi, IRTemp oldLo,
                 IREndness end, IRExpr* addr, 
                 IRExpr* expdHi, IRExpr* expdLo,
                 IRExpr* dataHi, IRExpr* dataLo ) {
    IRCAS* cas = (IRCAS *)vx_Alloc(sizeof(IRCAS));
   cas->oldHi  = oldHi;
   cas->oldLo  = oldLo;
   cas->end    = end;
   cas->addr   = addr;
   cas->expdHi = expdHi;
   cas->expdLo = expdLo;
   cas->dataHi = dataHi;
   cas->dataLo = dataLo;
   return cas;
}


IRCAS* vx_dopyIRCAS ( IRCAS* cas )
{
   return vx_mkIRCAS( cas->oldHi, cas->oldLo, cas->end,
                   vx_dopyIRExpr(cas->addr),
                   cas->expdHi==NULL ? NULL : vx_dopyIRExpr(cas->expdHi),
                   vx_dopyIRExpr(cas->expdLo),
                   cas->dataHi==NULL ? NULL : vx_dopyIRExpr(cas->dataHi),
                   vx_dopyIRExpr(cas->dataLo) );
}

IRStmt* vx_dopyIRStmt ( IRStmt* s )
{
   switch (s->tag) {
      case Ist_NoOp:
         return vx_IRStmt_NoOp();
      case Ist_AbiHint:
         return vx_IRStmt_AbiHint(vx_dopyIRExpr(s->Ist.AbiHint.base),
                               s->Ist.AbiHint.len);
      case Ist_IMark:
         return vx_IRStmt_IMark(s->Ist.IMark.addr, s->Ist.IMark.len);
      case Ist_Put: 
         return vx_IRStmt_Put(s->Ist.Put.offset, 
                           vx_dopyIRExpr(s->Ist.Put.data));
      case Ist_PutI: 
         return vx_IRStmt_PutI(vx_dopyIRRegArray(s->Ist.PutI.descr),
                            vx_dopyIRExpr(s->Ist.PutI.ix),
                            s->Ist.PutI.bias, 
                            vx_dopyIRExpr(s->Ist.PutI.data));
      case Ist_WrTmp:
         return vx_IRStmt_Tmp(s->Ist.WrTmp.tmp,
                           vx_dopyIRExpr(s->Ist.WrTmp.data));
      case Ist_Store: 
         return vx_IRStmt_Store(s->Ist.Store.end,
                             vx_dopyIRExpr(s->Ist.Store.addr),
                             vx_dopyIRExpr(s->Ist.Store.data));
      case Ist_Dirty: 
         return vx_IRStmt_Dirty(vx_dopyIRDirty(s->Ist.Dirty.details));
      case Ist_MFence:
         return vx_IRStmt_MFence();
      case Ist_Exit: 
         return vx_IRStmt_Exit(vx_dopyIRExpr(s->Ist.Exit.guard),
                            s->Ist.Exit.jk,
                               vx_dopyIRConst(s->Ist.Exit.dst));
   case Ist_CAS:
       return vx_IRStmt_CAS(vx_dopyIRCAS(s->Ist.CAS.details));
   case Ist_LLSC:
                return vx_IRStmt_LLSC(s->Ist.LLSC.end,
                            s->Ist.LLSC.result,
                            vx_dopyIRExpr(s->Ist.LLSC.addr),
                            s->Ist.LLSC.storedata
                                      ? vx_dopyIRExpr(s->Ist.LLSC.storedata)
                               : NULL);
       
      default: 
         vx_panic("vx_dopyIRStmt");
   }

   return NULL;
}

IRTypeEnv* vx_dopyIRTypeEnv ( IRTypeEnv* src )
{
   Int        i;
   IRTypeEnv* dst = (IRTypeEnv *)vx_Alloc(sizeof(IRTypeEnv));
   dst->types_size = src->types_size;
   dst->types_used = src->types_used;
   dst->types = (IRType *)vx_Alloc(dst->types_size * sizeof(IRType));
   for (i = 0; i < src->types_used; i++)
      dst->types[i] = src->types[i];
   return dst;
}

IRSB* vx_dopyIRSB ( IRSB* bb )
{
   Int      i;
   IRStmt** sts2;
   IRSB* bb2 = vx_emptyIRSB();
   bb2->tyenv = vx_dopyIRTypeEnv(bb->tyenv);
   bb2->stmts_used = bb2->stmts_size = bb->stmts_used;
   sts2 = (IRStmt **)vx_Alloc(bb2->stmts_used * sizeof(IRStmt*));
   for (i = 0; i < bb2->stmts_used; i++)
      sts2[i] = vx_dopyIRStmt(bb->stmts[i]);
   bb2->stmts    = sts2;
   bb2->next     = vx_dopyIRExpr(bb->next);
   bb2->jumpkind = bb->jumpkind;
   return bb2;
}



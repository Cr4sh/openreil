/*
 Owned and copyright BitBlaze, 2007. All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.
*/
#ifndef _IRVISITOR_H
#define _IRVISITOR_H

class VarDecl;
class Temp;
class Mem;
class UnOp;
class BinOp;
class Constant;
class Phi;
class Special;
class Jmp;
class CJmp;
class Label;
class Move;
class Comment;
class ExpStmt;
class Unknown;
class Cast;
class Name;
class Exp;
class Let;
class Call;
class Return;
class Func;
class Assert;

class IRVisitor {
 public:
  IRVisitor() {} ;
  virtual void visitBinOp(BinOp *) = 0;
  virtual void visitUnOp(UnOp *) = 0;
  virtual void visitConstant(Constant *) = 0;
  virtual void visitTemp(Temp *) = 0;
  virtual void visitPhi(Phi *) = 0;
  virtual void visitSpecial(Special *) = 0;
  virtual void visitMem(Mem *) = 0;
  virtual void visitUnknown(Unknown *) = 0;
  virtual void visitCast(Cast *) = 0;
  virtual void visitName(Name *) = 0;
  virtual void visitJmp(Jmp *) = 0;
  virtual void visitCJmp(CJmp *) = 0;
  virtual void visitLabel(Label *) = 0;
  virtual void visitMove(Move *) = 0;
  virtual void visitComment(Comment *) = 0;
  virtual void visitExpStmt(ExpStmt *) = 0;
  virtual void visitVarDecl(VarDecl *) = 0;
  virtual void visitLet(Let *) = 0;
  virtual void visitCall(Call *) = 0;
  virtual void visitReturn(Return *) = 0;
  virtual void visitFunc(Func *) = 0;
  virtual void visitAssert(Assert *) = 0;
  virtual ~IRVisitor() {};
};

class DefaultIRVisitor : public virtual IRVisitor {
 public:
  DefaultIRVisitor() {};
  virtual void visitBinOp(BinOp *);
  virtual void visitUnOp(UnOp *);
  virtual void visitConstant(Constant *);
  virtual void visitTemp(Temp *);
  virtual void visitPhi(Phi *);
  virtual void visitSpecial(Special *);
  virtual void visitMem(Mem *);
  virtual void visitUnknown(Unknown *);
  virtual void visitCast(Cast *);
  virtual void visitName(Name *);
  virtual void visitJmp(Jmp *);
  virtual void visitCJmp(CJmp *);
  virtual void visitLabel(Label *);
  virtual void visitMove(Move *);
  virtual void visitComment(Comment *);
  virtual void visitExpStmt(ExpStmt *);
  virtual void visitVarDecl(VarDecl *);
  virtual void visitLet(Let *);
  virtual void visitCall(Call *);
  virtual void visitReturn(Return *);
  virtual void visitFunc(Func *);
  virtual void visitAssert(Assert *);
  virtual ~DefaultIRVisitor() { };
};

class IRChangeVisitor : public IRVisitor {
 public: 
  virtual ~IRChangeVisitor() { }; 
  IRChangeVisitor() {};

  virtual void visitBinOp(BinOp *);
  virtual void visitUnOp(UnOp *);
  virtual void visitConstant(Constant *);
  virtual void visitTemp(Temp *);
  virtual void visitPhi(Phi *);
  virtual void visitSpecial(Special *);
  virtual void visitMem(Mem *);
  virtual void visitUnknown(Unknown *);
  virtual void visitCast(Cast *);
  virtual void visitName(Name *);
  virtual void visitJmp(Jmp *);
  virtual void visitCJmp(CJmp *);
  virtual void visitLabel(Label *);
  virtual void visitMove(Move *);
  virtual void visitComment(Comment *);
  virtual void visitExpStmt(ExpStmt *);
  virtual void visitVarDecl(VarDecl *);
  virtual void visitLet(Let *);
  virtual void visitCall(Call *);
  virtual void visitReturn(Return *);
  virtual void visitFunc(Func *);
  virtual void visitAssert(Assert *);
 protected:
  Exp *ret;
};
#endif

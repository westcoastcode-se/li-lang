#include "SyntaxTreeNodeOpUnaryop.h"
#include "SyntaxTreeNodePackage.h"
#include "SyntaxTreeNodeFuncDef.h"

using namespace WestCoastCode;
using namespace WestCoastCode::Compilation;

SyntaxTreeNodeOpUnaryop::~SyntaxTreeNodeOpUnaryop()
{
	for (auto c : _children)
		delete c;
}

void SyntaxTreeNodeOpUnaryop::ToString(StringStream& s, int indent) const
{
	s << _id << Indent(indent);
	s << "Unaryop(op=" << ToString(_op) << ")" << std::endl;
	for (int i = 0; i < _children.Size(); ++i)
		_children[i]->ToString(s, indent + 1);
}

ISyntaxTree* SyntaxTreeNodeOpUnaryop::GetSyntaxTree() const
{
	return _parent->GetSyntaxTree();
}

ISyntaxTreeNode* SyntaxTreeNodeOpUnaryop::GetRootNode()
{
	if (_parent)
		return _parent->GetRootNode();
	return this;
}

void SyntaxTreeNodeOpUnaryop::SetParent(ISyntaxTreeNode* parent)
{
	_parent = parent;
}

void SyntaxTreeNodeOpUnaryop::Compile(Builder::Linker* linker, Builder::Instructions& instructions)
{
	GetRight()->Compile(linker, instructions);

	switch (_op)
	{
	case Op::Minus:
		//instructions.Add(GetRight());
		break;
	}
	throw CompileErrorNotImplemented(this, "Unaryop");
}

ISyntaxTreeNodeType* SyntaxTreeNodeOpUnaryop::GetStackType()
{
	return _children[0]->GetStackType();
}

Vector<ISyntaxTreeNodeOp*> SyntaxTreeNodeOpUnaryop::OptimizeOp(ISyntaxTreeNodeOptimizer* optimizer)
{
	for (int i = 0; i < _children.Size(); ++i) {
		auto optimized = _children[i]->OptimizeOp(optimizer);
		if (!optimized.IsEmpty()) {
			if (optimized.Size() != 1)
				throw CompileErrorNotImplemented(this, "Unaryop optimize is not allowed expand");
			delete _children[i];
			_children[i] = optimized[0];
		}
	}
	return optimizer->Optimize(this);
}

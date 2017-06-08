/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @author Alex Beregszaszi
 * @date 2017
 * Julia to WebAssembly code generator.
 */

#include <libjulia/backends/webassembly/WebAssembly.h>
#include <libsolidity/inlineasm/AsmData.h>
#include <libsolidity/interface/Utils.h>

#include <libdevcore/CommonIO.h>

#include <binaryen-c.h>
#include <wasm-builder.h>

#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/count_if.hpp>

#include <memory>
#include <functional>

using namespace std;
using namespace dev;
using namespace dev::solidity;
using namespace dev::solidity::assembly;

class Generator: public boost::static_visitor<>
{
public:
	/// Create the code transformer which appends assembly to _state.assembly when called
	/// with parsed assembly data.
	/// @param _identifierAccess used to resolve identifiers external to the inline assembly
	explicit Generator(assembly::Block const& _block)
	{
		std::for_each(_block.statements.begin(), _block.statements.end(), boost::apply_visitor(*this));
	}

	string assembly() { return m_assembly; }

public:
	void operator()(assembly::Instruction const&)
	{
		solAssert(false, "Instructions are not supported in Julia.");
	}
	void operator()(assembly::FunctionalInstruction const&)
	{
		solAssert(false, "Instructions are not supported in Julia.");
	}
	void operator()(assembly::StackAssignment const&)
	{
		solAssert(false, "Assignment from stack is not supported in Julia.");
	}
	void operator()(assembly::Label const&)
	{
		solAssert(false, "Labels are not supported in Julia.");
	}
	void operator()(assembly::Literal const& _literal)
	{
		if (_literal.kind == assembly::LiteralKind::Number)
			m_assembly += "(" + convertType(_literal.type) + ".const " + _literal.value + ")";
		else if (_literal.kind == assembly::LiteralKind::Boolean)
			m_assembly += string("(i64.const ") + ((_literal.value == "true") ? "1" : "0") + ")";
		else
			solAssert(false, "Non-number literals not supported.");
	}
	void operator()(assembly::Identifier const& _identifier)
	{
		m_assembly += "(get_local $" + _identifier.name + ")";
	}
	void operator()(assembly::VariableDeclaration const& _varDecl)
	{
		solAssert(_varDecl.variables.size() == 1, "Tuples not supported yet.");
		m_assembly += "(local $" + _varDecl.variables.front().name + " " + convertType(_varDecl.variables.front().type) + ")\n";
		m_assembly += "(set_local $" + _varDecl.variables.front().name + " ";
		boost::apply_visitor(*this, *_varDecl.value);
		m_assembly += ")\n";
	}
	void operator()(assembly::Assignment const& _assignment)
	{
		m_assembly += "(set_local $" + _assignment.variableName.name + " ";
		boost::apply_visitor(*this, *_assignment.value);
		m_assembly += ")\n";
	}
	void operator()(assembly::FunctionDefinition const& _funDef)
	{
		m_assembly += "(func $" + _funDef.name + " ";
		for (auto const& argument: _funDef.arguments)
			m_assembly += "(param $" + argument.name + " " + convertType(argument.type) + ")";
		solAssert(_funDef.returns.size() <= 1, "Multiple return values not supported yet.");
		string returnName;
		for (auto const& returnArgument: _funDef.returns)
		{
			returnName = returnArgument.name;
			m_assembly += "(result " + convertType(returnArgument.type) + ")";
			m_assembly += "(local $" + returnArgument.name + " " + convertType(returnArgument.type) + ")";
		}
		/// Scope rules: return parameters must be marked appropriately
		Generator generator = Generator(_funDef.body);
		m_assembly += generator.assembly();
//		boost::apply_visitor(*this, _funDef.body);
		if (!returnName.empty())
			m_assembly += "(return $" + returnName + ")";
		m_assembly += ")\n";
	}
	void operator()(assembly::FunctionCall const& _funCall)
	{
		if (resolveBuiltinFunction(_funCall))
			return;

		m_assembly += "\n(call $" + _funCall.functionName.name;
		for (auto const& _statement: _funCall.arguments)
		{
			m_assembly += " ";
			boost::apply_visitor(*this, _statement);
		}
		m_assembly += ")\n";
	}
	void operator()(assembly::Switch const& _switch)
	{
		solAssert(_switch.cases.size() <= 2, "");
//		solAssert(false, "Not implemented yet.");
		m_assembly += "(if (result i64) ";
		boost::apply_visitor(*this, *_switch.expression);
		m_assembly += "(then ";
		Generator generator1 = Generator(_switch.cases[0].body);
		m_assembly += generator1.assembly();
		m_assembly += ")";
		if (_switch.cases.size() == 2)
		{
			m_assembly += "(else ";
			Generator generator2 = Generator(_switch.cases[1].body);
			m_assembly += generator2.assembly();
			m_assembly += ")";
		}
		m_assembly += ")";
	}
	void operator()(assembly::Block const& _block)
	{
		Generator generator = Generator(_block);
		m_assembly += "(block " + generator.assembly() + ")";
	}
private:
	string convertType(assembly::Type type)
	{
		solAssert(!type.empty(), "Only Julia input is supported.");
		set<string> const supportedTypes{"bool", "u8", "s8", "u32", "s32", "u64", "s64"};
		solAssert(supportedTypes.count(type), "Type (" + type + ") not supported yet.");
		return "i64";
	}

	/// TODO: replace with a proper structure (and not manual code)
	bool resolveBuiltinFunction(assembly::FunctionCall const& _funCall)
	{
		if (_funCall.functionName.name == "add64")
		{
			m_assembly += "(i64.add ";
			solAssert(_funCall.arguments.size() == 2, "");
			boost::apply_visitor(*this, _funCall.arguments[0]);
			boost::apply_visitor(*this, _funCall.arguments[1]);
			m_assembly += ")";
			return true;
		}
		else if (_funCall.functionName.name == "sub64")
		{
			m_assembly += "(i64.sub ";
			solAssert(_funCall.arguments.size() == 2, "");
			boost::apply_visitor(*this, _funCall.arguments[0]);
			boost::apply_visitor(*this, _funCall.arguments[1]);
			m_assembly += ")";
			return true;
		}
		else if (_funCall.functionName.name == "mul64")
		{
			m_assembly += "(i64.mul ";
			solAssert(_funCall.arguments.size() == 2, "");
			boost::apply_visitor(*this, _funCall.arguments[0]);
			boost::apply_visitor(*this, _funCall.arguments[1]);
			m_assembly += ")";
			return true;
		}
		else if (_funCall.functionName.name == "gt64")
		{
			m_assembly += "(i64.gt_u ";
			solAssert(_funCall.arguments.size() == 2, "");
			boost::apply_visitor(*this, _funCall.arguments[0]);
			boost::apply_visitor(*this, _funCall.arguments[1]);
			m_assembly += ")";
			return true;
		}

		return false;
	}

	string m_assembly;
};

string julia::WebAssembly::assemble(assembly::Block const& _block)
{
#if 0
	BinaryenModuleRef module = BinaryenModuleCreate();

	// Create a function type for  i32 (i32, i32)
	BinaryenType params[2] = { BinaryenInt32(), BinaryenInt32() };
	BinaryenFunctionTypeRef iii = BinaryenAddFunctionType(module, "iii", BinaryenInt32(), params, 2);

	// Get the 0 and 1 arguments, and add them
	BinaryenExpressionRef x = BinaryenGetLocal(module, 0, BinaryenInt32()),
                        y = BinaryenGetLocal(module, 1, BinaryenInt32());
	BinaryenExpressionRef add = BinaryenBinary(module, BinaryenAddInt32(), x, y);

	// Create the add function
	// Note: no additional local variables
	// Note: no basic blocks here, we are an AST. The function body is just an expression node.
	BinaryenFunctionRef adder = BinaryenAddFunction(module, "adder", iii, NULL, 0, add);

	BinaryenSetFunctionTable(module, &adder, 1);

	// Print it out
	BinaryenModulePrint(module);

	// Clean up the module, which owns all the objects we created above
	BinaryenModuleDispose(module);
#endif

	Generator generator = Generator(_block);
	return generator.assembly();
}

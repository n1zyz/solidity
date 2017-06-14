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
	void operator()(assembly::Literal const&)
	{
		solAssert(false, "Not implemented yet.");
	}
	void operator()(assembly::Identifier const&)
	{
		solAssert(false, "Not implemented yet.");
	}
	void operator()(assembly::VariableDeclaration const&)
	{
		solAssert(false, "Not implemented yet.");
	}
	void operator()(assembly::Assignment const&)
	{
		solAssert(false, "Not implemented yet.");
	}
	void operator()(assembly::FunctionDefinition const&)
	{
		solAssert(false, "Not implemented yet.");
	}
	void operator()(assembly::FunctionCall const&)
	{
		solAssert(false, "Not implemented yet.");
	}
	void operator()(assembly::Switch const&)
	{
		solAssert(false, "Not implemented yet.");
	}
	void operator()(assembly::Block const&)
	{
		solAssert(false, "Not implemented yet.");
	}
private:
	string m_assembly;
};

string julia::WebAssembly::assemble(assembly::Block const& _block)
{
	Generator generator = Generator(_block);
	return generator.assembly();
}

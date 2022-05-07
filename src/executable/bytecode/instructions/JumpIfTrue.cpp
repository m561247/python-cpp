#include "JumpIfTrue.hpp"
#include "runtime/PyBool.hpp"
#include "runtime/PyNone.hpp"

using namespace py;

PyResult<Value> JumpIfTrue::execute(VirtualMachine &vm, Interpreter &interpreter) const
{
	ASSERT(m_offset.has_value())
	auto &result = vm.reg(m_test_register);

	const auto test_result = truthy(result, interpreter);
	if (test_result.is_ok()) {
		if (test_result.unwrap()) {
			const auto ip = vm.instruction_pointer() + *m_offset;
			vm.set_instruction_pointer(ip);
		}
		return Ok(Value{ NameConstant{ test_result.unwrap() } });
	}
	return Err(test_result.unwrap_err());
}

void JumpIfTrue::relocate(codegen::BytecodeGenerator &, size_t instruction_idx)
{
	m_offset = m_label->position() - instruction_idx - 1;
}

std::vector<uint8_t> JumpIfTrue::serialize() const
{
	ASSERT(m_offset.has_value())
	ASSERT(m_offset < std::numeric_limits<uint8_t>::max())

	return {
		JUMP_IF_TRUE,
		m_test_register,
		static_cast<uint8_t>(*m_offset),
	};
}
#include "JumpIfTrue.hpp"
#include "runtime/PyBool.hpp"

using namespace py;

PyResult JumpIfTrue::execute(VirtualMachine &vm, Interpreter &) const
{
	auto &result = vm.reg(m_test_register);

	const auto test_result =
		std::visit(overloaded{ [](PyObject *const &obj) -> PyResult { return obj->bool_(); },
					   [](const auto &) -> PyResult {
						   TODO();
						   return PyResult::Err(nullptr);
					   },
					   [](const NameConstant &value) -> PyResult {
						   if (auto *bool_type = std::get_if<bool>(&value.value)) {
							   return PyResult::Ok(*bool_type ? py_true() : py_false());
						   } else {
							   return PyResult::Ok(py_false());
						   }
					   } },
			result);
	if (test_result.is_ok() && test_result.unwrap_as<PyObject>() == py_true()) {
		const auto ip = vm.instruction_pointer() + m_label->position();
		vm.set_instruction_pointer(ip);
	}
	return test_result;
}

void JumpIfTrue::relocate(codegen::BytecodeGenerator &, size_t instruction_idx)
{
	m_label->set_position(m_label->position() - instruction_idx - 1);
	m_label->immutable();
}

std::vector<uint8_t> JumpIfTrue::serialize() const
{
	ASSERT(m_label->position() < std::numeric_limits<uint8_t>::max())
	if (m_offset.has_value()) { ASSERT(m_offset < std::numeric_limits<uint8_t>::max()) }

	return {
		JUMP_IF_TRUE,
		m_test_register,
		static_cast<uint8_t>(m_label->position()),
		m_offset ? uint8_t{ 0 } : static_cast<uint8_t>(*m_offset),
	};
}
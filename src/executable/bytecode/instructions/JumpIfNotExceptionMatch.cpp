#include "JumpIfNotExceptionMatch.hpp"
#include "interpreter/Interpreter.hpp"
#include "runtime/PyType.hpp"

using namespace py;

namespace {
bool has_stashed_exception(Interpreter &interpreter)
{
	return interpreter.execution_frame()->stashed_exception_info().has_value();
}
}// namespace

void JumpIfNotExceptionMatch::execute(VirtualMachine &vm, Interpreter &interpreter) const
{
	ASSERT(has_stashed_exception(interpreter))
	const auto &exception_type = vm.reg(m_exception_type_reg);
	ASSERT(std::holds_alternative<PyObject *>(exception_type))
	auto *exception_type_obj = std::get<PyObject *>(exception_type);
	ASSERT(as<PyType>(exception_type_obj))

	if (!interpreter.execution_frame()->stashed_exception_info()->exception->type()->issubclass(
			as<PyType>(exception_type_obj))) {
		// skip exception handler body block
		vm.jump_blocks(2);
	}
}
#include "GreaterThan.hpp"


void GreaterThan::execute(VirtualMachine &vm, Interpreter &interpreter) const
{
	const auto &lhs = vm.reg(m_lhs);
	const auto &rhs = vm.reg(m_rhs);

	if (auto result = greater_than(lhs, rhs, interpreter)) {
		ASSERT(vm.registers().has_value())
		ASSERT(vm.registers()->get().size() > m_dst)
		vm.reg(m_dst) = *result;
	}
};
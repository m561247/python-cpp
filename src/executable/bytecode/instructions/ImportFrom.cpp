#include "ImportFrom.hpp"

#include "interpreter/Interpreter.hpp"
#include "runtime/PyFrame.hpp"
#include "runtime/PyModule.hpp"
#include "vm/VM.hpp"

using namespace py;

PyResult<Value> ImportFrom::execute(VirtualMachine &vm, Interpreter &interpreter) const
{
	const std::string name = interpreter.execution_frame()->names(m_name);
	const auto &from = vm.reg(m_from);

	ASSERT(as<PyModule>(PyObject::from(from).unwrap()));

	auto obj_ = PyObject::from(from).unwrap()->get_attribute(PyString::create(name).unwrap());

	return obj_.and_then([&vm, this](PyObject *obj) {
		vm.reg(m_destination) = obj;
		return Ok(obj);
	});
}

std::vector<uint8_t> ImportFrom::serialize() const
{
	std::vector<uint8_t> result{
		IMPORT_FROM,
		m_destination,
		m_name,
		m_from,
	};

	return result;
}
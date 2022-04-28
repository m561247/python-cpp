#include "PyBool.hpp"
#include "PyString.hpp"
#include "types/api.hpp"
#include "types/builtin.hpp"
#include "vm/VM.hpp"

namespace py {

template<> PyBool *as(PyObject *node)
{
	if (node->type() == bool_()) { return static_cast<PyBool *>(node); }
	return nullptr;
}

template<> const PyBool *as(const PyObject *node)
{
	if (node->type() == bool_()) { return static_cast<const PyBool *>(node); }
	return nullptr;
}

PyBool::PyBool(bool name) : PyBaseObject(BuiltinTypes::the().bool_()), m_value(name) {}

std::string PyBool::to_string() const { return m_value ? "True" : "False"; }

PyResult PyBool::__repr__() const { return PyString::create(to_string()); }

PyResult PyBool::__add__(const PyObject *) const { TODO(); }

PyResult PyBool::__bool__() const { return PyResult::Ok(m_value ? py_true() : py_false()); }

PyResult PyBool::create(bool value)
{
	auto &heap = VirtualMachine::the().heap();
	auto *result = heap.allocate_static<PyBool>(value).get();
	ASSERT(result)
	return PyResult::Ok(result);
}

PyType *PyBool::type() const { return py::bool_(); }

PyObject *py_true()
{
	static PyObject *value = nullptr;

	if (!value) { value = PyBool::create(true).unwrap_as<PyBool>(); }

	return value;
}

PyObject *py_false()
{
	static PyObject *value = nullptr;

	if (!value) { value = PyBool::create(false).unwrap_as<PyBool>(); }

	return value;
}

namespace {

	std::once_flag bool_flag;

	std::unique_ptr<TypePrototype> register_bool() { return std::move(klass<PyBool>("bool").type); }
}// namespace

std::unique_ptr<TypePrototype> PyBool::register_type()
{
	static std::unique_ptr<TypePrototype> type = nullptr;
	std::call_once(bool_flag, []() { type = register_bool(); });
	return std::move(type);
}
}// namespace py

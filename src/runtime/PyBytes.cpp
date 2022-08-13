#include "PyBytes.hpp"
#include "MemoryError.hpp"
#include "PyBool.hpp"
#include "types/api.hpp"
#include "types/builtin.hpp"
#include "vm/VM.hpp"

namespace py {

template<> PyBytes *as(PyObject *obj)
{
	if (obj->type() == bytes()) { return static_cast<PyBytes *>(obj); }
	return nullptr;
}

template<> const PyBytes *as(const PyObject *obj)
{
	if (obj->type() == bytes()) { return static_cast<const PyBytes *>(obj); }
	return nullptr;
}

PyBytes::PyBytes(const Bytes &number) : PyBaseObject(BuiltinTypes::the().bytes()), m_value(number)
{}

PyBytes::PyBytes() : PyBytes(Bytes{}) {}

PyResult<PyBytes *> PyBytes::create(const Bytes &value)
{
	auto &heap = VirtualMachine::the().heap();
	auto *obj = heap.allocate<PyBytes>(value);
	if (!obj) { return Err(memory_error(sizeof(PyBytes))); }
	return Ok(obj);
}

PyResult<PyBytes *> PyBytes::create()
{
	auto &heap = VirtualMachine::the().heap();
	auto *obj = heap.allocate<PyBytes>();
	if (!obj) { return Err(memory_error(sizeof(PyBytes))); }
	return Ok(obj);
}

std::string PyBytes::to_string() const { return m_value.to_string(); }

PyResult<PyObject *> PyBytes::__add__(const PyObject *other) const
{
	if (!as<PyBytes>(other)) {
		return Err(type_error("can't concat {} to bytes", other->type()->name()));
	}
	auto bytes = as<PyBytes>(other);
	auto new_bytes = m_value;
	new_bytes.b.insert(new_bytes.b.end(), bytes->value().b.begin(), bytes->value().b.end());
	return PyBytes::create(new_bytes);
}

PyResult<size_t> PyBytes::__len__() const { return Ok(m_value.b.size()); }

PyResult<PyObject *> PyBytes::__eq__(const PyObject *obj) const
{
	if (this == obj) return Ok(py_true());
	if (auto obj_bytes = as<PyBytes>(obj)) {
		return Ok(m_value.b == obj_bytes->value().b ? py_true() : py_false());
	} else {
		return Err(type_error("'==' not supported between instances of '{}' and '{}'",
			type()->name(),
			obj->type()->name()));
	}
}

PyResult<PyObject *> PyBytes::__repr__() const { return PyString::create(to_string()); }

PyType *PyBytes::type() const { return bytes(); }

namespace {

	std::once_flag bytes_flag;

	std::unique_ptr<TypePrototype> register_bytes()
	{
		return std::move(klass<PyBytes>("bytes").type);
	}
}// namespace

std::function<std::unique_ptr<TypePrototype>()> PyBytes::type_factory()
{
	return [] {
		static std::unique_ptr<TypePrototype> type = nullptr;
		std::call_once(bytes_flag, []() { type = register_bytes(); });
		return std::move(type);
	};
}

}// namespace py
#pragma once

#include "builtin.hpp"
#include "runtime/PyDict.hpp"
#include "runtime/PyModule.hpp"
#include "runtime/PyTuple.hpp"
#include "runtime/PyType.hpp"

namespace py {
template<typename T> struct klass
{
	std::unique_ptr<TypePrototype> type;
	PyModule *m_module;

	klass(PyModule *module, std::string_view name)
		: type(TypePrototype::create<T>(name)), m_module(module)
	{}

	template<typename... BaseType>
	requires(std::is_same_v<std::remove_reference_t<BaseType>, PyType *> &&...)
		klass(PyModule *module, std::string_view name, BaseType &&... bases)
		: type(TypePrototype::create<T>(name)), m_module(module)
	{
		type->__bases__ = PyTuple::create(bases...);
	}

	klass(std::string_view name) : type(TypePrototype::create<T>(name)) {}

	template<typename FuncType>
	klass &def(std::string_view name, FuncType &&F) requires requires(PyObject *self)
	{
		(static_cast<T *>(self)->*F)();
	}
	{
		type->add_method(MethodDefinition{
			std::string(name), [F](PyObject *self, PyTuple *args, PyDict *kwargs) {
				// TODO: this should raise an exception
				//       TypeError: {}() takes no arguments ({} given)
				//       TypeError: {}() takes no keyword arguments
				ASSERT(!args || args->size() == 0)
				ASSERT(!kwargs || kwargs->map().empty())
				return (static_cast<T *>(self)->*F)();
			} });
		return *this;
	}

	template<typename FuncType>
	klass &def(std::string_view name,
		FuncType &&F) requires requires(PyObject *self, PyTuple *args, PyDict *kwargs)
	{
		(static_cast<T *>(self)->*F)(args, kwargs);
	}
	{
		type->add_method(MethodDefinition{
			std::string(name), [F](PyObject *self, PyTuple *args, PyDict *kwargs) {
				return (static_cast<T *>(self)->*F)(args, kwargs);
			} });
		return *this;
	}

	template<typename FuncType>
	klass &def(std::string_view name,
		FuncType &&F) requires requires(PyObject *self, PyTuple *args, PyDict *kwargs)
	{
		F(static_cast<T *>(self), args, kwargs);
	}
	{
		type->add_method(MethodDefinition{
			std::string(name), [F](PyObject *self, PyTuple *args, PyDict *kwargs) {
				return F(static_cast<T *>(self), args, kwargs);
			} });
		return *this;
	}

	PyType *finalize()
	{
		auto *type_ = PyType::initialize(*type.release());
		m_module->insert(PyString::create(type_->name()), type_);
		return type_;
	}
};

}// namespace py
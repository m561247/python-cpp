#pragma once

#include "Exception.hpp"
#include "PyString.hpp"
#include "PyTuple.hpp"
#include "vm/VM.hpp"

namespace py {

class RuntimeError : public Exception
{
	friend class ::Heap;
	template<typename... Args>
	friend BaseException *runtime_error(const std::string &message, Args &&... args);

  private:
	RuntimeError(PyTuple *args);

	static PyResult create(PyTuple *args);

  public:
	static PyType *register_type(PyModule *);

	PyType *type() const override;
};

template<typename... Args>
inline BaseException *runtime_error(const std::string &message, Args &&... args)
{
	auto msg = PyString::create(fmt::format(message, std::forward<Args>(args)...));
	ASSERT(msg.is_ok())
	auto args_tuple = PyTuple::create(msg.template unwrap_as<PyString>());
	ASSERT(args_tuple.is_ok())
	auto obj = RuntimeError::create(args_tuple.template unwrap_as<PyTuple>());
	ASSERT(obj.is_ok())
	return obj.template unwrap_as<RuntimeError>();
}

}// namespace py
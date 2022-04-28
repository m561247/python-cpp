#include "PyLLVMFunction.hpp"
#include "PyString.hpp"
#include "types/api.hpp"
#include "types/builtin.hpp"

namespace py {

PyLLVMFunction::PyLLVMFunction(std::string &&name, FunctionType &&function)
	: PyBaseObject(BuiltinTypes::the().llvm_function()), m_name(std::move(name)),
	  m_function(std::move(function))
{}

std::string PyLLVMFunction::to_string() const
{
	return fmt::format("LLVM JIT function {} at {}", m_name, (void *)this);
}

PyResult PyLLVMFunction::__call__(PyTuple *args, PyDict *kwargs) { return (*this)(args, kwargs); }

PyResult PyLLVMFunction::__repr__() const { return PyString::create(to_string()); }

void PyLLVMFunction::visit_graph(Visitor &visitor)
{
	PyObject::visit_graph(visitor);
	for (auto *obj : m_captures) { obj->visit_graph(visitor); }
}

PyType *PyLLVMFunction::type() const { return llvm_function(); }

namespace {
	std::once_flag llvm_function_flag;

	std::unique_ptr<TypePrototype> register_llvm_function()
	{
		return std::move(klass<PyLLVMFunction>("llvm_function_or_method").type);
	}
}// namespace

std::unique_ptr<TypePrototype> PyLLVMFunction::register_type()
{
	static std::unique_ptr<TypePrototype> type = nullptr;
	std::call_once(llvm_function_flag, []() { type = register_llvm_function(); });
	return std::move(type);
}

template<> PyLLVMFunction *as(PyObject *node)
{
	if (node->type() == llvm_function()) { return static_cast<PyLLVMFunction *>(node); }
	return nullptr;
}

template<> const PyLLVMFunction *as(const PyObject *node)
{
	if (node->type() == llvm_function()) { return static_cast<const PyLLVMFunction *>(node); }
	return nullptr;
}

}// namespace py
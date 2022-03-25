#include "ExecutionFrame.hpp"
#include "runtime/PyCell.hpp"
#include "runtime/PyDict.hpp"
#include "runtime/PyModule.hpp"
#include "runtime/PyNone.hpp"
#include "runtime/PyObject.hpp"
#include "runtime/PyType.hpp"
#include "runtime/types/builtin.hpp"

using namespace py;

ExecutionFrame::ExecutionFrame() {}

ExecutionFrame *ExecutionFrame::create(ExecutionFrame *parent,
	size_t register_count,
	size_t free_vars_count,
	PyDict *globals,
	PyDict *locals,
	const PyTuple *consts)
{
	auto *new_frame = Heap::the().allocate<ExecutionFrame>();
	new_frame->m_parent = parent;
	new_frame->m_register_count = register_count;
	new_frame->m_globals = globals;
	new_frame->m_locals = locals;
	new_frame->m_consts = consts;
	// if (parent) {
	// 	new_frame->m_freevars = parent->m_freevars;
	// 	new_frame->m_freevars.resize(parent->m_freevars.size() + free_vars_count, nullptr);
	// } else {
	// 	new_frame->m_freevars = std::vector<PyCell *>(free_vars_count, nullptr);
	// }
	new_frame->m_freevars = std::vector<PyCell *>(free_vars_count, nullptr);

	if (new_frame->m_parent) {
		new_frame->m_builtins = new_frame->m_parent->m_builtins;
	} else {
		ASSERT(new_frame->locals()->map().contains(String{ "__builtins__" }))
		ASSERT(std::get<PyObject *>((*new_frame->m_locals)[String{ "__builtins__" }])->type()
			   == module())
		// TODO: could this just return the builtin singleton?
		new_frame->m_builtins =
			as<PyModule>(std::get<PyObject *>((*new_frame->m_locals)[String{ "__builtins__" }]));
	}
	return new_frame;
}

void ExecutionFrame::set_exception_to_catch(PyObject *exception)
{
	m_exception_to_catch = exception;
}

void ExecutionFrame::set_exception(PyObject *exception)
{
	if (exception) {
		// make sure that we are not accidentally overriding an active exception with another
		// exception
		ASSERT(!m_exception.has_value())
		m_exception = ExceptionInfo{ .exception = exception };
	} else {
		// FIXME: create a ExecutionFrame::clear_exception method instead
		m_exception.reset();
	}
}

void ExecutionFrame::clear_stashed_exception() { m_stashed_exception.reset(); }

void ExecutionFrame::stash_exception() { m_stashed_exception.swap(m_exception); }

bool ExecutionFrame::catch_exception(PyObject *exception) const
{
	if (m_exception_to_catch)
		return exception->type()->issubclass(m_exception_to_catch->type());
	else
		return false;
}

void ExecutionFrame::put_local(const std::string &name, const Value &value)
{
	m_locals->insert(String{ name }, value);
}

void ExecutionFrame::put_global(const std::string &name, const Value &value)
{
	m_globals->insert(String{ name }, value);
}

PyDict *ExecutionFrame::locals() const { return m_locals; }
PyDict *ExecutionFrame::globals() const { return m_globals; }
PyModule *ExecutionFrame::builtins() const { return m_builtins; }

const std::vector<py::PyCell *> &ExecutionFrame::freevars() const { return m_freevars; }
std::vector<py::PyCell *> &ExecutionFrame::freevars() { return m_freevars; }

ExecutionFrame *ExecutionFrame::exit() { return m_parent; }

std::string ExecutionFrame::to_string() const
{
	const auto locals = m_locals ? m_locals->to_string() : "";
	const auto globals = m_globals ? m_globals->to_string() : "";
	const auto builtins = m_builtins ? m_builtins->to_string() : "";
	const void *parent = m_parent ? &m_parent : nullptr;

	return fmt::format("ExecutionFrame(locals={}, globals={}, builtins={}, parent={})",
		locals,
		globals,
		builtins,
		parent);
}

void ExecutionFrame::visit_graph(Visitor &visitor)
{
	visitor.visit(*this);
	if (m_locals) visitor.visit(*m_locals);
	if (m_globals) visitor.visit(*m_globals);
	if (m_builtins) visitor.visit(*m_builtins);
	if (m_exception_to_catch) visitor.visit(*m_exception_to_catch);
	if (m_exception.has_value()) visitor.visit(*m_exception->exception);
	if (m_stashed_exception.has_value()) visitor.visit(*m_stashed_exception->exception);
	if (m_parent) { visitor.visit(*m_parent); }
	for (const auto &freevar : m_freevars) {
		if (freevar) { visitor.visit(*freevar); }
	}
	if (m_consts) { visitor.visit(*const_cast<PyTuple *>(m_consts)); }
}

py::Value ExecutionFrame::consts(size_t index) const
{
	ASSERT(index < m_consts->size())
	spdlog::debug("m_consts: {}", (void*)m_consts);
	return m_consts->elements()[index];
}

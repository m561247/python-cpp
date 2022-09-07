#include "PyCode.hpp"
#include "MemoryError.hpp"
#include "PyCell.hpp"
#include "PyFrame.hpp"
#include "PyFunction.hpp"
#include "PyTuple.hpp"
#include "executable/Function.hpp"
#include "executable/bytecode/Bytecode.hpp"
#include "executable/bytecode/instructions/Instructions.hpp"
#include "executable/bytecode/serialization/deserialize.hpp"
#include "executable/bytecode/serialization/serialize.hpp"
#include "interpreter/Interpreter.hpp"
#include "types/api.hpp"
#include "types/builtin.hpp"

namespace py {

template<> PyCode *as(PyObject *obj)
{
	if (obj->type() == code()) { return static_cast<PyCode *>(obj); }
	return nullptr;
}

template<> const PyCode *as(const PyObject *obj)
{
	if (obj->type() == code()) { return static_cast<const PyCode *>(obj); }
	return nullptr;
}

PyCode::PyCode(std::unique_ptr<Function> &&function,
	std::vector<size_t> &&cell2arg,
	size_t arg_count,
	std::vector<std::string> &&cellvars,
	PyTuple *consts,
	std::string &&filename,
	size_t first_line_number,
	CodeFlags flags,
	std::vector<std::string> &&freevars,
	size_t positional_arg_count,
	size_t kwonly_arg_count,
	size_t stack_size,
	std::string &&name,
	std::vector<std::string> &&names,
	size_t nlocals,
	std::vector<std::string> &&varnames)
	: PyBaseObject(BuiltinTypes::the().code()), m_function(std::move(function)),
	  m_register_count(m_function->register_count()), m_cell2arg(std::move(cell2arg)),
	  m_arg_count(arg_count), m_cellvars(std::move(cellvars)), m_consts(consts),
	  m_filename(std::move(filename)), m_first_line_number(first_line_number), m_flags(flags),
	  m_freevars(std::move(freevars)), m_positional_only_arg_count(positional_arg_count),
	  m_kwonly_arg_count(kwonly_arg_count), m_name(std::move(name)), m_names(std::move(names)),
	  m_nlocals(nlocals), m_stack_size(stack_size), m_varnames(std::move(varnames))
{}

PyResult<PyCode *> PyCode::create(std::unique_ptr<Function> &&function,
	std::vector<size_t> cell2arg,
	size_t arg_count,
	std::vector<std::string> cellvars,
	PyTuple *consts,
	std::string filename,
	size_t first_line_number,
	CodeFlags flags,
	std::vector<std::string> freevars,
	size_t positional_arg_count,
	size_t kwonly_arg_count,
	size_t stack_size,
	std::string name,
	std::vector<std::string> names,
	size_t nlocals,
	std::vector<std::string> varnames)
{
	auto program = function->program();
	auto *result = VirtualMachine::the().heap().allocate<PyCode>(std::move(function),
		std::move(cell2arg),
		arg_count,
		std::move(cellvars),
		consts,
		std::move(filename),
		first_line_number,
		flags,
		std::move(freevars),
		positional_arg_count,
		kwonly_arg_count,
		stack_size,
		std::move(name),
		std::move(names),
		nlocals,
		std::move(varnames));
	if (!result) { return Err(memory_error(sizeof(PyCode))); }
	result->m_program = std::move(program);
	return Ok(result);
}

PyResult<PyCode *> PyCode::create(std::shared_ptr<Program> program)
{
	auto main_function = program->main_function();
	ASSERT(main_function);
	auto *code = as<PyCode>(main_function);
	ASSERT(code)
	code->m_program = std::move(program);
	return Ok(code);
}

PyCode::~PyCode() {}

std::string PyCode::to_string() const
{
	return fmt::format("<code object {} at {}, file \"{}\", line {}>",
		m_name,
		static_cast<const void *>(this),
		m_filename,
		m_first_line_number);
}

PyResult<PyObject *> PyCode::__repr__() const { return PyString::create(to_string()); }

size_t PyCode::register_count() const { return m_register_count; }

size_t PyCode::freevars_count() const { return m_freevars.size(); }

size_t PyCode::cellvars_count() const { return m_cellvars.size(); }

const std::vector<size_t> &PyCode::cell2arg() const { return m_cell2arg; }

size_t PyCode::arg_count() const { return m_arg_count; }

size_t PyCode::kwonly_arg_count() const { return m_kwonly_arg_count; }

CodeFlags PyCode::flags() const { return m_flags; }

PyType *PyCode::type() const { return code(); }

const PyTuple *PyCode::consts() const { return m_consts; }

const std::vector<std::string> &PyCode::names() const { return m_names; }

void PyCode::visit_graph(Visitor &visitor)
{
	PyObject::visit_graph(visitor);
	if (m_consts) { visitor.visit(*const_cast<PyTuple *>(m_consts)); }
	if (m_lnotab) { visitor.visit(*m_lnotab); }
	m_program->visit_functions(visitor);
}

PyObject *PyCode::make_function(const std::string &function_name,
	const std::vector<py::Value> &default_values,
	const std::vector<py::Value> &kw_default_values,
	const std::vector<py::PyCell *> &closure) const
{
	auto *f = m_program->as_pyfunction(function_name, default_values, kw_default_values, closure);
	ASSERT(f)
	return f;
}

PyResult<PyObject *> PyCode::eval(PyDict *globals,
	PyDict *locals,
	PyTuple *args,
	PyDict *kwargs,
	const std::vector<Value> &defaults,
	const std::vector<Value> &kw_defaults,
	const std::vector<PyCell *> &closure,
	PyString *name) const
{
	auto *function_frame = PyFrame::create(VirtualMachine::the().interpreter().execution_frame(),
		register_count(),
		cellvars_count() + freevars_count(),
		const_cast<PyCode *>(this),
		globals,
		locals,
		consts(),
		names());
	[[maybe_unused]] auto scoped_stack =
		VirtualMachine::the().interpreter().setup_call_stack(m_function, function_frame);

	for (size_t i = 0; i < cellvars_count(); ++i) {
		auto cell = PyCell::create();
		if (cell.is_err()) return cell;
		function_frame->freevars()[i] = cell.unwrap();
	}

	const size_t total_arguments_count = m_arg_count + m_kwonly_arg_count;
	std::vector<std::string> positional_args{ m_varnames.begin(),
		m_varnames.begin() + m_arg_count };
	std::vector<std::string> keyword_only_args{ m_varnames.begin() + m_arg_count,
		m_varnames.begin() + total_arguments_count };

	size_t args_count = 0;
	size_t kwargs_count = 0;

	if (args) {
		size_t max_args = std::min(args->size(), m_arg_count);
		for (size_t idx = 0; idx < max_args; ++idx) {
			const auto &obj = args->elements()[idx];
			VirtualMachine::the().stack_local(idx) = obj;
			if (auto it = std::find(m_cell2arg.begin(), m_cell2arg.end(), idx);
				it != m_cell2arg.end()) {
				const auto free_var_idx = std::distance(m_cell2arg.begin(), it);
				auto cell = PyCell::create(obj);
				if (cell.is_err()) return cell;
				function_frame->freevars()[free_var_idx] = cell.unwrap();
			}
		}
		args_count = max_args;
	}
	if (kwargs) {
		const auto &argnames = m_varnames;
		for (const auto &[key, value] : kwargs->map()) {
			ASSERT(std::holds_alternative<String>(key))
			auto key_str = std::get<String>(key);
			auto arg_iter = std::find(m_varnames.begin(), m_varnames.end(), key_str.s);
			if (arg_iter == m_varnames.end()) {
				if (m_flags.is_set(CodeFlags::Flag::VARKEYWORDS)) {
					continue;
				} else {
					return Err(type_error(
						"{}() got an unexpected keyword argument '{}'", name->value(), key_str.s));
				}
			}
			auto &arg =
				VirtualMachine::the().stack_local(std::distance(argnames.begin(), arg_iter));

			if (std::holds_alternative<PyObject *>(arg)) {
				if (std::get<PyObject *>(arg)) {
					return Err(type_error(
						"{}() got multiple values for argument '{}'", name->value(), key_str.s));
				}
			}
			if (auto it = std::find(m_cell2arg.begin(), m_cell2arg.end(), kwargs_count);
				it != m_cell2arg.end()) {
				const auto free_var_idx = std::distance(m_cell2arg.begin(), it);
				auto cell = PyCell::create(value);
				if (cell.is_err()) return cell;
				function_frame->freevars()[free_var_idx] = cell.unwrap();
			}
			arg = value;
			kwargs_count++;
		}
	}

	{
		auto default_iter = defaults.rbegin();
		for (size_t i = m_arg_count - 1; i > (m_arg_count - defaults.size() - 1); --i) {
			auto &arg = VirtualMachine::the().stack_local(i);
			if (std::holds_alternative<PyObject *>(arg) && !std::get<PyObject *>(arg)) {
				VirtualMachine::the().stack_local(i) = *default_iter;
			}
			if (auto it = std::find(m_cell2arg.begin(), m_cell2arg.end(), i);
				it != m_cell2arg.end()) {
				const auto free_var_idx = std::distance(m_cell2arg.begin(), it);
				auto cell = PyCell::create(*default_iter);
				if (cell.is_err()) return cell;
				function_frame->freevars()[free_var_idx] = cell.unwrap();
			}
			default_iter = std::next(default_iter);
		}
	}
	{
		auto kw_default_iter = kw_defaults.rbegin();
		const size_t start = m_kwonly_arg_count + m_arg_count - 1;
		for (size_t i = start; i > start - kw_defaults.size(); --i) {
			auto &arg = VirtualMachine::the().stack_local(i);
			if (std::holds_alternative<PyObject *>(arg) && !std::get<PyObject *>(arg)) {
				VirtualMachine::the().stack_local(i) = *kw_default_iter;
			}
			if (auto it = std::find(m_cell2arg.begin(), m_cell2arg.end(), i);
				it != m_cell2arg.end()) {
				const auto free_var_idx = std::distance(m_cell2arg.begin(), it);
				auto cell = PyCell::create(*kw_default_iter);
				if (cell.is_err()) return cell;
				function_frame->freevars()[free_var_idx] = cell.unwrap();
			}
			kw_default_iter = std::next(kw_default_iter);
		}
	}

	if (m_flags.is_set(CodeFlags::Flag::VARARGS)) {
		std::vector<Value> remaining_args;
		if (args) {
			for (size_t idx = args_count; idx < args->size(); ++idx) {
				remaining_args.push_back(args->elements()[idx]);
			}
		}
		auto args_ = PyTuple::create(remaining_args);
		if (args_.is_err()) { return args_; }
		VirtualMachine::the().stack_local(total_arguments_count) = args_.unwrap();
		size_t i = m_kwonly_arg_count + m_arg_count;
		if (auto it = std::find(m_cell2arg.begin(), m_cell2arg.end(), i); it != m_cell2arg.end()) {
			const auto free_var_idx = std::distance(m_cell2arg.begin(), it);
			auto cell = PyCell::create(args_.unwrap());
			if (cell.is_err()) return cell;
			function_frame->freevars()[free_var_idx] = cell.unwrap();
		}
	} else if (args_count < args->size()) {
		return Err(type_error("{}() takes {} positional arguments but {} given",
			name->value(),
			args_count,
			args->size()));
	}

	if (m_flags.is_set(CodeFlags::Flag::VARKEYWORDS)) {
		auto remaining_kwargs_ = PyDict::create();
		if (remaining_kwargs_.is_err()) { return remaining_kwargs_; }
		auto *remaining_kwargs = remaining_kwargs_.unwrap();
		if (kwargs) {
			const auto &argnames = m_varnames;
			for (const auto &[key, value] : kwargs->map()) {
				auto key_str = std::get<String>(key);
				auto arg_iter = std::find(argnames.begin(), argnames.end(), key_str.s);
				if (arg_iter == argnames.end()) {
					remaining_kwargs->insert(key, value);
					kwargs_count++;
					continue;
				}

				auto &arg =
					VirtualMachine::the().stack_local(std::distance(argnames.begin(), arg_iter));
				if (std::holds_alternative<PyObject *>(arg) && !std::get<PyObject *>(arg)) {
					remaining_kwargs->insert(key, value);
					kwargs_count++;
				}
			}
		}
		size_t kwargs_index = [&]() {
			if (m_flags.is_set(CodeFlags::Flag::VARARGS)) {
				return total_arguments_count + 1;
			} else {
				return total_arguments_count;
			}
		}();
		VirtualMachine::the().stack_local(kwargs_index) = remaining_kwargs;
		size_t i = m_kwonly_arg_count + m_arg_count;
		if (m_flags.is_set(CodeFlags::Flag::VARARGS)) { i++; }
		if (auto it = std::find(m_cell2arg.begin(), m_cell2arg.end(), i); it != m_cell2arg.end()) {
			const auto free_var_idx = std::distance(m_cell2arg.begin(), it);
			auto cell = PyCell::create(remaining_kwargs);
			if (cell.is_err()) return cell;
			function_frame->freevars()[free_var_idx] = cell.unwrap();
		}
	}

	spdlog::debug("Requesting stack frame with {} virtual registers", m_register_count);

	for (size_t idx = m_cellvars.size(); const auto &el : closure) {
		function_frame->freevars()[idx++] = el;
	}

	// spdlog::debug("Frame: {}", (void *)execution_frame);
	// spdlog::debug("Locals: {}", execution_frame->locals()->to_string());
	// spdlog::debug("Globals: {}", execution_frame->globals()->to_string());
	// if (ns) { spdlog::info("Namespace: {}", ns->to_string()); }
	return VirtualMachine::the().interpreter().call(m_function, function_frame);
}

std::vector<uint8_t> PyCode::serialize() const
{
	std::vector<uint8_t> result;
	auto serialized_function = m_function->serialize();
	result.reserve(serialized_function.size());
	for (const auto &el : serialized_function) { result.push_back(el); }

	::py::serialize(m_cell2arg, result);
	::py::serialize(m_arg_count, result);
	::py::serialize(m_cellvars, result);
	::py::serialize(m_consts, result);
	::py::serialize(m_filename, result);
	::py::serialize(m_first_line_number, result);
	::py::serialize(static_cast<uint8_t>(m_flags.bits().to_ulong()), result);
	::py::serialize(m_freevars, result);
	::py::serialize(m_positional_only_arg_count, result);
	::py::serialize(m_kwonly_arg_count, result);
	::py::serialize(m_stack_size, result);
	::py::serialize(m_name, result);
	::py::serialize(m_names, result);
	::py::serialize(m_nlocals, result);
	::py::serialize(m_varnames, result);

	return result;
}

std::pair<PyResult<PyCode *>, size_t> PyCode::deserialize(std::span<const uint8_t> &buffer,
	std::shared_ptr<Program> program)
{
	auto function = Bytecode::deserialize(buffer, program);
	const auto cell2arg = ::py::deserialize<std::vector<size_t>>(buffer);
	const auto arg_count = ::py::deserialize<size_t>(buffer);
	const auto cellvars = ::py::deserialize<std::vector<std::string>>(buffer);
	const auto consts = ::py::deserialize<PyTuple>(buffer);
	if (consts.is_err()) { return { Err(consts.unwrap_err()), 0 }; }
	const auto filename = ::py::deserialize<std::string>(buffer);
	const auto first_line_number = ::py::deserialize<size_t>(buffer);
	const auto flags = ::py::deserialize<uint8_t>(buffer);
	const auto freevars = ::py::deserialize<std::vector<std::string>>(buffer);
	const auto positional_only_arg_count = ::py::deserialize<size_t>(buffer);
	const auto kwonly_arg_count = ::py::deserialize<size_t>(buffer);
	const auto stack_size = ::py::deserialize<size_t>(buffer);
	const auto name = ::py::deserialize<std::string>(buffer);
	const auto names = ::py::deserialize<std::vector<std::string>>(buffer);
	const auto nlocals = ::py::deserialize<size_t>(buffer);
	const auto varnames = ::py::deserialize<std::vector<std::string>>(buffer);

	return { PyCode::create(std::move(function),
				 cell2arg,
				 arg_count,
				 cellvars,
				 consts.unwrap(),
				 filename,
				 first_line_number,
				 CodeFlags::from_byte(flags),
				 freevars,
				 positional_only_arg_count,
				 kwonly_arg_count,
				 stack_size,
				 name,
				 names,
				 nlocals,
				 varnames),
		0 };
}


namespace {
	std::once_flag code_flag;

	std::unique_ptr<TypePrototype> register_code()
	{
		return std::move(klass<PyCode>("code").attr("co_consts", &PyCode::m_consts).type);
	}
}// namespace

std::function<std::unique_ptr<TypePrototype>()> PyCode::type_factory()
{
	return [] {
		static std::unique_ptr<TypePrototype> type = nullptr;
		std::call_once(code_flag, []() { type = register_code(); });
		return std::move(type);
	};
}
}// namespace py
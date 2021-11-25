#include "PyTuple.hpp"
#include "PyString.hpp"
#include "StopIterationException.hpp"
#include "interpreter/Interpreter.hpp"
#include "types/api.hpp"
#include "types/builtin.hpp"
#include "vm/VM.hpp"

PyTuple::PyTuple() : PyBaseObject(PyObjectType::PY_TUPLE, BuiltinTypes::the().tuple()) {}

PyTuple::PyTuple(std::vector<Value> &&elements) : PyTuple() { m_elements = std::move(elements); }

PyTuple::PyTuple(const std::vector<PyObject *> &elements) : PyTuple()
{
	m_elements.reserve(elements.size());
	for (auto *el : elements) { m_elements.push_back(el); }
}

PyTuple *PyTuple::create()
{
	auto &heap = VirtualMachine::the().heap();
	return heap.allocate<PyTuple>();
}

PyTuple *PyTuple::create(std::vector<Value> elements)
{
	auto &heap = VirtualMachine::the().heap();
	return heap.allocate<PyTuple>(std::move(elements));
}

PyTuple *PyTuple::create(const std::vector<PyObject *> &elements)
{
	auto &heap = VirtualMachine::the().heap();
	return heap.allocate<PyTuple>(elements);
}

std::string PyTuple::to_string() const
{
	std::ostringstream os;

	os << "(";
	auto it = m_elements.begin();
	while (std::next(it) != m_elements.end()) {
		std::visit([&os](const auto &value) { os << value << ", "; }, *it);
		std::advance(it, 1);
	}
	std::visit([&os](const auto &value) { os << value; }, *it);
	os << ")";

	return os.str();
}

PyObject *PyTuple::__repr__() const { return PyString::create(to_string()); }

PyObject *PyTuple::__iter__() const
{
	auto &heap = VirtualMachine::the().heap();
	return heap.allocate<PyTupleIterator>(*this);
}

PyTupleIterator PyTuple::begin() const { return PyTupleIterator(*this); }

PyTupleIterator PyTuple::end() const { return PyTupleIterator(*this, m_elements.size()); }

PyObject *PyTuple::operator[](size_t idx) const
{
	return std::visit([](const auto &value) { return PyObject::from(value); }, m_elements[idx]);
}

void PyTuple::visit_graph(Visitor &visitor)
{
	PyObject::visit_graph(visitor);
	for (auto &el : m_elements) {
		if (std::holds_alternative<PyObject *>(el)) {
			if (std::get<PyObject *>(el) != this) std::get<PyObject *>(el)->visit_graph(visitor);
		}
	}
}

PyType *PyTuple::type_() const { return tuple(); }

namespace {

std::once_flag tuple_flag;

std::unique_ptr<TypePrototype> register_tuple() { return std::move(klass<PyTuple>("tuple").type); }
}// namespace

std::unique_ptr<TypePrototype> PyTuple::register_type()
{
	static std::unique_ptr<TypePrototype> type = nullptr;
	std::call_once(tuple_flag, []() { type = ::register_tuple(); });
	return std::move(type);
}


PyTupleIterator::PyTupleIterator(const PyTuple &pytuple)
	: PyBaseObject(PyObjectType::PY_TUPLE_ITERATOR, BuiltinTypes::the().tuple_iterator()),
	  m_pytuple(pytuple)
{}

PyTupleIterator::PyTupleIterator(const PyTuple &pytuple, size_t position) : PyTupleIterator(pytuple)
{
	m_current_index = position;
}

std::string PyTupleIterator::to_string() const
{
	return fmt::format("<tuple_iterator at {}>", static_cast<const void *>(this));
}

PyObject *PyTupleIterator::__repr__() const { return PyString::create(to_string()); }

PyObject *PyTupleIterator::__next__()
{
	if (m_current_index < m_pytuple.elements().size())
		return std::visit([](const auto &element) { return PyObject::from(element); },
			m_pytuple.elements()[m_current_index++]);
	VirtualMachine::the().interpreter().raise_exception(stop_iteration(""));
	return nullptr;
}

bool PyTupleIterator::operator==(const PyTupleIterator &other) const
{
	return &m_pytuple == &other.m_pytuple && m_current_index == other.m_current_index;
}

PyTupleIterator &PyTupleIterator::operator++()
{
	m_current_index++;
	return *this;
}

PyTupleIterator &PyTupleIterator::operator--()
{
	m_current_index--;
	return *this;
}

PyObject *PyTupleIterator::operator*() const
{
	return std::visit([](const auto &element) { return PyObject::from(element); },
		m_pytuple.elements()[m_current_index]);
}

PyType *PyTupleIterator::type_() const { return tuple_iterator(); }

namespace {

std::once_flag tuple_iterator_flag;

std::unique_ptr<TypePrototype> register_tuple_iterator()
{
	return std::move(klass<PyTupleIterator>("tuple_iterator").type);
}
}// namespace

std::unique_ptr<TypePrototype> PyTupleIterator::register_type()
{
	static std::unique_ptr<TypePrototype> type = nullptr;
	std::call_once(tuple_iterator_flag, []() { type = ::register_tuple_iterator(); });
	return std::move(type);
}
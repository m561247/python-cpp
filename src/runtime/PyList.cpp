#include "PyList.hpp"
#include "IndexError.hpp"
#include "MemoryError.hpp"
#include "PyBool.hpp"
#include "PyDict.hpp"
#include "PyFunction.hpp"
#include "PyGenericAlias.hpp"
#include "PyInteger.hpp"
#include "PyNone.hpp"
#include "PyNumber.hpp"
#include "PySlice.hpp"
#include "PyString.hpp"
#include "PyTuple.hpp"
#include "StopIteration.hpp"
#include "ValueError.hpp"
#include "interpreter/Interpreter.hpp"
#include "types/api.hpp"
#include "types/builtin.hpp"
#include "vm/VM.hpp"

using namespace py;

template<> PyList *py::as(PyObject *obj)
{
	if (obj->type() == list()) { return static_cast<PyList *>(obj); }
	return nullptr;
}

template<> const PyList *py::as(const PyObject *obj)
{
	if (obj->type() == list()) { return static_cast<const PyList *>(obj); }
	return nullptr;
}

PyList::PyList() : PyBaseObject(BuiltinTypes::the().list()) {}

PyList::PyList(std::vector<Value> elements) : PyList() { m_elements = std::move(elements); }

PyResult<PyList *> PyList::create(std::vector<Value> elements)
{
	auto *result = VirtualMachine::the().heap().allocate<PyList>(elements);
	if (!result) { return Err(memory_error(sizeof(PyList))); }
	return Ok(result);
}

PyResult<PyList *> PyList::create()
{
	auto *result = VirtualMachine::the().heap().allocate<PyList>();
	if (!result) { return Err(memory_error(sizeof(PyList))); }
	return Ok(result);
}

PyResult<PyObject *> PyList::append(PyObject *element)
{
	m_elements.push_back(element);
	return Ok(py_none());
}

PyResult<PyObject *> PyList::extend(PyObject *iterable)
{
	auto iterator = iterable->iter();
	if (iterator.is_err()) return iterator;

	auto tmp_list_ = PyList::create();
	if (tmp_list_.is_err()) return tmp_list_;
	auto *tmp_list = tmp_list_.unwrap();
	auto value = iterator.unwrap()->next();
	while (value.is_ok()) {
		tmp_list->append(value.unwrap());
		value = iterator.unwrap()->next();
	}

	if (!value.unwrap_err()->type()->issubclass(stop_iteration()->type())) { return value; }

	m_elements.insert(m_elements.end(), tmp_list->elements().begin(), tmp_list->elements().end());

	return Ok(py_none());
}

PyResult<PyObject *> PyList::pop(PyObject *index)
{
	if (m_elements.empty()) { return Err(index_error("pop from empty list")); }

	if (index) {
		if (!as<PyInteger>(index)) {
			return Err(type_error(
				"'{}' object cannot be interpreted as an integer", index->type()->name()));
		}
		auto idx = [index, this]() -> PyResult<size_t> {
			auto idx_value = as<PyInteger>(index)->as_i64();
			size_t idx = m_elements.size();
			if (idx_value < 0) {
				if (static_cast<uint64_t>(std::abs(idx_value)) > m_elements.size()) {
					return Err(index_error("pop index '{}' out of range for list of size '{}'",
						idx,
						m_elements.size()));
				}
				idx += idx_value;
			} else {
				idx = static_cast<size_t>(idx_value);
			}
			if (idx >= m_elements.size()) {
				return Err(index_error(
					"pop index '{}' out of range for list of size '{}'", idx, m_elements.size()));
			}
			return Ok(idx);
		}();
		return idx.and_then([this](size_t idx) {
			return PyObject::from(m_elements[idx]).and_then([this, idx](PyObject *el) {
				if (idx == m_elements.size()) {
					m_elements.pop_back();
				} else {
					m_elements.erase(m_elements.begin() + idx);
				}
				return Ok(el);
			});
		});
	} else {
		return PyObject::from(m_elements.back()).and_then([this](PyObject *el) {
			m_elements.pop_back();
			return Ok(el);
		});
	}
}

std::string PyList::to_string() const
{
	std::ostringstream os;

	os << "[";
	if (!m_elements.empty()) {
		auto it = m_elements.begin();
		while (std::next(it) != m_elements.end()) {
			std::visit(overloaded{
						   [&os](const auto &value) { os << value << ", "; },
						   [&os](PyObject *value) { os << value->to_string() << ", "; },
					   },
				*it);
			std::advance(it, 1);
		}
		std::visit(overloaded{
					   [&os](const auto &value) { os << value; },
					   [&os](PyObject *value) { os << value->to_string(); },
				   },
			*it);
	}
	os << "]";

	return os.str();
}

PyResult<PyObject *> PyList::__repr__() const { return PyString::create(to_string()); }

PyResult<PyObject *> PyList::__iter__() const
{
	auto &heap = VirtualMachine::the().heap();
	auto *it = heap.allocate<PyListIterator>(*this);
	if (!it) { return Err(memory_error(sizeof(PyListIterator))); }
	return Ok(it);
}

PyResult<PyObject *> PyList::__getitem__(int64_t index)
{
	if (index >= 0) {
		if (static_cast<size_t>(index) >= m_elements.size()) {
			return Err(index_error("list index out of range"));
		}
		return PyObject::from(m_elements[index]);
	} else {
		// TODO: write wrap around logic
		TODO();
	}
}

PyResult<std::monostate> PyList::__setitem__(int64_t index, PyObject *value)
{
	if (index >= 0) {
		if (static_cast<size_t>(index) >= m_elements.size()) {
			return Err(index_error("list index out of range"));
		}
		m_elements[index] = value;
		return Ok(std::monostate{});
	} else {
		// TODO: write wrap around logic
		TODO();
	}
}

PyResult<PyObject *> PyList::__getitem__(PyObject *index)
{
	if (auto index_int = as<PyInteger>(index)) {
		const auto i = index_int->as_i64();
		return __getitem__(i);
	} else if (auto slice = as<PySlice>(index)) {
		auto indices_ = slice->unpack();
		if (indices_.is_err()) return Err(indices_.unwrap_err());
		const auto [start_, end_, step] = indices_.unwrap();

		const auto [start, end, slice_length] =
			PySlice::adjust_indices(start_, end_, step, m_elements.size());

		if (slice_length == 0) { return PyList::create(); }
		if (start == 0 && end == static_cast<int64_t>(m_elements.size()) && step == 1) {
			// shallow copy of the list since we need all elements
			return PyList::create(m_elements);
		}

		auto new_list = PyList::create();
		if (new_list.is_err()) return new_list;

		for (int64_t idx = start, i = 0; i < slice_length; idx += step, ++i) {
			new_list.unwrap()->elements().push_back(m_elements[idx]);
		}
		return new_list;
	} else {
		return Err(
			type_error("list indices must be integers or slices, not {}", index->type()->name()));
	}
}

PyResult<size_t> PyList::__len__() const { return Ok(m_elements.size()); }

PyResult<PyObject *> PyList::__eq__(const PyObject *other) const
{
	if (!as<PyList>(other)) { return Ok(py_false()); }

	auto *other_list = as<PyList>(other);
	// Value contains PyObject* so we can't just compare vectors with std::vector::operator==
	// otherwise if we compare PyObject* with PyObject* we compare the pointers, rather
	// than PyObject::__eq__(const PyObject*)
	if (m_elements.size() != other_list->elements().size()) { return Ok(py_false()); }
	auto &interpreter = VirtualMachine::the().interpreter();
	const bool result = std::equal(m_elements.begin(),
		m_elements.end(),
		other_list->elements().begin(),
		[&interpreter](const auto &lhs, const auto &rhs) -> bool {
			const auto &result = equals(lhs, rhs, interpreter);
			ASSERT(result.is_ok())
			auto is_true = truthy(result.unwrap(), interpreter);
			ASSERT(is_true.is_ok())
			return is_true.unwrap();
		});
	return Ok(result ? py_true() : py_false());
}

PyResult<PyObject *> PyList::__reversed__() const
{
	return PyListReverseIterator::create(*const_cast<PyList *>(this));
}

void PyList::sort()
{
	std::sort(m_elements.begin(), m_elements.end(), [](const Value &lhs, const Value &rhs) -> bool {
		if (auto cmp = less_than(lhs, rhs, VirtualMachine::the().interpreter()); cmp.is_ok()) {
			auto is_true = truthy(cmp.unwrap(), VirtualMachine::the().interpreter());
			ASSERT(is_true.is_ok())
			return is_true.unwrap();
		} else {
			// VirtualMachine::the().interpreter().raise_exception("Failed to compare {} with {}",
			// 	PyObject::from(lhs)->to_string(),
			// 	PyObject::from(rhs)->to_string());
			return false;
		}
	});
}

void PyList::visit_graph(Visitor &visitor)
{
	PyObject::visit_graph(visitor);
	for (auto &el : m_elements) {
		if (std::holds_alternative<PyObject *>(el)) {
			if (std::get<PyObject *>(el) != this) visitor.visit(*std::get<PyObject *>(el));
		}
	}
}

PyType *PyList::type() const { return list(); }

namespace {

std::once_flag list_flag;

std::unique_ptr<TypePrototype> register_list()
{
	return std::move(
		klass<PyList>("list")
			.def("append", &PyList::append)
			.def("extend", &PyList::extend)
			.def(
				"pop",
				+[](PyObject *self, PyTuple *args, PyDict *kwargs) -> PyResult<PyObject *> {
					auto result = PyArgsParser<PyObject *>::unpack_tuple(args,
						kwargs,
						"pop",
						std::integral_constant<size_t, 0>{},
						std::integral_constant<size_t, 1>{},
						nullptr);
					if (result.is_err()) return Err(result.unwrap_err());
					return static_cast<PyList *>(self)->pop(std::get<0>(result.unwrap()));
				})
			//  .def(
			// 	 "sort",
			// 	 +[](PyObject *self) {
			// 		 self->sort();
			// 		 return py_none();
			// 	 })
			.classmethod(
				"__class_getitem__",
				+[](PyType *type, PyTuple *args, PyDict *kwargs) {
					ASSERT(args && args->elements().size() == 1);
					ASSERT(!kwargs || kwargs->map().empty());
					return PyObject::from(args->elements()[0]).and_then([type](PyObject *arg) {
						return PyGenericAlias::create(type, arg);
					});
				})
			.def("__reversed__", &PyList::__reversed__)
			.type);
}
}// namespace

std::function<std::unique_ptr<TypePrototype>()> PyList::type_factory()
{
	return [] {
		static std::unique_ptr<TypePrototype> type = nullptr;
		std::call_once(list_flag, []() { type = ::register_list(); });
		return std::move(type);
	};
}


PyListIterator::PyListIterator(const PyList &pylist)
	: PyBaseObject(BuiltinTypes::the().list_iterator()), m_pylist(pylist)
{}

std::string PyListIterator::to_string() const
{
	return fmt::format("<list_iterator at {}>", static_cast<const void *>(this));
}

void PyListIterator::visit_graph(Visitor &visitor)
{
	PyObject::visit_graph(visitor);
	// the iterator has to keep a reference to the list
	// otherwise GC could clean up a temporary list in a loop
	// TODO: should visit_graph be const and the bit flags mutable?
	const_cast<PyList &>(m_pylist).visit_graph(visitor);
}

PyResult<PyObject *> PyListIterator::__repr__() const { return PyString::create(to_string()); }

PyResult<PyObject *> PyListIterator::__next__()
{
	if (m_current_index < m_pylist.elements().size())
		return std::visit([](const auto &element) { return PyObject::from(element); },
			m_pylist.elements()[m_current_index++]);
	return Err(stop_iteration());
}

PyType *PyListIterator::type() const { return list_iterator(); }

namespace {

std::once_flag list_iterator_flag;

std::unique_ptr<TypePrototype> register_list_iterator()
{
	return std::move(klass<PyListIterator>("list_iterator").type);
}
}// namespace

std::function<std::unique_ptr<TypePrototype>()> PyListIterator::type_factory()
{
	return [] {
		static std::unique_ptr<TypePrototype> type = nullptr;
		std::call_once(list_iterator_flag, []() { type = ::register_list_iterator(); });
		return std::move(type);
	};
}

PyListReverseIterator::PyListReverseIterator(PyList &pylist, size_t start_index)
	: PyBaseObject(BuiltinTypes::the().list_reverseiterator()), m_pylist(pylist),
	  m_current_index(start_index)
{}

PyResult<PyListReverseIterator *> PyListReverseIterator::create(PyList &lst)
{
	auto list_size = lst.elements().size();
	auto *result = VirtualMachine::the().heap().allocate<PyListReverseIterator>(lst, list_size - 1);
	if (!result) { return Err(memory_error(sizeof(PyListReverseIterator))); }
	return Ok(result);
}

void PyListReverseIterator::visit_graph(Visitor &visitor)
{
	PyObject::visit_graph(visitor);
	if (m_pylist.has_value()) { visitor.visit(m_pylist->get()); }
}

PyResult<PyObject *> PyListReverseIterator::__iter__() const
{
	return Ok(const_cast<PyListReverseIterator *>(this));
}

PyResult<PyObject *> PyListReverseIterator::__next__()
{
	if (m_pylist.has_value()) {
		if (m_current_index < m_pylist->get().elements().size())
			return std::visit([](const auto &element) { return PyObject::from(element); },
				m_pylist->get().elements()[m_current_index--]);
		m_pylist = std::nullopt;
	}
	return Err(stop_iteration());
}

PyType *PyListReverseIterator::type() const { return list_reverseiterator(); }

namespace {

std::once_flag list_reverseiterator_flag;

std::unique_ptr<TypePrototype> register_list_reverseiterator()
{
	return std::move(klass<PyListReverseIterator>("list_reverseiterator").type);
}
}// namespace

std::function<std::unique_ptr<TypePrototype>()> PyListReverseIterator::type_factory()
{
	return [] {
		static std::unique_ptr<TypePrototype> type = nullptr;
		std::call_once(
			list_reverseiterator_flag, []() { type = ::register_list_reverseiterator(); });
		return std::move(type);
	};
}

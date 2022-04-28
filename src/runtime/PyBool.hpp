#pragma once

#include "PyObject.hpp"

namespace py {

class PyBool : public PyBaseObject
{
	friend class ::Heap;
	friend PyObject *py_true();
	friend PyObject *py_false();

	bool m_value;

  public:
	std::string to_string() const override;

	bool value() const { return m_value; }

	void visit_graph(Visitor &) override {}

	PyResult __add__(const PyObject *obj) const;
	PyResult __repr__() const;
	PyResult __bool__() const;

	static std::unique_ptr<TypePrototype> register_type();
	PyType *type() const override;

  private:
	static PyResult create(bool);

	PyBool(bool name);
};

PyObject *py_true();
PyObject *py_false();

}// namespace py
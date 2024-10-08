#include "PyNumber.hpp"
#include "PyFloat.hpp"
#include "PyInteger.hpp"
#include "PyString.hpp"
#include "PyType.hpp"
#include "TypeError.hpp"
#include "PyNone.hpp"
#include "PyObject.hpp"
#include "Value.hpp"
#include "NotImplemented.hpp"
#include "types/builtin.hpp"

#include "interpreter/Interpreter.hpp"
#include <variant>

namespace py {

PyNumber::PyNumber(PyType *type) : PyBaseObject(type) {}

std::string PyNumber::to_string() const { return m_value.to_string(); }

PyResult<PyObject *> PyNumber::__repr__() const { return PyString::create(to_string()); }

const PyNumber *PyNumber::as_number(const PyObject *obj)
{
	if (obj->type()->issubclass(types::float_())) {
		return static_cast<const PyFloat *>(obj);
	} else if (obj->type()->issubclass(types::integer())) {
		return static_cast<const PyInteger *>(obj);
	}
	return nullptr;
}

PyResult<PyObject *> PyNumber::__abs__() const
{
	return PyNumber::create(
		std::visit(overloaded{ [](const auto &val) { return Number{ std::abs(val) }; },
					   [](const mpz_class &val) {
						   mpz_class result{};
						   mpz_abs(result.get_mpz_t(), val.get_mpz_t());
						   return Number{ result };
					   } },
			m_value.value));
}

PyResult<PyObject *> PyNumber::__neg__() const
{
	return PyNumber::create(
		std::visit([](const auto &val) { return Number{ -val }; }, m_value.value));
}

PyResult<PyObject *> PyNumber::__pos__() const
{
	return PyNumber::create(
		std::visit([](const auto &val) { return Number{ val }; }, m_value.value));
}

PyResult<PyObject *> PyNumber::__invert__() const
{
	if (std::holds_alternative<double>(m_value.value)) {
		return Err(type_error("bad operand type for unary ~: 'float'"));
	}
	return PyNumber::create(Number{ ~std::get<BigIntType>(m_value.value) });
}

PyResult<PyObject *> PyNumber::__add__(const PyObject *obj) const
{
	if (auto *rhs = as_number(obj)) {
		return PyNumber::create(m_value + rhs->value());
	} else {
		return Err(type_error("unsupported operand type(s) for +: \'{}\' and \'{}\'",
			type()->name(),
			obj->type()->name()));
	}
}

PyResult<PyObject *> PyNumber::__sub__(const PyObject *obj) const
{
	if (auto rhs = as_number(obj)) {
		return PyNumber::create(m_value - rhs->value());
	} else {
		return Err(type_error("unsupported operand type(s) for -: \'{}\' and \'{}\'",
			type()->name(),
			obj->type()->name()));
	}
}

PyResult<PyObject *> PyNumber::__mod__(const PyObject *obj) const
{
	if (auto rhs = as_number(obj)) {
		return PyNumber::create(m_value % rhs->value());
	} else {
		return Err(type_error("unsupported operand type(s) for %: \'{}\' and \'{}\'",
			type()->name(),
			obj->type()->name()));
	}
}

PyResult<PyObject *> PyNumber::__mul__(const PyObject *obj) const
{
	if (auto rhs = as_number(obj)) {
		return PyNumber::create(m_value * rhs->value());
	} else {
		return Err(type_error("unsupported operand type(s) for *: \'{}\' and \'{}\'",
			type()->name(),
			obj->type()->name()));
	}
}

PyResult<PyObject *> PyNumber::__pow__(const PyObject *obj, const PyObject *modulo_obj) const
{
	if (auto rhs_ = as_number(obj)) {
		if (modulo_obj == py_none()) {
			return PyObject::from(m_value.exp(rhs_->value()));
		}
		auto modulo_ = as_number(modulo_obj);
		if (!modulo_) {
			return Ok(not_implemented());
		}
		mpz_class result{};
		mpz_class lhs = std::holds_alternative<BigIntType>( m_value.value) ? std::get<BigIntType>(m_value.value) : BigIntType{std::get<double>(m_value.value)};
		mpz_class rhs = std::holds_alternative<BigIntType>( rhs_->value().value) ? std::get<BigIntType>(rhs_->value().value) : BigIntType{std::get<double>(rhs_->value().value)};
		mpz_class modulo = std::holds_alternative<BigIntType>( modulo_->value().value) ? std::get<BigIntType>(modulo_->value().value) : BigIntType{std::get<double>(modulo_->value().value)};
		mpz_powm(result.get_mpz_t(), lhs.get_mpz_t(), rhs.get_mpz_t(), modulo.get_mpz_t());

		return PyNumber::create(Number{result});
	} else {
		return Err(type_error("unsupported operand type(s) for **: \'{}\' and \'{}\'",
			type()->name(),
			obj->type()->name()));
	}
}

PyResult<PyObject *> PyNumber::__truediv__(const PyObject *obj) const
{
	if (auto rhs = as_number(obj)) {
		return PyNumber::create(m_value / rhs->value());
	} else {
		return Err(type_error("unsupported operand type(s) for /: \'{}\' and \'{}\'",
			type()->name(),
			obj->type()->name()));
	}
}

PyResult<PyObject *> PyNumber::__floordiv__(const PyObject *obj) const
{
	if (auto rhs = as_number(obj)) {
		return PyNumber::create(m_value.floordiv(rhs->value()));
	} else {
		return Err(type_error("unsupported operand type(s) for //: \'{}\' and \'{}\'",
			type()->name(),
			obj->type()->name()));
	}
}

PyResult<PyObject *> PyNumber::__eq__(const PyObject *obj) const
{
	if (auto *pynum = as_number(obj)) {
		const bool comparisson = m_value == pynum->value();
		return PyObject::from(NameConstant{ comparisson });
	}
	return Err(type_error("'==' not supported between instances of '{}' and '{}'",
		type()->name(),
		obj->type()->name()));
}


PyResult<PyObject *> PyNumber::__ne__(const PyObject *obj) const
{
	if (auto *pynum = as_number(obj)) {
		const bool comparisson = m_value != pynum->value();
		return PyObject::from(NameConstant{ comparisson });
	}
	return Err(type_error("'!=' not supported between instances of '{}' and '{}'",
		type()->name(),
		obj->type()->name()));
}


PyResult<PyObject *> PyNumber::__lt__(const PyObject *other) const
{
	if (auto *pynum = as_number(other)) {
		const bool comparisson = m_value < pynum->value();
		return PyObject::from(NameConstant{ comparisson });
	}
	return Err(type_error("'<' not supported between instances of '{}' and '{}'",
		type()->name(),
		other->type()->name()));
}

PyResult<PyObject *> PyNumber::__le__(const PyObject *other) const
{
	if (auto *pynum = as_number(other)) {
		const bool comparisson = m_value <= pynum->value();
		return PyObject::from(NameConstant{ comparisson });
	}
	return Err(type_error("'<=' not supported between instances of '{}' and '{}'",
		type()->name(),
		other->type()->name()));
}

PyResult<PyObject *> PyNumber::__gt__(const PyObject *other) const
{
	if (auto *pynum = as_number(other)) {
		const bool comparisson = m_value > pynum->value();
		return PyObject::from(NameConstant{ comparisson });
	}
	return Err(type_error("'>' not supported between instances of '{}' and '{}'",
		type()->name(),
		other->type()->name()));
}

PyResult<PyObject *> PyNumber::__ge__(const PyObject *other) const
{
	if (auto *pynum = as_number(other)) {
		const bool comparisson = m_value >= pynum->value();
		return PyObject::from(NameConstant{ comparisson });
	}
	return Err(type_error("'>=' not supported between instances of '{}' and '{}'",
		type()->name(),
		other->type()->name()));
}

PyResult<bool> PyNumber::__bool__() const
{
	if (std::holds_alternative<double>(m_value.value)) {
		return Ok(std::fpclassify(std::get<double>(m_value.value)) != FP_ZERO);
	} else {
		return Ok(static_cast<bool>(std::get<BigIntType>(m_value.value)));
	}
}

PyResult<PyNumber *> PyNumber::create(const Number &number)
{
	if (std::holds_alternative<double>(number.value)) {
		return PyFloat::create(std::get<double>(number.value));
	} else {
		return PyInteger::create(std::get<mpz_class>(number.value));
	}
}
}// namespace py

class A:
    a = 1
    def __init__(self):
        self.a = 2

    def __repr__(self):
        return "foo"

class B:
    a = 1
    def __init__(self):
        self.a = -1

    def __repr__(self):
        return "foob"

    def foo(self, other):
        return other

a = A()
assert a.__repr__() == "foo"
assert a.a == 2
b = B()
assert b.__repr__() == "foob"
assert b.foo(a).a == a.a
assert b.a == -1
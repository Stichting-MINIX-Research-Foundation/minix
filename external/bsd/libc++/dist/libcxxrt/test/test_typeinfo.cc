#include "../src/typeinfo.h"
#include "test.h"
#include <stdio.h>

struct Virt1;
struct Virt2;
struct Diamond;
struct Virt1a;
struct Virt2a;
struct Diamond2;

struct Root
{
	int test;
	void * foo;
	virtual Virt1 *as_v1() { return 0; }
	virtual Virt2 *as_v2() { return 0; }
	virtual Diamond *as_diamond() { return 0; }
	virtual Virt1a *as_v1a() { return 0; }
	virtual Virt2a *as_v2a() { return 0; }
	virtual Diamond2 *as_diamond2() { return 0; }
};

struct Sub1 : public Root
{
	double a;
};

struct Sub2 : public  Sub1
{
	float ignored;
};

struct Virt1a : public virtual Root
{
	int b;
	virtual Virt1a *as_v1a() { return this; }
};

struct Virt2a : public virtual Root
{
	int b;
	virtual Virt2a *as_v2a() { return this; }
};

struct Virt1 : public virtual Virt1a
{
	double a;
	virtual Virt1 *as_v1() { return this; }
};

struct Virt2 : public virtual Virt2a
{
	double b;
	virtual Virt2 *as_v2() { return this; }
};

struct Diamond : public virtual Virt1, public virtual Virt2
{
	int c;
	virtual Diamond *as_diamond() { return this; }
};

struct Diamond2 : public virtual Virt1a, public virtual Virt2a
{
	int c;
	virtual Diamond2 *as_diamond2() { return this; }
};

void test_type_info(void)
{
	Sub2 sub2;
	Root root;
	Virt1 virt1;
	Diamond diamond;
	Root *b = &sub2;
	Root *f = &sub2;
	Root *s2 = &sub2;
	Root *b2 = &root;
	Root *v1 = &virt1;
	Virt1 *d1 = &diamond;
	Root *up = &diamond;
	b->test = 12;
	f->test = 12;
	b2->test = 12;
	s2->test = 12;
	TEST(12 == b->test, "Setting field");
	b = dynamic_cast<Root*>(f);

	TEST(12 == b->test, "Casting Sub1 to superclass");
	((Sub1*)(s2))->a = 12;
	TEST(12 == dynamic_cast<Sub1*>(s2)->a, "Casting Sub2 -> Sub1");

	v1->as_v1()->a = 12;
	TEST(12 == dynamic_cast<Virt1*>(v1)->a, "Casting Root (Virt1) -> Virt1");

	d1->as_v1()->test = 12;
	TEST(12 == d1->as_v2()->test, "Accessing virt2 via vtable method");
	TEST(12 == dynamic_cast<Virt2*>(d1)->test, "Casting diamond to virt2");
	TEST(12 == dynamic_cast<Diamond*>(d1)->test, "casting diamond to diamond");

	Diamond2 diamond2;
	Root *d2 = &diamond2;
	d2->test = 12;
	TEST(12 == dynamic_cast<Diamond2*>(d2)->test, "Casting Diamond2 to Diamond2");
	TEST(12 == dynamic_cast<Virt2a*>(d2)->test, "Casting Diamond2 to Virt2a");
	TEST(&diamond == dynamic_cast<Diamond*>(up), "Downcasting root-pointer to diamond");
	TEST(0 == dynamic_cast<Diamond*>(&root), "Downcasting root to diamond");

	TEST(0 == dynamic_cast<Sub1*>(b2), "Casting Root to Sub1 (0 expected)");
}


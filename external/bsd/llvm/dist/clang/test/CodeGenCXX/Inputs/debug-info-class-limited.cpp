
// CHECK-DAG: [ DW_TAG_structure_type ] [PR16214] [line [[@LINE+1]], {{.*}} [def]
struct PR16214 {
  int i;
};

typedef PR16214 bar;

bar *a;
bar b;

namespace PR14467 {
// CHECK-DAG: [ DW_TAG_structure_type ] [foo] [line [[@LINE+1]], {{.*}} [def]
struct foo {
};

foo *bar(foo *a) {
  foo *b = new foo(*a);
  return b;
}
}

namespace test1 {
// CHECK-DAG: [ DW_TAG_structure_type ] [foo] [line [[@LINE+1]], {{.*}} [def]
struct foo {
};

extern int bar(foo *a);
int baz(foo *a) {
  return bar(a);
}
}

namespace test2 {
// FIXME: if we were a bit fancier, we could realize that the 'foo' type is only
// required because of the 'bar' type which is not required at all (or might
// only be required to be declared)
// CHECK-DAG: [ DW_TAG_structure_type ] [foo] [line [[@LINE+1]], {{.*}} [def]
struct foo {
};

struct bar {
  foo f;
};

void func() {
  foo *f;
}
}

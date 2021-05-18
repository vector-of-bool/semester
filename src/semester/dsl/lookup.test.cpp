#include "./lookup.hpp"

// using names = smstr::names<1, 2, 3, 4, 5>;

// static_assert(names::index_of(2) == 1);
// static_assert(names::index_of(45) == -1);

// using scopes = smstr::scope_chain<smstr::names<1, 2, 55, 13, 5>, smstr::names<4, 2, 55>>;
// static_assert(scopes::resolve<5>.did_resolve);
// static_assert(scopes::resolve<5>.nth_scope == 0);
// static_assert(scopes::resolve<5>.name_index == 4);

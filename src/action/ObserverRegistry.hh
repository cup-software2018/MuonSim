#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Where observers announce themselves, so that adding one is adding a file.
//
// One registry per family, from one template: ObserverRegistry<SteppingObserver>,
// <TrackingObserver>, <StackingObserver>. Each instantiation keeps its own table, so a
// name is only unique within its family and no cast is needed to get an observer
// back out.
//
// An observer registers from a static initialiser in its own .cc:
//
//   namespace {
//   const bool registered = ObserverRegistry<SteppingObserver>::Add(
//       "rockgamma", [] { return std::make_shared<RockGammaSteppingObserver>(); });
//   }
//
// main then asks for CreateAll() and never names any of them; the macro picks with
// /observer/<family>/enable. Everything comes back disabled -- see Observer.
//
// THE ONE HAZARD, and it is silent: a static initialiser only runs if its object
// file is linked, and a linker drops an object file from a STATIC LIBRARY when
// nothing references it. Observers here live in src/user, which CMake compiles
// straight into the executable, so every one of them is linked and registers. Move
// them into a static library and they vanish from /observer/list with no error --
// then the archive needs --whole-archive, or the registration needs a symbol that
// main refers to.
//
// So: if an observer you wrote is not in /observer/list, look at how it is linked
// before you look at the code.
template <class T>
class ObserverRegistry {
public:
  using Factory = std::function<std::shared_ptr<T>()>;

  // Returns true when the name was free, which is what makes it usable as the
  // initialiser of a namespace-scope constant. A repeated name is refused and
  // reported: two observers answering to one name would make enable ambiguous.
  static bool Add(const std::string & name, Factory factory)
  {
    if (name.empty() || !factory) return false;

    for (const auto & entry : Table())
      if (entry.first == name) {
        // std::cerr, not G4cout: this runs before main, where G4cout's stream may
        // not be set up yet.
        std::cerr << "ObserverRegistry: '" << name << "' is registered twice in one family"
                  << " -- the second is ignored" << std::endl;
        return false;
      }

    Table().emplace_back(name, std::move(factory));
    return true;
  }

  // One of each, in registration order, all disabled.
  static std::vector<std::shared_ptr<T>> CreateAll()
  {
    std::vector<std::shared_ptr<T>> observers;
    observers.reserve(Table().size());

    for (const auto & entry : Table()) {
      auto observer = entry.second();
      if (!observer) {
        std::cerr << "ObserverRegistry: the factory for '" << entry.first << "' returned nothing"
                  << std::endl;
        continue;
      }
      if (observer->GetName() != entry.first)
        std::cerr << "ObserverRegistry: registered as '" << entry.first << "' but calls itself '"
                  << observer->GetName() << "'" << std::endl;
      observers.push_back(std::move(observer));
    }
    return observers;
  }

private:
  // A function-local static, not a file-scope one: Add() is called from static
  // initialisers in other translation units and the order of those is not defined.
  // Built on first use, the table cannot be used before it exists.
  static std::vector<std::pair<std::string, Factory>> & Table()
  {
    static std::vector<std::pair<std::string, Factory>> table;
    return table;
  }
};

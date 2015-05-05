// Boost nil_generator.hpp header file  ----------------------------------------------//

// Copyright 2010 Andy Tompkins.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.lslboost.org/LICENSE_1_0.txt)

#ifndef BOOST_UUID_NIL_GENERATOR_HPP
#define BOOST_UUID_NIL_GENERATOR_HPP

#include <lslboost/uuid/uuid.hpp>

namespace lslboost {
namespace uuids {

// generate a nil uuid
struct nil_generator {
    typedef uuid result_type;
    
    uuid operator()() const {
        // initialize to all zeros
        uuid u = {{0}};
        return u;
    }
};

inline uuid nil_uuid() {
    return nil_generator()();
}

}} // namespace lslboost::uuids

#endif // BOOST_UUID_NIL_GENERATOR_HPP


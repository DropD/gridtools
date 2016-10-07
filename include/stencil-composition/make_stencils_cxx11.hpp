/*
  GridTools Libraries

  Copyright (c) 2016, GridTools Consortium
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  For information: http://eth-cscs.github.io/gridtools/
*/
#pragma once

#include "common/generic_metafunctions/variadic_to_vector.hpp"
#include "mss_metafunctions.hpp"
#include "mss.hpp"
#include "conditionals/if_.hpp"
#include "conditionals/case_.hpp"
#include "conditionals/switch_.hpp"

namespace gridtools {

    template < typename ExecutionEngine, typename... MssParameters >
    mss_descriptor< ExecutionEngine,
        typename extract_mss_esfs< typename variadic_to_vector< MssParameters... >::type >::type,
        typename extract_mss_caches< typename variadic_to_vector< MssParameters... >::type >::type >
    make_multistage(ExecutionEngine && /**/, MssParameters...) {

        GRIDTOOLS_STATIC_ASSERT((is_execution_engine< ExecutionEngine >::value),
            "The first argument passed to make_mss must be the execution engine (e.g. execute<forward>(), "
            "execute<backward>(), execute<parallel>()");

        return mss_descriptor< ExecutionEngine,
            typename extract_mss_esfs< typename variadic_to_vector< MssParameters... >::type >::type,
            typename extract_mss_caches< typename variadic_to_vector< MssParameters... >::type >::type >();
    }

    template < typename... EsfDescr >
    independent_esf< boost::mpl::vector< EsfDescr... > > make_independent(EsfDescr &&...) {
        return independent_esf< boost::mpl::vector< EsfDescr... > >();
    }

} // namespace gridtools
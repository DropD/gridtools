/*
   Copyright 2016 GridTools Consortium

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#pragma once

#include "../esf.hpp"
#include "../make_esf.hpp"
#ifdef CXX11_ENABLE
#include "make_reduction_cxx11.hpp"
#else
#include "make_reduction_cxx03.hpp"
#endif

#include "../make_computation.hpp"
#include "../axis.hpp"
#include "../../common/binops.hpp"

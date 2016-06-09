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
#include "gtest/gtest.h"
#include "copy_stencil_single_storage.hpp"
#include "Options.hpp"

int main(int argc, char** argv)
{
    // Pass command line arguments to googltest
    ::testing::InitGoogleTest(&argc, argv);

    if (argc != 4) {
        std::cout << "Usage: copy_stencil_single_storage<whatever> dimx dimy dimz\n where args are integer sizes of the data fields" << std::endl;
        return 1;
    }

    for(int i=0; i!=3; ++i) {
        Options::getInstance().m_size[i] = atoi(argv[i+1]);
    }

    return RUN_ALL_TESTS();
}

TEST(CopyStencil, SingleStorageTest)
{
    uint_t x = Options::getInstance().m_size[0];
    uint_t y = Options::getInstance().m_size[1];
    uint_t z = Options::getInstance().m_size[2];

    ASSERT_TRUE(copy_stencil::test(x, y, z));
}

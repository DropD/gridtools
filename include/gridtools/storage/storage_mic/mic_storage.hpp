/*
  GridTools Libraries

  Copyright (c) 2017, ETH Zurich and MeteoSwiss
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

#include <atomic>
#include <utility>

#include "../../common/gt_assert.hpp"
#include "../common/state_machine.hpp"
#include "../common/storage_interface.hpp"

namespace gridtools {

    /*
     * @brief The Mic storage implementation. This class owns the pointer
     * to the data. Additionally there is a field that contains information about
     * the ownership. Instances of this class are noncopyable.
     * @tparam DataType the type of the data and the pointer respectively (e.g., float or double)
     *
     * Here we are using the CRTP. Actually the same
     * functionality could be implemented using standard inheritance
     * but we prefer the CRTP because it can be seen as the standard
     * gridtools pattern and we clearly want to avoid virtual
     * methods, etc.
     */
    template < typename DataType >
    struct mic_storage : storage_interface< mic_storage< DataType > > {
        typedef DataType data_t;
        typedef data_t *ptrs_t;
        typedef state_machine state_machine_t;

      private:
        data_t *m_cpu_ptr;
        ownership m_ownership = ownership::Full;
        uint_t m_data_offset;

      public:
        mic_storage(mic_storage const &) = delete;
        mic_storage &operator=(mic_storage const &) = delete;
        /*
         * @brief mic_storage constructor. Allocates data aligned to 2MB pages (to encourage the system to use
         * transparent huge pages) and adds an additional samll offset which changes for every allocation to reduce the
         * risk of L1 cache set conflicts.
         * @param size defines the size of the storage and the allocated space.
         */
        mic_storage(uint_t size) {
            // get data_offset and update it atomically for next allocation
            static std::atomic< uint_t > s_data_offset(64);
            uint_t data_offset = s_data_offset.load(std::memory_order_relaxed);
            uint_t next_data_offset;
            do {
                m_data_offset = data_offset / sizeof(data_t);
                next_data_offset = 2 * data_offset;
                if (next_data_offset > 8192)
                    next_data_offset = 64;
            } while (!s_data_offset.compare_exchange_weak(data_offset, next_data_offset, std::memory_order_relaxed));

            // allocate memory aligned to 2MB
            if (posix_memalign(
                    reinterpret_cast< void ** >(&m_cpu_ptr), 2 * 1024 * 1024, (size + m_data_offset) * sizeof(data_t)))
                throw std::bad_alloc();

            m_cpu_ptr += m_data_offset;
        }

        /*
         * @brief mic_storage constructor. Does not allocate memory but uses an external pointer.
         * Reason for having this is to support externally allocated memory (e.g., from Fortran or Python).
         * @param size defines the size of the storage and the allocated space.
         * @param external_ptr a pointer to the external data
         * @param own ownership information (in this case only externalCPU is valid)
         */
        explicit constexpr mic_storage(uint_t size, data_t *external_ptr, ownership own = ownership::ExternalCPU)
            : m_cpu_ptr(external_ptr),
              m_ownership(error_or_return(
                  (own == ownership::ExternalCPU), own, "ownership type must be ExternalCPU when using mic_storage")) {}

        /*
         * @brief mic_storage constructor. Allocate memory on Mic and initialize the memory according to the given
         * initializer.
         * @param size defines the size of the storage and the allocated space.
         * @param initializer initialization value
         */
        mic_storage(uint_t size, data_t initializer) : mic_storage(size) {
            for (uint_t i = 0; i < size; ++i) {
                m_cpu_ptr[i] = initializer;
            }
        }

        /*
         * @brief mic_storage destructor.
         */
        ~mic_storage() {
            if (m_ownership == ownership::Full && m_cpu_ptr) {
                free(m_cpu_ptr - m_data_offset);
            }
        }

        /*
         * @brief swap implementation for mic_storage
         */
        void swap_impl(mic_storage &other) {
            using std::swap;
            swap(m_cpu_ptr, other.m_cpu_ptr);
            swap(m_ownership, other.m_ownership);
            swap(m_data_offset, other.m_data_offset);
        }

        /*
         * @brief retrieve the mic data pointer.
         * @return data pointer
         */
        data_t *get_cpu_ptr() const {
            ASSERT_OR_THROW(m_cpu_ptr, "This storage has never been initialized");
            return m_cpu_ptr;
        }

        /*
         * @brief get_ptrs implementation for mic_storage.
         */
        ptrs_t get_ptrs_impl() const { return m_cpu_ptr; }

        /*
         * @brief valid implementation for mic_storage.
         */
        bool valid_impl() const { return m_cpu_ptr; }

        /*
         * @brief clone_to_device implementation for mic_storage.
         */
        void clone_to_device_impl(){};

        /*
         * @brief clone_from_device implementation for mic_storage.
         */
        void clone_from_device_impl(){};

        /*
         * @brief synchronization implementation for mic_storage.
         */
        void sync_impl(){};

        /*
         * @brief device_needs_update implementation for mic_storage.
         */
        bool device_needs_update_impl() const { return false; }

        /*
         * @brief host_needs_update implementation for mic_storage.
         */
        bool host_needs_update_impl() const { return false; }

        /*
         * @brief reactivate_device_write_views implementation for mic_storage.
         */
        void reactivate_device_write_views_impl() {}

        /*
         * @brief reactivate_host_write_views implementation for mic_storage.
         */
        void reactivate_host_write_views_impl() {}

        /*
         * @brief get_state_machine_ptr implementation for mic_storage.
         */
        state_machine *get_state_machine_ptr_impl() { return nullptr; }
    };

    // simple metafunction to check if a type is a mic storage
    template < typename T >
    struct is_mic_storage : boost::mpl::false_ {};

    template < typename T >
    struct is_mic_storage< mic_storage< T > > : boost::mpl::true_ {};
}
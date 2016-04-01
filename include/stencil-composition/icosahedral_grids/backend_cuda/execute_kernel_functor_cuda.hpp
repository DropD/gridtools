#pragma once
#include "../../iteration_policy.hpp"
#include "../../backend_traits_fwd.hpp"
#include "stencil-composition/iterate_domain.hpp"
#include "../../backend_cuda/shared_iterate_domain.hpp"
#include "common/gt_assert.hpp"

namespace gridtools {

    namespace _impl_iccuda {

        template < int VBoundary >
        struct padded_boundary
            : boost::mpl::integral_c< int, VBoundary <= 1 ? 1 : (VBoundary <= 2 ? 2 : (VBoundary <= 4 ? 4 : 8)) > {
            BOOST_STATIC_ASSERT(VBoundary >= 0 && VBoundary <= 8);
        };

        template < typename RunFunctorArguments, typename LocalDomain >
        __global__ void do_it_on_gpu(LocalDomain const *RESTRICT l_domain,
            typename RunFunctorArguments::grid_t const *grid,
            const int starti,
            const int startj,
            const uint_t nx,
            const uint_t ny) {

            assert(l_domain);
            typedef typename RunFunctorArguments::iterate_domain_t iterate_domain_t;
            typedef typename RunFunctorArguments::execution_type_t execution_type_t;

            typedef typename RunFunctorArguments::physical_domain_block_size_t block_size_t;
            typedef typename RunFunctorArguments::extent_sizes_t extent_sizes_t;

            typedef typename RunFunctorArguments::max_extent_t max_extent_t;
            typedef typename RunFunctorArguments::iterate_domain_t iterate_domain_t;
            typedef typename RunFunctorArguments::async_esf_map_t async_esf_map_t;

            typedef backend_traits_from_id< enumtype::Cuda > backend_traits_t;
            typedef typename iterate_domain_t::strides_cached_t strides_t;
            typedef typename iterate_domain_t::data_pointer_array_t data_pointer_array_t;
            typedef shared_iterate_domain< data_pointer_array_t,
                strides_t,
                max_extent_t,
                typename iterate_domain_t::iterate_domain_cache_t::ij_caches_tuple_t > shared_iterate_domain_t;

            const uint_t block_size_i = (blockIdx.x + 1) * block_size_t::i_size_t::value < nx
                                            ? block_size_t::i_size_t::value
                                            : nx - blockIdx.x * block_size_t::i_size_t::value;
            const uint_t block_size_j = (blockIdx.y + 1) * block_size_t::j_size_t::value < ny
                                            ? block_size_t::j_size_t::value
                                            : ny - blockIdx.y * block_size_t::j_size_t::value;

            __shared__ shared_iterate_domain_t shared_iterate_domain;

            // Doing construction of the ierate domain and assignment of pointers and strides
            iterate_domain_t it_domain(*l_domain, grid->grid_topology(), block_size_i, block_size_j);

            it_domain.set_shared_iterate_domain_pointer_impl(&shared_iterate_domain);

            it_domain.template assign_storage_pointers< backend_traits_t >();
            it_domain.template assign_stride_pointers< backend_traits_t, strides_t >();

            __syncthreads();

            // computing the global position in the physical domain
            /*
             *  In a typical cuda block we have the following regions
             *
             *    aa bbbbbbbb cc
             *    aa bbbbbbbb cc
             *
             *    hh dddddddd ii
             *    hh dddddddd ii
             *    hh dddddddd ii
             *    hh dddddddd ii
             *
             *    ee ffffffff gg
             *    ee ffffffff gg
             *
             * Regions b,d,f have warp (or multiple of warp size)
             * Size of regions a, c, h, i, e, g are determined by max_extent_t
             * Regions b,d,f are easily executed by dedicated warps (one warp for each line).
             * Regions (a,h,e) and (c,i,g) are executed by two specialized warp
             *
             */
            // jboundary_limit determines the number of warps required to execute (b,d,f)
            // TODO FUSING
            //        const int jboundary_limit = block_size_t::j_size_t::value - max_extent_t::jminus::value
            //            + max_extent_t::jplus::value;
            //        //iminus_limit adds to jboundary_limit an additional warp for regions (a,h,e)
            //        const int iminus_limit = jboundary_limit + (max_extent_t::iminus::value<0 ? 1 : 0);
            //        //iminus_limit adds to iminus_limit an additional warp for regions (c,i,g)
            //        const int iplus_limit = iminus_limit + (max_extent_t::iplus::value>0 ? 1 : 0);
            const int jboundary_limit = block_size_t::j_size_t::value;
            // iminus_limit adds to jboundary_limit an additional warp for regions (a,h,e)
            const int iminus_limit = jboundary_limit;
            // iminus_limit adds to iminus_limit an additional warp for regions (c,i,g)
            const int iplus_limit = iminus_limit;

            // The kernel allocate enough warps to execute all halos of all ESFs.
            // The max_extent_t is the enclosing extent of all the ESFs
            // (i,j) is the position (in the global domain, minus initial halos which are accounted with istart, jstart
            // args)
            // of this thread within the physical block
            // (iblock, jblock) are relative positions of the thread within the block. Grid positions in the halos of
            // the block
            //   get negative values

            // TODO FUSING
            //            int i = max_extent_t::iminus::value - 1;
            //            int j = max_extent_t::jminus::value - 1;
            //            int iblock = max_extent_t::iminus::value - 1;
            //            int jblock = max_extent_t::jminus::value - 1;
            //        if(threadIdx.y < jboundary_limit)
            //        {
            //            i = blockIdx.x * block_size_t::i_size_t::value + threadIdx.x;
            //            j = blockIdx.y * block_size_t::j_size_t::value + threadIdx.y + max_extent_t::jminus::value;
            //            iblock = threadIdx.x;
            //            jblock = threadIdx.y + max_extent_t::jminus::value;
            //        }
            //        else if(threadIdx.y < iminus_limit)
            //        {
            //            const int padded_boundary_ = padded_boundary<-max_extent_t::iminus::value>::value;
            //            //we dedicate one warp to execute regions (a,h,e), so here we make sure we have enough threads
            //            assert( (block_size_t::j_size_t::value - max_extent_t::jminus::value +
            //            max_extent_t::jplus::value)*padded_boundary_ <= enumtype::vector_width);

            //            i = blockIdx.x * block_size_t::i_size_t::value -padded_boundary_ + threadIdx.x %
            //            padded_boundary_;
            //            j = blockIdx.y* block_size_t::j_size_t::value +  threadIdx.x / padded_boundary_ +
            //            max_extent_t::jminus::value;
            //            iblock = -padded_boundary_ + threadIdx.x % padded_boundary_;
            //            jblock = threadIdx.x / padded_boundary_ + max_extent_t::jminus::value;
            //        }
            //        else if(threadIdx.y < iplus_limit)
            //        {
            //            const int padded_boundary_ = padded_boundary<max_extent_t::iplus::value>::value;
            //            //we dedicate one warp to execute regions (c,i,g), so here we make sure we have enough threads
            //            assert( (block_size_t::j_size_t::value - max_extent_t::jminus::value +
            //            max_extent_t::jplus::value)*padded_boundary_ <= enumtype::vector_width);

            //            i = blockIdx.x * block_size_t::i_size_t::value + threadIdx.x % padded_boundary_ +
            //            block_size_t::i_size_t::value;
            //            j = blockIdx.y* block_size_t::j_size_t::value + threadIdx.x / padded_boundary_ +
            //            max_extent_t::jminus::value;
            //            iblock = threadIdx.x % padded_boundary_ + block_size_t::i_size_t::value;
            //            jblock = threadIdx.x / padded_boundary_ + max_extent_t::jminus::value;
            //        }

            int i = -1;
            int j = -1;
            int iblock = -1;
            int jblock = -1;
            if (threadIdx.y < jboundary_limit) {
                i = blockIdx.x * block_size_t::i_size_t::value + threadIdx.x;
                j = blockIdx.y * block_size_t::j_size_t::value + threadIdx.y;
                iblock = threadIdx.x;
                jblock = threadIdx.y;
            }

            it_domain.set_index(0);

            // initialize the i index
            it_domain.template initialize< grid_traits_from_id<enumtype::icosahedral>::dim_i_t::value >(i + starti, blockIdx.x);
            //initialize to color 0
            it_domain.template initialize< grid_traits_from_id<enumtype::icosahedral>::dim_c_t::value >(0, 0);
            // initialize the j index
            it_domain.template initialize< grid_traits_from_id<enumtype::icosahedral>::dim_j_t::value >(j + startj, blockIdx.y);

            it_domain.set_block_pos(iblock, jblock);

            typedef typename boost::mpl::front< typename RunFunctorArguments::loop_intervals_t >::type interval;
            typedef typename index_to_level< typename interval::first >::type from;
            typedef typename index_to_level< typename interval::second >::type to;
            typedef _impl::iteration_policy< from,
                to,
                typename grid_traits_from_id< enumtype::icosahedral >::dim_k_t,
                execution_type_t::type::iteration > iteration_policy_t;

            it_domain.template initialize< grid_traits_from_id< enumtype::icosahedral >::dim_k_t::value >(grid->template value_at< iteration_policy_t::from >());

            // execute the k interval functors
            boost::mpl::for_each< typename RunFunctorArguments::loop_intervals_t >(
                _impl::run_f_on_interval< execution_type_t, RunFunctorArguments >(it_domain, *grid));
        }
    } // namespace _impl_iccuda

    namespace icgrid {

        /**
         * @brief main functor that setups the CUDA kernel for a MSS and launchs it
         * @tparam RunFunctorArguments run functor argument type with the main configuration of the MSS
         */
        template < typename RunFunctorArguments >
        struct execute_kernel_functor_cuda {
            GRIDTOOLS_STATIC_ASSERT(
                (is_run_functor_arguments< RunFunctorArguments >::value), "Internal Error: wrong type");
            typedef typename RunFunctorArguments::local_domain_t local_domain_t;
            typedef typename RunFunctorArguments::grid_t grid_t;

            // ctor
            explicit execute_kernel_functor_cuda(const local_domain_t &local_domain,
                const grid_t &grid,
                const uint_t block_idx_i,
                const uint_t block_idx_j)
                : m_local_domain(local_domain), m_grid(grid), m_block_idx_i(block_idx_i), m_block_idx_j(block_idx_j) {}

            void operator()() {
#ifdef VERBOSE
                short_t count;
                cudaGetDeviceCount(&count);

                if (count) {
                    cudaDeviceProp prop;
                    cudaGetDeviceProperties(&prop, 0);
                    std::cout << "total global memory " << prop.totalGlobalMem << std::endl;
                    std::cout << "shared memory per block " << prop.sharedMemPerBlock << std::endl;
                    std::cout << "registers per block " << prop.regsPerBlock << std::endl;
                    std::cout << "maximum threads per block " << prop.maxThreadsPerBlock << std::endl;
                    std::cout << "maximum threads dimension " << prop.maxThreadsDim << std::endl;
                    std::cout << "clock rate " << prop.clockRate << std::endl;
                    std::cout << "total const memory " << prop.totalConstMem << std::endl;
                    std::cout << "compute capability " << prop.major << "." << prop.minor << std::endl;
                    std::cout << "multiprocessors count " << prop.multiProcessorCount << std::endl;
                    std::cout << "CUDA compute mode (0=default, 1=exclusive, 2=prohibited, 3=exclusive process) "
                              << prop.computeMode << std::endl;
                    std::cout << "concurrent kernels " << prop.concurrentKernels << std::endl;
                    std::cout << "Number of asynchronous engines  " << prop.asyncEngineCount << std::endl;
                    std::cout << "unified addressing " << prop.unifiedAddressing << std::endl;
                    std::cout << "memoryClockRate " << prop.memoryClockRate << std::endl;
                    std::cout << "memoryBusWidth " << prop.memoryBusWidth << std::endl;
                    std::cout << "l2CacheSize " << prop.l2CacheSize << std::endl;
                    std::cout << "maxThreadsPerMultiProcessor " << prop.maxThreadsPerMultiProcessor << std::endl;
                }
#endif

                local_domain_t *local_domain_gp = m_local_domain.gpu_object_ptr;

                grid_t const *grid_gp = m_grid.gpu_object_ptr;

                // number of threads
                const uint_t nx = (uint_t)(m_grid.i_high_bound() - m_grid.i_low_bound() + 1);
                const uint_t ny = (uint_t)(m_grid.j_high_bound() - m_grid.j_low_bound() + 1);

                typedef typename RunFunctorArguments::physical_domain_block_size_t block_size_t;

                // compute the union (or enclosing) extent for the extents of all ESFs.
                // This maximum extent of all ESF will determine the size of the CUDA block:
                // *  If there are redundant computations to be executed at the IMinus or IPlus halos,
                //    each CUDA thread will execute two grid points (one at the core of the block and
                //    another within one of the halo regions)
                // *  Otherwise each CUDA thread executes only one grid point.
                // Based on the previous we compute the size of the CUDA block required.
                typedef typename boost::mpl::fold< typename RunFunctorArguments::extent_sizes_t,
                    extent< 0 >,
                    enclosing_extent< boost::mpl::_1, boost::mpl::_2 > >::type maximum_extent_t;

                //        typedef block_size<
                //            block_size_t::i_size_t::value,
                //            (block_size_t::j_size_t::value - maximum_extent_t::jminus::value +
                //            maximum_extent_t::jplus::value +
                //                    (maximum_extent_t::iminus::value != 0 ? 1 : 0) + (maximum_extent_t::iplus::value
                //                    != 0 ? 1 : 0)
                //            )
                //        > cuda_block_size_t;

                // TODO FUSING used max extent when fusing esfs
                typedef block_size< block_size_t::i_size_t::value, (block_size_t::j_size_t::value) > cuda_block_size_t;

                // number of grid points that a cuda block covers
                const uint_t ntx = block_size_t::i_size_t::value;
                const uint_t nty = block_size_t::j_size_t::value;
                const uint_t ntz = 1;
                dim3 threads(cuda_block_size_t::i_size_t::value, cuda_block_size_t::j_size_t::value, ntz);

                // number of blocks required
                const uint_t nbx = (nx + ntx - 1) / ntx;
                const uint_t nby = (ny + nty - 1) / nty;
                const uint_t nbz = 1;

                dim3 blocks(nbx, nby, nbz);

                // re-create the run functor arguments, replacing the processing elements block size
                // with the corresponding, recently computed, block size
                typedef run_functor_arguments< typename RunFunctorArguments::backend_ids_t,
                    cuda_block_size_t,
                    typename RunFunctorArguments::physical_domain_block_size_t,
                    typename RunFunctorArguments::functor_list_t,
                    typename RunFunctorArguments::esf_sequence_t,
                    typename RunFunctorArguments::esf_args_map_sequence_t,
                    typename RunFunctorArguments::loop_intervals_t,
                    typename RunFunctorArguments::functors_map_t,
                    typename RunFunctorArguments::extent_sizes_t,
                    typename RunFunctorArguments::local_domain_t,
                    typename RunFunctorArguments::cache_sequence_t,
                    typename RunFunctorArguments::async_esf_map_t,
                    typename RunFunctorArguments::grid_t,
                    typename RunFunctorArguments::execution_type_t > run_functor_arguments_cuda_t;

#ifdef VERBOSE
                printf("ntx = %d, nty = %d, ntz = %d\n", ntx, nty, ntz);
                printf("nbx = %d, nby = %d, nbz = %d\n", nbx, nby, nbz);
                printf("nx = %d, ny = %d, nz = 1\n", nx, ny);
#endif

                _impl_iccuda::do_it_on_gpu< run_functor_arguments_cuda_t,
                    local_domain_t ><<< blocks, threads >>> //<<<nbx*nby, ntx*nty>>>
                    (local_domain_gp, grid_gp, m_grid.i_low_bound(), m_grid.j_low_bound(), (nx), (ny));

                // TODOCOSUNA we do not need this. It will block the host, and we want to continue doing other stuff
                cudaDeviceSynchronize();
            }

          private:
            const local_domain_t &m_local_domain;
            const grid_t &m_grid;
            const uint_t m_block_idx_i;
            const uint_t m_block_idx_j;
        };

    } // namespace icgrid

} // namespace gridtools

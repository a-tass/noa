// Copyright (c) 2004-2022 Tomáš Oberhuber et al.
//
// This file is part of TNL - Template Numerical Library (https://tnl-project.org/)
//
// SPDX-License-Identifier: MIT

#pragma once

#include <noa/3rdparty/TNL/Assert.h>
#include <noa/3rdparty/TNL/Cuda/LaunchHelpers.h>
#include <noa/3rdparty/TNL/Containers/VectorView.h>
#include <noa/3rdparty/TNL/Algorithms/ParallelFor.h>
#include <noa/3rdparty/TNL/Algorithms/Segments/detail/LambdaAdapter.h>
#include <noa/3rdparty/TNL/Algorithms/Segments/Kernels/CSRLightKernel.h>

namespace noa::TNL {
   namespace Algorithms {
      namespace Segments {

#ifdef HAVE_CUDA
template< typename Real,
          typename Index,
          typename OffsetsView,
          typename Fetch,
          typename Reduce,
          typename Keep >
__global__
void SpMVCSRLight2( OffsetsView offsets,
                                 const Index first,
                                 const Index last,
                                 Fetch fetch,
                                 Reduce reduce,
                                 Keep keep,
                                 const Real zero,
                                 const Index gridID)
{
   const Index segmentIdx =
      first + ( ( gridID * noa::TNL::Cuda::getMaxGridXSize() ) + (blockIdx.x * blockDim.x) + threadIdx.x ) / 2;
   if( segmentIdx >= last )
      return;

   const Index inGroupID = threadIdx.x & 1; // & is cheaper than %
   const Index maxID = offsets[ segmentIdx  + 1];

   Real result = zero;
   bool compute = true;
   for( Index i = offsets[segmentIdx] + inGroupID; i < maxID; i += 2)
      result = reduce( result, fetch( i, compute ) );

   /* Parallel reduction */
   result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result, 1 ) );

   /* Write result */
   if( inGroupID == 0 )
      keep( segmentIdx, result );
}

template< typename Real,
          typename Index,
          typename OffsetsView,
          typename Fetch,
          typename Reduce,
          typename Keep >
__global__
void SpMVCSRLight4( OffsetsView offsets,
                                 const Index first,
                                 const Index last,
                                 Fetch fetch,
                                 Reduce reduce,
                                 Keep keep,
                                 const Real zero,
                                 const Index gridID )
{
   const Index segmentIdx =
      first + ((gridID * noa::TNL::Cuda::getMaxGridXSize() ) + (blockIdx.x * blockDim.x) + threadIdx.x) / 4;
   if (segmentIdx >= last)
      return;

   const Index inGroupID = threadIdx.x & 3; // & is cheaper than %
   const Index maxID = offsets[segmentIdx + 1];

   Real result = zero;
   bool compute = true;
   for (Index i = offsets[segmentIdx] + inGroupID; i < maxID; i += 4)
      result = reduce( result, fetch( i, compute ) );

   /* Parallel reduction */
   result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result, 2 ) );
   result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result, 1 ) );

   /* Write result */
   if( inGroupID == 0 )
      keep( segmentIdx, result );

}

template< typename Real,
          typename Index,
          typename OffsetsView,
          typename Fetch,
          typename Reduce,
          typename Keep >
__global__
void SpMVCSRLight8( OffsetsView offsets,
                                 const Index first,
                                 const Index last,
                                 Fetch fetch,
                                 Reduce reduce,
                                 Keep keep,
                                 const Real zero,
                                 const Index gridID)
{
   const Index segmentIdx =
      first + ((gridID * noa::TNL::Cuda::getMaxGridXSize() ) + (blockIdx.x * blockDim.x) + threadIdx.x) / 8;
   if (segmentIdx >= last)
      return;

   Index i;
   const Index inGroupID = threadIdx.x & 7; // & is cheaper than %
   const Index maxID = offsets[segmentIdx + 1];

   Real result = zero;
   bool compute = true;
   for (i = offsets[segmentIdx] + inGroupID; i < maxID; i += 8)
      result = reduce( result, fetch( i, compute ) );

   /* Parallel reduction */
   result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result, 4 ) );
   result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result, 2 ) );
   result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result, 1 ) );

   /* Write result */
   if( inGroupID == 0 )
      keep( segmentIdx, result );
}

template< typename Real,
          typename Index,
          typename OffsetsView,
          typename Fetch,
          typename Reduce,
          typename Keep >
__global__
void SpMVCSRLight16( OffsetsView offsets,
                                  const Index first,
                                  const Index last,
                                  Fetch fetch,
                                  Reduce reduce,
                                  Keep keep,
                                  const Real zero,
                                  const Index gridID )
{
   const Index segmentIdx =
      first + ((gridID * noa::TNL::Cuda::getMaxGridXSize() ) + (blockIdx.x * blockDim.x) + threadIdx.x ) / 16;
   if( segmentIdx >= last )
      return;

   Index i;
   const Index inGroupID = threadIdx.x & 15; // & is cheaper than %
   const Index maxID = offsets[segmentIdx + 1];

   Real result = zero;
   bool compute = true;
   for( i = offsets[segmentIdx] + inGroupID; i < maxID; i += 16 )
      result = reduce( result, fetch( i, compute ) );

   /* Parallel reduction */
   result = reduce( result, __shfl_down_sync( 0xFFFFFFFF, result, 8 ) );
   result = reduce( result, __shfl_down_sync( 0xFFFFFFFF, result, 4 ) );
   result = reduce( result, __shfl_down_sync( 0xFFFFFFFF, result, 2 ) );
   result = reduce( result, __shfl_down_sync( 0xFFFFFFFF, result, 1 ) );

   /* Write result */
   if( inGroupID == 0 )
      keep( segmentIdx, result );
}

/*template< typename Real,
          typename Index,
          typename OffsetsView,
          typename Fetch,
          typename Reduce,
          typename Keep >
__global__
void SpMVCSRVector( OffsetsView offsets,
                    const Index first,
                    const Index last,
                    Fetch fetch,
                    Reduce reduce,
                    Keep keep,
                    const Real zero,
                    const Index gridID )
{
   const int warpSize = 32;
   const Index warpID = first + ((gridID * noa::TNL::Cuda::getMaxGridXSize() ) + (blockIdx.x * blockDim.x) + threadIdx.x) / warpSize;
   if (warpID >= last)
      return;

   Real result = zero;
   const Index laneID = threadIdx.x & 31; // & is cheaper than %
   Index endID = offsets[warpID + 1];

   // Calculate result
   bool compute = true;
   for (Index i = offsets[warpID] + laneID; i < endID; i += warpSize)
      result = reduce( result, fetch( i, compute ) );

   // Reduction
   result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result, 16 ) );
   result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  8 ) );
   result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  4 ) );
   result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  2 ) );
   result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  1 ) );
   // Write result
   if( laneID == 0 )
      keep( warpID, result );
}*/

template< int ThreadsPerSegment,
          typename Real,
          typename Index,
          typename OffsetsView,
          typename Fetch,
          typename Reduce,
          typename Keep >
__global__
void SpMVCSRVector( OffsetsView offsets,
                    const Index first,
                    const Index last,
                    Fetch fetch,
                    Reduce reduce,
                    Keep keep,
                    const Real zero,
                    const Index gridID )
{
   //const int warpSize = 32;
   const Index warpID = first + ((gridID * noa::TNL::Cuda::getMaxGridXSize() ) + (blockIdx.x * blockDim.x) + threadIdx.x) / ThreadsPerSegment;
   if (warpID >= last)
      return;

   Real result = zero;
   const Index laneID = threadIdx.x & ( ThreadsPerSegment - 1 ); // & is cheaper than %
   Index endID = offsets[warpID + 1];

   // Calculate result
   bool compute = true;
   for (Index i = offsets[warpID] + laneID; i < endID; i += ThreadsPerSegment )
      result = reduce( result, fetch( i, compute ) );

   // Reduction
   if( ThreadsPerSegment > 16 )
   {
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result, 16 ) );
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  8 ) );
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  4 ) );
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  2 ) );
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  1 ) );
   } else if( ThreadsPerSegment > 8 ) {
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  8 ) );
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  4 ) );
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  2 ) );
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  1 ) );
   } else if( ThreadsPerSegment > 4 ) {
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  4 ) );
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  2 ) );
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  1 ) );
   } else if( ThreadsPerSegment > 2 ) {
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  2 ) );
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  1 ) );
   } else if( ThreadsPerSegment > 1 )
      result = reduce( result, __shfl_down_sync(0xFFFFFFFF, result,  1 ) );

   // Store result
   if( laneID == 0 )
      keep( warpID, result );
}


template< int BlockSize,
          int ThreadsPerSegment,
          typename Offsets,
          typename Index,
          typename Fetch,
          typename Reduction,
          typename ResultKeeper,
          typename Real >
__global__
void reduceSegmentsCSRLightMultivectorKernel(
    int gridIdx,
    const Offsets offsets,
    Index first,
    Index last,
    Fetch fetch,
    const Reduction reduce,
    ResultKeeper keep,
    const Real zero )
{
    const Index segmentIdx =  noa::TNL::Cuda::getGlobalThreadIdx( gridIdx ) / ThreadsPerSegment + first;
    if( segmentIdx >= last )
        return;

    __shared__ Real shared[ BlockSize / 32 ];
    if( threadIdx.x < BlockSize / noa::TNL::Cuda::getWarpSize() )
        shared[ threadIdx.x ] = zero;

    const int laneIdx = threadIdx.x & ( ThreadsPerSegment - 1 ); // & is cheaper than %
    const int inWarpLaneIdx = threadIdx.x & ( noa::TNL::Cuda::getWarpSize() - 1 ); // & is cheaper than %
    const Index beginIdx = offsets[ segmentIdx ];
    const Index endIdx   = offsets[ segmentIdx + 1 ] ;

    Real result = zero;
    bool compute( true );
    Index localIdx = laneIdx;
    for( Index globalIdx = beginIdx + laneIdx; globalIdx < endIdx && compute; globalIdx += ThreadsPerSegment )
    {
       result = reduce( result, detail::FetchLambdaAdapter< Index, Fetch >::call( fetch, segmentIdx, localIdx, globalIdx, compute ) );
       localIdx += ThreadsPerSegment;
    }
    result += __shfl_down_sync(0xFFFFFFFF, result, 16);
    result += __shfl_down_sync(0xFFFFFFFF, result, 8);
    result += __shfl_down_sync(0xFFFFFFFF, result, 4);
    result += __shfl_down_sync(0xFFFFFFFF, result, 2);
    result += __shfl_down_sync(0xFFFFFFFF, result, 1);

    const Index warpIdx = threadIdx.x / noa::TNL::Cuda::getWarpSize();
    if( inWarpLaneIdx == 0 )
        shared[ warpIdx ] = result;

    __syncthreads();
    // Reduction in shared
    if( warpIdx == 0 && inWarpLaneIdx < 16 )
    {
        //constexpr int totalWarps = BlockSize / WarpSize;
        constexpr int warpsPerSegment = ThreadsPerSegment / noa::TNL::Cuda::getWarpSize();
        if( warpsPerSegment >= 32 )
        {
            shared[ inWarpLaneIdx ] =  reduce( shared[ inWarpLaneIdx ], shared[ inWarpLaneIdx + 16 ] );
            __syncwarp();
        }
        if( warpsPerSegment >= 16 )
        {
            shared[ inWarpLaneIdx ] =  reduce( shared[ inWarpLaneIdx ], shared[ inWarpLaneIdx +  8 ] );
            __syncwarp();
        }
        if( warpsPerSegment >= 8 )
        {
            shared[ inWarpLaneIdx ] =  reduce( shared[ inWarpLaneIdx ], shared[ inWarpLaneIdx +  4 ] );
            __syncwarp();
        }
        if( warpsPerSegment >= 4 )
        {
            shared[ inWarpLaneIdx ] =  reduce( shared[ inWarpLaneIdx ], shared[ inWarpLaneIdx +  2 ] );
            __syncwarp();
        }
        if( warpsPerSegment >= 2 )
        {
            shared[ inWarpLaneIdx ] =  reduce( shared[ inWarpLaneIdx ], shared[ inWarpLaneIdx +  1 ] );
            __syncwarp();
        }
        constexpr int segmentsCount = BlockSize / ThreadsPerSegment;
        if( inWarpLaneIdx < segmentsCount && segmentIdx + inWarpLaneIdx < last )
        {
            //printf( "Long: segmentIdx %d -> %d \n", segmentIdx, aux );
            keep( segmentIdx + inWarpLaneIdx, shared[ inWarpLaneIdx * ThreadsPerSegment / 32 ] );
        }
    }
}

#endif
template< typename Index,
          typename Device,
          typename Fetch,
          typename Reduce,
          typename Keep,
          bool DispatchScalarCSR =
            detail::CheckFetchLambda< Index, Fetch >::hasAllParameters() ||
            std::is_same< Device, Devices::Host >::value >
struct CSRLightKernelreduceSegmentsDispatcher;

template< typename Index,
          typename Device,
          typename Fetch,
          typename Reduction,
          typename ResultKeeper >
struct CSRLightKernelreduceSegmentsDispatcher< Index, Device, Fetch, Reduction, ResultKeeper, true >
{

   template< typename Offsets,
             typename Real >
   static void reduce( const Offsets& offsets,
                       Index first,
                       Index last,
                       Fetch& fetch,
                       const Reduction& reduce,
                       ResultKeeper& keep,
                       const Real& zero,
                       const Index threadsPerSegment )
   {
      noa::TNL::Algorithms::Segments::CSRScalarKernel< Index, Device >::
         reduceSegments( offsets, first, last, fetch, reduce, keep, zero );
   }
};

template< typename Index,
          typename Device,
          typename Fetch,
          typename Reduce,
          typename Keep >
struct CSRLightKernelreduceSegmentsDispatcher< Index, Device, Fetch, Reduce, Keep, false >
{
   template< typename OffsetsView,
             typename Real >
   static void reduce( const OffsetsView& offsets,
                       Index first,
                       Index last,
                       Fetch& fetch,
                       const Reduce& reduce,
                       Keep& keep,
                       const Real& zero,
                       const Index threadsPerSegment )
   {
#ifdef HAVE_CUDA
    if( last <= first )
       return;

      const size_t threads = 128;
      Index blocks, groupSize;

      size_t  neededThreads = threadsPerSegment * ( last - first );

      for (Index grid = 0; neededThreads != 0; ++grid)
      {
         if( noa::TNL::Cuda::getMaxGridXSize() * threads >= neededThreads)
         {
            blocks = roundUpDivision(neededThreads, threads);
            neededThreads = 0;
         }
         else
         {
            blocks = noa::TNL::Cuda::getMaxGridXSize();
            neededThreads -= noa::TNL::Cuda::getMaxGridXSize() * threads;
         }

         if( threadsPerSegment == 1 )
            SpMVCSRVector< 1, Real, Index, OffsetsView, Fetch, Reduce, Keep ><<< blocks, threads >>>(
               offsets, first, last, fetch, reduce, keep, zero, grid );
         if( threadsPerSegment == 2 )
            SpMVCSRVector< 2, Real, Index, OffsetsView, Fetch, Reduce, Keep ><<< blocks, threads >>>(
               offsets, first, last, fetch, reduce, keep, zero, grid );
         if( threadsPerSegment == 4 )
            SpMVCSRVector< 4, Real, Index, OffsetsView, Fetch, Reduce, Keep ><<< blocks, threads >>>(
               offsets, first, last, fetch, reduce, keep, zero, grid );
         if( threadsPerSegment == 8 )
            SpMVCSRVector< 8, Real, Index, OffsetsView, Fetch, Reduce, Keep ><<< blocks, threads >>>(
               offsets, first, last, fetch, reduce, keep, zero, grid );
         if( threadsPerSegment == 16 )
            SpMVCSRVector< 16, Real, Index, OffsetsView, Fetch, Reduce, Keep ><<< blocks, threads >>>(
               offsets, first, last, fetch, reduce, keep, zero, grid );
         if( threadsPerSegment == 32 )
            SpMVCSRVector< 32, Real, Index, OffsetsView, Fetch, Reduce, Keep ><<< blocks, threads >>>(
               offsets, first, last, fetch, reduce, keep, zero, grid );
         if( threadsPerSegment == 64 )
         { // Execute CSR MultiVector
            reduceSegmentsCSRLightMultivectorKernel< 128, 64 ><<<blocks, threads>>>(
                     grid, offsets, first, last, fetch, reduce, keep, zero );
         }
         if (threadsPerSegment >= 128 )
         { // Execute CSR MultiVector
            reduceSegmentsCSRLightMultivectorKernel< 128, 128 ><<<blocks, threads>>>(
                     grid, offsets, first, last, fetch, reduce, keep, zero );
         }


         /*if (threadsPerSegment == 2)
            SpMVCSRLight2<Real, Index, OffsetsView, Fetch, Reduce, Keep ><<<blocks, threads>>>(
               offsets, first, last, fetch, reduce, keep, zero, grid );
         else if (threadsPerSegment == 4)
            SpMVCSRLight4<Real, Index, OffsetsView, Fetch, Reduce, Keep ><<<blocks, threads>>>(
               offsets, first, last, fetch, reduce, keep, zero, grid );
         else if (threadsPerSegment == 8)
            SpMVCSRLight8<Real, Index, OffsetsView, Fetch, Reduce, Keep ><<<blocks, threads>>>(
               offsets, first, last, fetch, reduce, keep, zero, grid );
         else if (threadsPerSegment == 16)
            SpMVCSRLight16<Real, Index, OffsetsView, Fetch, Reduce, Keep ><<<blocks, threads>>>(
               offsets, first, last, fetch, reduce, keep, zero, grid );
         else if (threadsPerSegment == 32)
         { // CSR SpMV Light with threadsPerSegment = 32 is CSR Vector
            SpMVCSRVector<Real, Index, OffsetsView, Fetch, Reduce, Keep ><<<blocks, threads>>>(
               offsets, first, last, fetch, reduce, keep, zero, grid );
         }
         else if (threadsPerSegment == 64 )
         { // Execute CSR MultiVector
            reduceSegmentsCSRLightMultivectorKernel< 128, 64 ><<<blocks, threads>>>(
                     grid, offsets, first, last, fetch, reduce, keep, zero );
         }
         else //if (threadsPerSegment == 64 )
         { // Execute CSR MultiVector
            reduceSegmentsCSRLightMultivectorKernel< 128, 128 ><<<blocks, threads>>>(
                     grid, offsets, first, last, fetch, reduce, keep, zero );
         }*/
      }
      cudaStreamSynchronize(0);
      TNL_CHECK_CUDA_DEVICE;
#endif
   }
};


template< typename Index,
          typename Device >
    template< typename Offsets >
void
CSRLightKernel< Index, Device >::
init( const Offsets& offsets )
{
   TNL_ASSERT_GT( offsets.getSize(), 0, "offsets size must be strictly positive" );
   const Index segmentsCount = offsets.getSize() - 1;
    if( segmentsCount <= 0 )
       return;

   if( this->getThreadsMapping() == CSRLightAutomaticThreads )
   {
      const Index elementsInSegment = roundUpDivision( offsets.getElement( segmentsCount ), segmentsCount ); // non zeroes per row
      if( elementsInSegment <= 2 )
         setThreadsPerSegment( 2 );
      else if( elementsInSegment <= 4 )
         setThreadsPerSegment( 4 );
      else if( elementsInSegment <= 8 )
         setThreadsPerSegment( 8 );
      else if( elementsInSegment <= 16 )
         setThreadsPerSegment( 16 );
      else //if (nnz <= 2 * matrix.MAX_ELEMENTS_PER_WARP)
         setThreadsPerSegment( 32 ); // CSR Vector
      //else
      //   threadsPerSegment = roundUpDivision(nnz, matrix.MAX_ELEMENTS_PER_WARP) * 32; // CSR MultiVector
   }

   if( this->getThreadsMapping() == CSRLightAutomaticThreadsLightSpMV )
   {
      const Index elementsInSegment = roundUpDivision( offsets.getElement( segmentsCount ), segmentsCount ); // non zeroes per row
      if( elementsInSegment <= 2 )
         setThreadsPerSegment( 2 );
      else if( elementsInSegment <= 4 )
         setThreadsPerSegment( 4 );
      else if( elementsInSegment <= 8 )
         setThreadsPerSegment( 8 );
      else if( elementsInSegment <= 16 )
         setThreadsPerSegment( 16 );
      else //if (nnz <= 2 * matrix.MAX_ELEMENTS_PER_WARP)
         setThreadsPerSegment( 32 ); // CSR Vector
      //else
      //   threadsPerSegment = roundUpDivision(nnz, matrix.MAX_ELEMENTS_PER_WARP) * 32; // CSR MultiVector
   }
}

template< typename Index,
          typename Device >
void
CSRLightKernel< Index, Device >::
reset()
{
   this->threadsPerSegment = 0;
}

template< typename Index,
          typename Device >
auto
CSRLightKernel< Index, Device >::
getView() -> ViewType
{
    return *this;
}

template< typename Index,
          typename Device >
noa::TNL::String
CSRLightKernel< Index, Device >::
getKernelType()
{
    return "Light";
}

template< typename Index,
          typename Device >
auto
CSRLightKernel< Index, Device >::
getConstView() const -> ConstViewType
{
    return *this;
};


template< typename Index,
          typename Device >
    template< typename OffsetsView,
              typename Fetch,
              typename Reduce,
              typename Keep,
              typename Real >
void
CSRLightKernel< Index, Device >::
reduceSegments( const OffsetsView& offsets,
                Index first,
                Index last,
                Fetch& fetch,
                const Reduce& reduce,
                Keep& keep,
                const Real& zero ) const
{
   TNL_ASSERT_GE( this->threadsPerSegment, 0, "" );
   TNL_ASSERT_LE( this->threadsPerSegment, 33, "" );
   CSRLightKernelreduceSegmentsDispatcher< Index, Device, Fetch, Reduce, Keep >::reduce(
      offsets, first, last, fetch, reduce, keep, zero, this->threadsPerSegment );
}

template< typename Index,
          typename Device >
void
CSRLightKernel< Index, Device >::
setThreadsMapping( LightCSRSThreadsMapping mapping )
{
   this-> mapping = mapping;
}

template< typename Index,
          typename Device >
LightCSRSThreadsMapping
CSRLightKernel< Index, Device >::
getThreadsMapping() const
{
   return this->mapping;
}

template< typename Index,
          typename Device >
void
CSRLightKernel< Index, Device >::
setThreadsPerSegment( int threadsPerSegment )
{
   if( threadsPerSegment !=  1 &&
       threadsPerSegment !=  2 &&
       threadsPerSegment !=  4 &&
       threadsPerSegment !=  8 &&
       threadsPerSegment != 16 &&
       threadsPerSegment != 32 )
       throw std::runtime_error( "Number of threads per segment must be power of 2 - 1, 2, ... 32." );
   this->threadsPerSegment = threadsPerSegment;
}

template< typename Index,
          typename Device >
int
CSRLightKernel< Index, Device >::
getThreadsPerSegment() const
{
   return this->threadsPerSegment;
}


      } // namespace Segments
   }  // namespace Algorithms
} // namespace noa::TNL

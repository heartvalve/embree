// ======================================================================== //
// Copyright 2009-2014 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "parallel_for_for_prefix_sum.h"

namespace embree
{
  struct parallel_for_for_prefix_sum_regression_test : public RegressionTest
  {
    parallel_for_for_prefix_sum_regression_test(const char* name) : name(name) {
      registerRegressionTest(this);
    }
    
    bool operator() ()
    {
      bool passed = true;
      printf("%s::%s ... ",TOSTRING(isa),name);
      fflush(stdout);

      /* create vector with random numbers */
      const size_t M = 10;
      std::vector<atomic_t> flattened;
      typedef std::vector<std::vector<size_t>* > ArrayArray;
      ArrayArray array2(M);
      for (size_t i=0; i<M; i++) {
        const size_t N = ::random() % 10;
        array2[i] = new std::vector<size_t>(N);
        for (size_t j=0; j<N; j++) 
          (*array2[i])[j] = ::random() % 10;
      }
  
      ParallelForForPrefixSumState<ArrayArray> state(array2,size_t(1));
  
      /* dry run only counts */
      parallel_for_for_prefix_sum( state, [&](std::vector<size_t>* v, const range<size_t>& r, const size_t base) 
      {
        size_t s = 0;
	for (size_t i=r.begin(); i<r.end(); i++) s += (*v)[i];
        return s;
      });
      
      /* create properly sized output array */
      flattened.resize(state.size());
      memset(&flattened[0],0,sizeof(atomic_t)*flattened.size());

      /* now we actually fill the flattened array */
      parallel_for_for_prefix_sum( state, [&](std::vector<size_t>* v, const range<size_t>& r, const size_t base) 
      {
        size_t s = 0;
	for (size_t i=r.begin(); i<r.end(); i++) {
          for (size_t j=0; j<(*v)[i]; j++) {
            atomic_add(&flattened[base+s+j],1);
          }
          s += (*v)[i];
        }
        return s;
      });

      /* check if each element was assigned exactly once */
      for (size_t i=0; i<flattened.size(); i++)
        passed &= (flattened[i] == 1);
      
      /* delete arrays again */
      for (size_t i=0; i<array2.size(); i++)
	delete array2[i];

      /* output if test passed or not */
      if (passed) printf("[passed]\n");
      else        printf("[failed]\n");
      
      return passed;
    }

    const char* name;
  };

  parallel_for_for_prefix_sum_regression_test parallel_for_for_prefix_sum_regression("parallel_for_for_prefix_sum_regression_test");
}
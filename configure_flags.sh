#!/bin/bash

CPU=$1

if [ "${CPU}" = "arm1176jzf-s" ]; then
   export CXXFLAGS="-g -O2 -fdebug-prefix-map=$(pwd)=. -mcpu=${CPU} -mfpu=vfp -mfloat-abi=hard"
elif [ "${CPU}" = "cortex-a53" ]; then
   export CXXFLAGS="-g -O2 -fdebug-prefix-map=$(pwd)=. -mcpu=${CPU} -mfpu=neon-fp-armv8 -mfloat-abi=hard"
elif [ "${CPU}" = "cortex-a7" ]; then
   export CXXFLAGS="-g -O2 -fdebug-prefix-map=$(pwd)=. -mcpu=${CPU} -mfpu=neon-vfpv4 -mfloat-abi=hard"
elif [ "${CPU}" = "cortex-a72" ]; then
   export CXXFLAGS="-g -O2 -fdebug-prefix-map=$(pwd)=. -mcpu=${CPU} -mfpu=neon-fp-armv8 -mfloat-abi=hard"
elif [ "${CPU}" = "generic" ]; then
   export CXXFLAGS="-g -O2 -fdebug-prefix-map=$(pwd)=."
elif [ "${CPU}" = "nocpuspecific" ]; then
   export CXXFLAGS="-g -O2 -fdebug-prefix-map=$(pwd)=."
else
   echo "unknown core: ${CPU}"
   exit 1
fi

if [[ "${CPU}" = "nocpuspecific" ]]; then
   export BUCKET=r2cloud
else
   export BUCKET="r2cloud/cpu-${CPU}"
fi

export ASMFLAGS="${CXXFLAGS} -mthumb"
export CFLAGS=${CXXFLAGS}


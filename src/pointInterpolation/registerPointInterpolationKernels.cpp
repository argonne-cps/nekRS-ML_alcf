#include "bcMap.hpp"
#include "mesh.h"
#include <compileKernels.hpp>
#include <limits>

namespace {
// compute nearest power of two larger than v
unsigned nearestPowerOfTwo(unsigned int v)
{
  unsigned answer = 1;
  while (answer < v)
    answer *= 2;
  return answer;
}

void registerKernels(int N)
{
  const dlong Nq = N + 1;

  const std::string oklpath = getenv("NEKRS_KERNEL_DIR");

  auto findptsKernelInfo = platform->kernelInfo;
  findptsKernelInfo["includes"].asArray();

  findptsKernelInfo["defines/p_D"] = 3;
  findptsKernelInfo["defines/p_Nq"] = Nq;
  findptsKernelInfo["defines/p_Np"] = Nq * Nq * Nq;
  findptsKernelInfo["defines/p_nptsBlock"] = 4;

  unsigned int Nq2 = Nq * Nq;
  const auto blockSize = nearestPowerOfTwo(Nq2);

  findptsKernelInfo["defines/p_blockSize"] = blockSize;
  findptsKernelInfo["defines/p_Nfp"] = Nq * Nq;
  findptsKernelInfo["defines/dlong"] = dlongString;
  findptsKernelInfo["defines/hlong"] = hlongString;
  findptsKernelInfo["defines/dfloat"] = dfloatString;
  findptsKernelInfo["defines/DBL_MAX"] = 1e30;

  // findpts kernel currently requires INNER_SIZE > 3 * p_Nq
  // However, we must also make this a multiple of the warp size
  auto innerSize = 3 * Nq;
  if (innerSize % platform->warpSize)
    innerSize = (innerSize / platform->warpSize + 1) * platform->warpSize;

  findptsKernelInfo["defines/p_innerSize"] = innerSize;

  findptsKernelInfo["includes"] += oklpath + "/pointInterpolation/findpts/findpts.okl.hpp";
  findptsKernelInfo["includes"] += oklpath + "/pointInterpolation/findpts/poly.okl.hpp";

  std::string kernelName;
  std::string fileName;
  std::string orderSuffix = "_" + std::to_string(N);

  kernelName = "findptsLocal";
  fileName = oklpath + "/pointInterpolation/findpts/" + kernelName + ".okl";
  platform->kernelRequests.add(kernelName + orderSuffix, fileName, findptsKernelInfo);

  kernelName = "findptsLocalEval";
  fileName = oklpath + "/pointInterpolation/findpts/" + kernelName + ".okl";
  platform->kernelRequests.add(kernelName + orderSuffix, fileName, findptsKernelInfo);

  kernelName = "findptsLocalEvalMask";
  fileName = oklpath + "/pointInterpolation/findpts/" + kernelName + ".okl";
  platform->kernelRequests.add(kernelName + orderSuffix, fileName, findptsKernelInfo);
}

} // namespace

void registerPointInterpolationKernels()
{
  for (int i = 1; i < mesh_t::maxNqIntp; i++) registerKernels(i);
}

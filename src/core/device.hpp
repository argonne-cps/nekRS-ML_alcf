#ifndef device_hpp_
#define device_hpp_
#include <string>
#include <occa.hpp>
#include <mpi.h>
#include "nekrsSys.hpp"

class setupAide;
class comm_t;

class device_t
{
public:
  device_t(setupAide &options, comm_t &comm);

  occa::memory
  malloc(size_t Nbytes, const void *src = nullptr, const occa::properties &properties = occa::properties());

  occa::memory malloc(size_t Nbytes, const occa::properties &properties);

  template <class T = void> occa::memory malloc(size_t entries, const occa::memory &src);

  template <class T = void> occa::memory malloc(size_t entries, const void *src);

  occa::memory malloc(size_t Nwords, size_t wordSize, const occa::memory &src);

  template <class T = void> occa::memory malloc(size_t entries);

  occa::memory malloc(size_t Nwords, size_t wordSize);

  template <class T = void> occa::memory mallocHost(size_t entries);
  occa::memory mallocHost(size_t Nbytes);

  int id() const
  {
    return _device_id;
  }

  const occa::device &occaDevice() const
  {
    return _device;
  }

  std::string mode() const
  {
    return _device.mode();
  }

  occa::device &occaDevice()
  {
    return _device;
  }

  void finish()
  {
    _device.finish();
  }

  occa::kernel compileKernel(const std::string &fullPath,
                             const occa::properties &props,
                             const std::string &suffix, 
                             const MPI_Comm& comm) const;

  occa::kernel loadKernel(const std::string &fileName,
                          const std::string &kernelName,
                          const occa::properties &props,
                          const std::string &suffix = std::string()) const;

  occa::kernel loadKernel(const std::string &fileName,
                          const occa::properties &props,
                          const std::string &suffix = std::string()) const;

  bool deviceAtomic;

  void compilationFinished(){ _compilationEnabled = true; };

  void compileWhenLoad(){ _compileWhenLoad = true; };


  size_t memoryUsage() const;

  void printMemoryUsage(MPI_Comm comm = MPI_COMM_SELF) const;

private:
  occa::kernel wrapperCompileKernel(const std::string &fileName,
                            const occa::properties &props_,
                            const std::string &suffix) const;

  occa::kernel wrapperLoadKernel(const std::string &fileName,
                                 const std::string &kernelName,
                                 const occa::properties &props_,
                                 const std::string &suffix) const;

  comm_t &_comm;
  occa::device _device;
  int _device_id;
  bool _compilationEnabled;
  bool _compileWhenLoad;
};
#endif

#include <filesystem>
#include <thread>
#include <chrono>
#include "adiosStreamer.hpp"

// Initialize the ADIOS2 client
adios_client_t::adios_client_t(MPI_Comm& comm, nrs_t *nrs)
                               : _comm(comm)
{
    // Set nekrs object
    _nrs = nrs;

    // Set MPI comm, rank and size 
    //_comm = platform->comm.mpiComm;
    MPI_Comm_rank(_comm, &_rank);
    MPI_Comm_size(_comm, &_size);

    // Set up adios2 parameters
    platform->options.getArgs("ADIOS ML ENGINE",_engine);
    platform->options.getArgs("ADIOS ML TRANSPORT",_transport);
    platform->options.getArgs("ADIOS ML STREAM",_stream);

    // Initialize adios2
    if (_rank == 0)
        printf("\nInitializing ADIOS2 client ...\n");
    try
    {
        _adios = new adios2::ADIOS(_comm);

        _stream_io = _adios->DeclareIO("streamIO");
        _stream_io.SetEngine(_engine);
        if (_stream == "sync") {
            _params["RendezvousReaderCount"] = "1";
            _params["QueueFullPolicy"] = "Block";
            _params["QueueLimit"] = "1";
        } else if (_stream == "async") {
            _params["RendezvousReaderCount"] = "0";
            _params["QueueFullPolicy"] = "Block";
            _params["QueueLimit"] = "3";
        }
        _params["DataTransport"] = _transport;
        _params["OpenTimeoutSecs"] = "600";
        _stream_io.SetParameters(_params);

        _write_io = _adios->DeclareIO("writeIO");
    } 
    catch (std::exception &e)
    {
        printf("Exception, STOPPING PROGRAM from rank %d\n", _rank);
        std::cout << e.what() << "\n";
        fflush(stdout);
    }
    MPI_Barrier(_comm);
    if (_rank == 0)
        printf("All done\n");
    fflush(stdout);
}

// destructor
adios_client_t::~adios_client_t()
{
    if (_rank == 0) printf("Taking down adios_client_t\n");
}

// check if nekRS should quit
int adios_client_t::check_run()
{
    dlong exit_val = 1;
    int exists;
    std::string fname = "check-run.bp";

    // Check if check-run file exists
    if (_rank == 0) {
        if (std::filesystem::exists(fname)) {
            printf("Found check-run file!\n");
            fflush(stdout);
            exists = 1;
        } else {
            exists = 0;
        }
    }
    MPI_Bcast(&exists, 1, MPI_INT, 0, _comm);

    // Read check-run file if exists
    if (exists) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        adios2::Engine reader = _write_io.Open(fname, adios2::Mode::Read);
        reader.BeginStep();
        adios2::Variable<dlong> var = _write_io.InquireVariable<dlong>("check-run");
        if (_rank == 0 and var) {
            reader.Get(var, &exit_val);
        }
        reader.EndStep();
        reader.Close();
        MPI_Bcast(&exit_val, 1, MPI_INT, 0, _comm);
    }

    if (exit_val == 0 && _rank == 0) {
        printf("ML training says time to quit ...\n");
    }
    fflush(stdout);

    return exit_val;
}

// write checkpoint file
void adios_client_t::checkpoint()
{
    if (_rank == 0)
        printf("\nWriting checkpoint for GNN inference ...\n");
    std::string fname = "checkpoint.bp";
    unsigned long num_dim = _nrs->mesh->dim;
    unsigned long field_offset = _nrs->fieldOffset;
    dfloat *U = new dfloat[num_dim * field_offset]();
    _nrs->o_U.copyTo(U, num_dim * field_offset);

    adios2::Variable<dfloat> varU = _write_io.DefineVariable<dfloat>(
        "checkpoint", 
        {_size * num_dim * field_offset}, 
        {_rank * num_dim * field_offset}, 
        {num_dim * field_offset});
    adios2::Engine writer = _write_io.Open(fname, adios2::Mode::Write);
    writer.BeginStep();
    writer.Put<dfloat>(varU, U);
    writer.EndStep();
    writer.Close();
}

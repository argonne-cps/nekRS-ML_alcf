#include "nrs.hpp"
#include "platform.hpp"
#include "nekInterfaceAdapter.hpp"
#include "trajGen.hpp"
#include "gnn.hpp"
#include <cstdlib>
#include <filesystem>


void deleteDirectoryContents(const std::filesystem::path& dir)
{
    for (const auto& entry : std::filesystem::directory_iterator(dir))
        std::filesystem::remove_all(entry.path());
}


trajGen_t::trajGen_t(nrs_t *nrs_, int dt_factor_, int skip_, dfloat time_init_)
{
    nrs = nrs_; // set nekrs object
    mesh = nrs->mesh; // set mesh object
    dt_factor = dt_factor_; 
    skip = skip_;
    time_init = time_init_; 

    // set MPI rank and size 
    MPI_Comm &comm = platform->comm.mpiComm;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    irank = "_rank_" + std::to_string(rank);
    nranks = "_size_" + std::to_string(size);

    // allocate memory 
    //dlong N = mesh->Nelements * mesh->Np; // total number of nodes

    if (verbose) printf("\n[RANK %d] -- Finished instantiating trajGen_t object\n", rank);
    if (verbose) printf("[RANK %d] -- The number of elements is %d \n", rank, mesh->Nelements);
}

trajGen_t::~trajGen_t()
{
    if (verbose) printf("[RANK %d] -- trajGen_t destructor\n", rank);
}

void trajGen_t::trajGenSetup()
{
    if (verbose) printf("[RANK %d] -- in trajGenSetup() \n", rank);
    if (write)
    {
        //std::string irank = "_rank_" + std::to_string(rank);
        //std::string nranks = "_size_" + std::to_string(size);
        std::filesystem::path currentPath = std::filesystem::current_path();
        currentPath /= "traj";
        writePath = currentPath.string();
        int poly_order = mesh->Nq - 1;
        writePath = writePath + "_poly_" + std::to_string(poly_order) 
                    + "/tinit_" + std::to_string(time_init)
                    + "_dtfactor_" + std::to_string(dt_factor)
                    + "/data" + irank + nranks;
        if (!std::filesystem::exists(writePath))
        {
            std::filesystem::create_directories(writePath);
        }
        else
        {
            deleteDirectoryContents(writePath);
        }
        MPI_Comm &comm = platform->comm.mpiComm;
        MPI_Barrier(comm);
    }
}

void trajGen_t::trajGenWrite(dfloat time, int tstep, const std::string& field_name)
{
    if (write)
    {
        if (verbose) printf("[RANK %d] -- in trajGenWrite() \n", rank);
        
        // ~~~~ Write the data
        if ((tstep%dt_factor)==0)
        {
            dfloat *U = new dfloat[nrs->mesh->dim * nrs->fieldOffset]();
            dfloat *P = new dfloat[nrs->fieldOffset]();
            nrs->o_U.copyTo(U, nrs->mesh->dim * nrs->fieldOffset);
            nrs->o_P.copyTo(P, nrs->fieldOffset);
            
            // print stuff
            if (platform->comm.mpiRank == 0) {
                if (verbose) printf("[TRAJ WRITE] -- In tstep %d, at physical time %g \n", tstep, time);
            }

            // write data
            if (field_name == "velocity" || field_name == "all") {
                writeToFileBinaryF(writePath + "/u_step_" + std::to_string(tstep) + ".bin",
                                   U, nrs->fieldOffset, 3);
            }
            if (field_name == "pressure" || field_name == "all") {
                writeToFileBinaryF(writePath + "/p_step_" + std::to_string(tstep) + ".bin",
                                   P, nrs->fieldOffset, 1);
            }
        }
    }
}

#ifdef NEKRS_ENABLE_SMARTREDIS
void trajGen_t::trajGenWriteDB(smartredis_client_t* client, 
    dfloat time, 
    int tstep, 
    const std::string& field_name) 
{
    bool send_inputs = false;
    bool send_outputs = false;
    if (skip == 0) {
        if (tstep % dt_factor == 0) {
            send_inputs = true;
            send_outputs = true;
        }
    } else {
        if (tstep % skip == 0) {
            send_inputs = true;
        }
        if (tstep % skip == dt_factor) {
            send_outputs = true;
        }
    }

    if (send_inputs or send_outputs) {
        MPI_Comm &comm = platform->comm.mpiComm;
        unsigned long int num_dim = nrs->mesh->dim;
        unsigned long int field_offset = nrs->fieldOffset;

        // print stuff
        if (rank == 0 and verbose) {
            printf("[TRAJ WRITE DB] -- Writing data at tstep %d and physical time %g \n", tstep, time);
        }

        // write data
        if (field_name == "velocity" || field_name == "all") {
            dfloat *U = new dfloat[num_dim * field_offset]();
            nrs->o_U.copyTo(U, num_dim * field_offset);
            if (first_step) {
                client->append_dataset_to_list("u_step_" + std::to_string(tstep) + irank, "data",
                    "inputs" + irank, U, num_dim, field_offset);
            } else {
                if (send_outputs) {
                    client->append_dataset_to_list("u_step_" + std::to_string(tstep) + irank, "data",
                        "outputs" + irank, U, num_dim, field_offset);
                }
                if (send_inputs) {
                    client->append_dataset_to_list("u_step_" + std::to_string(tstep) + irank, "data",
                        "inputs" + irank, U, num_dim, field_offset);
                }
            }
        }
        if (field_name == "pressure" || field_name == "all") {
            dfloat *P = new dfloat[field_offset]();
            nrs->o_P.copyTo(P, field_offset);
            if (first_step) {
                client->append_dataset_to_list("p_step_" + std::to_string(tstep) + irank, "data",
                    "inputs" + irank, P, num_dim, field_offset);
            } else {
                if (send_outputs) {
                    client->append_dataset_to_list("p_step_" + std::to_string(tstep) + irank, "data",
                        "outputs" + irank, P, 1, field_offset);
                }
                if (send_inputs) {
                    client->append_dataset_to_list("p_step_" + std::to_string(tstep) + irank, "data",
                        "inputs" + irank, P, 1, field_offset);
                }
            }
        }
        if (first_step) first_step = false;
        MPI_Barrier(comm);
        if (rank == 0 and verbose) {
            printf("[TRAJ WRITE DB] -- Done writing data to DB\n");
        }
    }
}
#endif

void trajGen_t::trajGenWriteADIOS(adios_client_t* client, 
    dfloat time, 
    int tstep, 
    const std::string& field_name) 
{
    MPI_Comm &comm = platform->comm.mpiComm;
    dlong num_dim = nrs->mesh->dim;
    dlong field_offset = nrs->fieldOffset;
    bool store_inputs = false;
    bool send_data = false;

    if (skip == 0) {
        if (tstep % dt_factor == 0) {
            store_inputs = true;
            if (! first_step) {
                send_data = true;
            }
        }
    } else {
        if (tstep % skip == 0) {
            store_inputs = true;
        }
        if (tstep % skip == dt_factor) {
            send_data = true;
        }
    }

    if (first_step) {
        first_step = false;
        previous_U = new dfloat[num_dim * field_offset]();

        // Get global size of data
        int global;
        MPI_Allreduce(&field_offset, &global, 1, MPI_INT, MPI_SUM, comm);
        client->_field_offset = field_offset;
        client->_global_field_offset = global;

        // Gather size of data
        int* gathered = new int[size];
        int offset = 0;
        MPI_Allgather(&field_offset, 1, MPI_INT, gathered, 1, MPI_INT, MPI_COMM_WORLD);
        for (int i=0; i<rank; i++) {
            offset += gathered[i];
        }
        client->_offset_field_offset = offset;

        // Define ADIOS variables
        client->uIn = client->_stream_io.DefineVariable<dfloat>("in_u", 
                                                        {client->_global_field_offset * client->_num_dim}, // global dim
                                                        {client->_offset_field_offset * client->_num_dim}, // starting offset in global dim
                                                        {client->_field_offset * client->_num_dim}); // local size
        client->uOut = client->_stream_io.DefineVariable<dfloat>("out_u", 
                                                        {client->_global_field_offset * client->_num_dim}, // global dim
                                                        {client->_offset_field_offset * client->_num_dim}, // starting offset in global dim
                                                        {client->_field_offset * client->_num_dim}); // local size

        // Open the stream for transfering the solution data
        client->openStream();
    }

    if (send_data) {
        if (rank == 0) {
            printf("[TRAJ WRITE ADIOS] -- Writing data at tstep %d and physical time %g \n", tstep, time);
        }
        client->_solWriter.BeginStep();
        if (field_name == "velocity") {
            dfloat *U = new dfloat[num_dim * field_offset]();
            nrs->o_U.copyTo(U, num_dim * field_offset);
            client->_solWriter.Put<dfloat>(client->uIn, previous_U);
            client->_solWriter.Put<dfloat>(client->uOut, U);
        }
        client->_solWriter.EndStep();
        MPI_Barrier(comm);
        if (rank == 0) {
            printf("[TRAJ WRITE ADIOS] -- Done writing data\n");
        }
    }

    if (store_inputs) {
        nrs->o_U.copyTo(previous_U, num_dim * field_offset);
    }
}

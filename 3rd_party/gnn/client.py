import os
import sys
from typing import Optional, Union, Tuple
from omegaconf import DictConfig
import numpy as np
from time import sleep, perf_counter

# Import SmartRedis
try:
    from smartredis import Client, Dataset
except ModuleNotFoundError:
    pass

# Import ADIOS2
try:
    from adios2 import Stream, Adios, bindings
except ModuleNotFoundError:
    pass

class OnlineClient:
    """Class for the online training client
    """
    def __init__(self, cfg: DictConfig, comm) -> None:
        self.client = None
        self.backend = cfg.client.backend
        self.comm = comm
        self.size = self.comm.Get_size()
        self.rank = self.comm.Get_rank()
        self.local_rank = int(os.getenv("PALS_LOCAL_RANKID"))
        self.local_size = int(os.getenv("PALS_LOCAL_SIZE"))

        # initialize the client backend
        clients = ['smartredis', 'adios']
        if self.backend not in clients:
            sys.exit(f'Client {self.backend} not implemented. '\
                     f'Available options are: {clients}')
        self.init_client(cfg)

    def init_client(self, cfg: DictConfig) -> None:
        """Initialize the client based on the specified backend
        """
        if self.backend == 'smartredis':
            self.db_nodes = cfg.client.db_nodes
            SSDB = os.getenv('SSDB')
            if (self.db_nodes==1):
                self.client = Client(address=SSDB,cluster=False)
            else:
                self.client = Client(address=SSDB,cluster=True)
        elif self.backend == 'adios':
            self.engine = cfg.client.adios_engine
            self.stream = cfg.client.adios_stream
            self.transport = cfg.client.adios_transport
            adios = Adios(self.comm)
            self.client = adios.declare_io('streamIO')
            self.client.set_engine(self.engine)
            if self.stream == 'sync':
                parameters = {
                    'RendezvousReaderCount': '1', # producer waits for consumer in Open()
                    'QueueFullPolicy': 'Block', # wait for consumer to get every step
                    'QueueLimit': '1', # only buffer one step
                }
            elif self.stream == 'async': 
                parameters = {
                    'RendezvousReaderCount': '0', # producer does not wait for consumer in Open()
                    'QueueFullPolicy': 'Block', # slow consumer misses out on steps
                    'QueueLimit': '3', # buffer first step
                }
            parameters['DataTransport'] = self.transport # options: MPI, WAN, UCX, RDMA
            parameters['OpenTimeoutSecs'] = '600' # number of seconds producer waits on Open() for consumer
            self.client.set_parameters(parameters)

    def file_exists(self, file_name: str) -> bool:
        """Check if a file (or key) exists
        """
        if self.backend == 'smartredis':
            return self.client.key_exists(file_name)

    def get_array(self, file_name: Union[str, Dataset]) -> np.ndarray:
        """Get an array frpm staging area / simulation
        """
        tic = perf_counter()
        if self.backend == 'smartredis':
            if isinstance(file_name, str):
                while True:
                    if self.file_exists(file_name):
                        array = self.client.get_tensor(file_name)
                        break
                    else:
                        sleep(0.5)
                        t_elapsed = perf_counter() - tic
                        if t_elapsed > 300:
                            sys.exit(f'Could not find {file_name} in DB')
            else:
                array = file_name.get_tensor('data')
        if self.backend == 'adios':
            var_name = file_name.split('.')[0]
            with Stream(file_name, 'r', self.comm) as stream:
                stream.begin_step()
                arr = stream.inquire_variable(var_name)
                shape = arr.shape()
                count = int(shape[0] / self.size)
                start = count * self.rank
                if self.rank == self.size - 1:
                    count += shape[0] % self.size
                array = stream.read(var_name, [start], [count])
                stream.end_step()
        return array
    
    def put_array(self, file_name: str, array: np.ndarray) -> None:
        """Put/send an array to staging area / simulation
        """
        if self.backend == 'smartredis':
            self.client.put_tensor(file_name, array)

    def get_file_list(self, list_name: str) -> list:
        """Get the list of files to read
        """
        if self.backend == 'smartredis':
            # Ensure the list of DataSets is available
            while True:
                list_length = self.client.get_list_length(list_name)
                if list_length == 0:
                    sleep(1)
                    continue
                else:
                    break
            
            # Grab list of datasets
            file_list = self.client.get_datasets_from_list(list_name)
        return file_list
    
    def get_file_list_length(self, list_name: str) -> int:
        """Get the length of the file list
        """
        if self.backend == 'smartredis':
            return self.client.get_list_length(list_name)
        
    def get_graph_data_from_stream(self) -> dict:
        """Get the entire set of graph datasets from a stream
        """
        graph_data = {}
        if self.backend == 'adios':
            while True:
                if os.path.exists('./graph.bp'):
                    break
                else:
                    sleep(1)
            #with Stream(self.client, 'graphStream', 'r', self.comm) as stream:
            with Stream('graph.bp', 'r', self.comm) as stream:
                stream.begin_step()
                
                graph_data['Np'] = int(stream.read('Np'))

                arr = stream.inquire_variable('pos_node')
                shape = arr.shape()
                count = int(shape[0] / self.size)
                start = count * self.rank
                if self.rank == self.size - 1:
                    count += shape[0] % self.size
                graph_data['pos'] = stream.read('pos_node', [start], [count]).reshape((-1,3))

                arr = stream.inquire_variable('edge_index')
                shape = arr.shape()
                count = int(shape[0] / self.size)
                start = count * self.rank
                if self.rank == self.size - 1:
                    count += shape[0] % self.size
                graph_data['edge_index'] = stream.read('edge_index', [start], [count]).reshape((-1,2)).T

                arr = stream.inquire_variable('global_ids')
                shape = arr.shape()
                count = int(shape[0] / self.size)
                start = count * self.rank
                if self.rank == self.size - 1:
                    count += shape[0] % self.size
                graph_data['global_ids'] = stream.read('global_ids', [start], [count]).reshape((-1,1))

                arr = stream.inquire_variable('local_unique_mask')
                shape = arr.shape()
                count = int(shape[0] / self.size)
                start = count * self.rank
                if self.rank == self.size - 1:
                    count += shape[0] % self.size
                graph_data['local_unique_mask'] = stream.read('local_unique_mask', [start], [count])

                arr = stream.inquire_variable('halo_unique_mask')
                shape = arr.shape()
                count = int(shape[0] / self.size)
                start = count * self.rank
                if self.rank == self.size - 1:
                    count += shape[0] % self.size
                graph_data['halo_unique_mask'] = stream.read('halo_unique_mask', [start], [count])
                
                stream.end_step()
        return graph_data

    def get_train_data_from_stream(self) -> Tuple[np.ndarray,np.ndarray]:
        """Get the solution from a stream
        """
        if self.backend == 'adios':
            with Stream(self.client, 'solutionStream', 'r', self.comm) as stream:
                stream.begin_step()

                arr = stream.inquire_variable('out_u')
                shape = arr.shape()
                count = int(shape[0] / self.size)
                start = count * self.rank
                if self.rank == self.size - 1:
                    count += shape[0] % self.size
                outputs = stream.read('out_u', [start], [count]).reshape((-1,3))

                arr = stream.inquire_variable('in_u')
                shape = arr.shape()
                count = int(shape[0] / self.size)
                start = count * self.rank
                if self.rank == self.size - 1:
                    count += shape[0] % self.size
                inputs = stream.read('in_u', [start], [count]).reshape((-1,3))

                stream.end_step()
        return inputs, outputs

    def stop_nekRS(self) -> None:
        """Communicate to nekRS to stop running and exit cleanly
        """
        MLrun = 0
        if self.backend == 'smartredis':
            if self.db_nodes == 1:
                if self.rank % self.local_size == 0:
                    self.client.put_array('check-run',
                                          np.int32(np.array([MLrun]))
                    )
            else:
                if self.rank == 0:
                    self.client.put_array('check-run',
                                          np.int32(np.array([MLrun]))
                    )
        elif self.backend == 'adios':
            with Stream('check-run.bp', 'w', self.comm) as stream:
                if self.rank == 0:
                    stream.write("check-run", MLrun)
            


#ifndef __EXAMPI_BASIC_INTERFACE_H
#define __EXAMPI_BASIC_INTERFACE_H

#include <basic.h>
#include "basic/progress.h"

namespace exampi {
namespace basic {

class Interface : public exampi::i::Interface
{
  private:
    int rank;  // TODO: Don't keep this here, hand off to progress
    int ranks;
    std::vector<std::string> hosts;

  public:
    Interface() : rank(0) {};
    virtual int MPI_Init(int *argc, char ***argv)
    {

      // first param is config file, second is rank
      // third is epoch file, fourth is epoch
      std::cout << "Loading config from " << **argv << std::endl;
      exampi::global::config->load(**argv);
      exampi::global::worldSize = std::stoi((*exampi::global::config)["size"]);
      (*argv)++;
      (*argc)--;
      std::cout << "Taking rank to be arg " << **argv << std::endl;
      rank = atoi(**argv);
      (*argv)++;
      (*argc)--;
      std::cout << "Taking epoch config to be " << **argv << std::endl;
      exampi::global::epochConfig = std::string(**argv);
      (*argv)++;
      (*argc)--;
      std::cout << "Taking epoch to be " << **argv << std::endl;
      exampi::global::epoch = atoi(**argv);
      (*argv)++;
      (*argc)--;
      
      exampi::global::rank = rank;
      int st = exampi::global::checkpoint->load();
#if 0
      exampi::global::rank = rank;
      exampi::global::transport->init();
      exampi::global::progress->init();
      //exampi::global::progress->barrier();
#endif
      exampi::global::progress->barrier();

      std::cout << "Finished MPI_Init with code: " << st << "\n";
      return st;
    }

    virtual int MPI_Finalize() { return 0; }

    virtual int MPI_Send(const void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm) {
      size_t szcount = count;
      MPI_Status st = exampi::global::progress->postSend(
          {const_cast<void *>(buf), &(exampi::global::datatypes[datatype]), szcount},
          {dest, comm},
          tag).get();
      std::cout << debug() << "Finished MPI_Send: " << mpiStatusString(st) << "\n";
    	return 0;
    }

    virtual int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status) {
      //exampi::global::progress->recv_data(buf, count, datatype, source, tag, comm, status);
      size_t szcount = count;
      MPI_Status st = exampi::global::progress->postRecv(
          {const_cast<void *>(buf), &(exampi::global::datatypes[datatype]), szcount},
          tag).get();
      std::cout << debug() << "Finished MPI_Recv: " << mpiStatusString(st) << "\n";
      memmove(status, &st, sizeof(MPI_Status));
    	return 0;
    }

    virtual int MPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request)
    {
      size_t szcount = count;
      // have to move construct the future; i'll fix this later with a pool in progress
      std::future<MPI_Status> *f = new std::future<MPI_Status>();
      (*f) = exampi::global::progress->postSend(
            {const_cast<void *>(buf), &(exampi::global::datatypes[datatype]), szcount},
            {dest, comm}, tag);
      (*request) = reinterpret_cast<MPI_Request>(f);    
    
      return 0;
    }

    virtual int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request) {
      //exampi::global::progress->recv_data(buf, count, datatype, source, tag, comm, status);
      size_t szcount = count;
      std::future<MPI_Status> *f = new std::future<MPI_Status>();
      (*f) = exampi::global::progress->postRecv(
            {const_cast<void *>(buf), &(exampi::global::datatypes[datatype]), szcount},
            tag);
      (*request) = reinterpret_cast<MPI_Request>(f);
    	return 0;
    }

    virtual int MPI_Wait(MPI_Request *request, MPI_Status *status)
    {
      std::future<MPI_Status> *f = reinterpret_cast<std::future<MPI_Status> *>(*request);
      (*status) = f->get();
      return 0;
    }

    virtual int MPI_Bcast(void *buf, int count, MPI_Datatype datatype, int root, MPI_Comm comm)
    {
      if(exampi::global::rank == root)
      {
        for(int i = 0; i < exampi::global::worldSize; i++)
          if(i != root) 
            MPI_Send(buf, count, datatype, i, 0, 0);
      }
      else
      {
        MPI_Status st;
        MPI_Recv(buf, count, datatype, root, 0, 0, &st);
      }          
      return MPI_SUCCESS;
    }

    virtual int MPI_Comm_rank(MPI_Comm comm, int *r)
    {
      *r = rank;
      return 0;
    }

    virtual int MPI_Comm_size(MPI_Comm comm, int *r)
    {
      *r = std::stoi((*exampi::global::config)["size"]);
      return 0;
    }

    virtual int MPI_Checkpoint(int *savedEpoch)
    {
      *savedEpoch = exampi::global::epoch;
      exampi::global::checkpoint->save();
      return MPI_SUCCESS;
    }

    virtual int MPI_Epoch(int *epoch)
    {
      *epoch = exampi::global::epoch;
      return MPI_SUCCESS;
    }

	virtual int MPI_Barrier(MPI_Comm comm) {
		int rank, size;
		MPI_Status status;
        int coll_tag = 0;

		MPI_Comm_rank(comm, &rank);
		MPI_Comm_size(comm, &size);

		if (rank == 0) {
			MPI_Send((void *) 0, 0, MPI_INT, (rank + 1) % size, coll_tag, comm);
			MPI_Recv((void *) 0, 0, MPI_INT, (rank + size - 1) % size, coll_tag, comm, &status);
			MPI_Send((void *) 0, 0, MPI_INT, (rank + 1) % size, coll_tag, comm);
			MPI_Recv((void *) 0, 0, MPI_INT, (rank + size - 1) % size, coll_tag, comm, &status);
		} else {
			MPI_Recv((void *) 0, 0, MPI_INT, (rank + size - 1) % size, coll_tag, comm, &status);
			MPI_Send((void *) 0, 0, MPI_INT, (rank + 1) % size, coll_tag, comm);
			MPI_Recv((void *) 0, 0, MPI_INT, (rank + size - 1) % size, coll_tag, comm, &status);
			MPI_Send((void *) 0, 0, MPI_INT, (rank + 1) % size, coll_tag, comm);
		}

		return MPI_SUCCESS;
	}
};


} // namespace basic
} // namespace exampi

#endif //...H

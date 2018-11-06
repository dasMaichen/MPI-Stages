#ifndef __EXAMPI_COMM_H
#define __EXAMPI_COMM_H

#include <global.h>
#include <context.h>
#include <group.h>
#include <memory>

namespace exampi {

class Comm {
public:
	Comm() {
	}
	Comm(bool _isintra, exampi::Group *_local,
			exampi::Group *_remote) :
			is_intra(_isintra), local(_local), remote(_remote) {
	}

	virtual ~Comm() {
	}

	// fix rule of 5 later
	// Comm &operator=(Comm &rhs) = 0;
	int get_next_context(int *pt2pt, int *coll) {
		int rc;
		if (rank == 0) {
			//Context::contextLock.lock();
			Context::nextID++;
			*pt2pt = Context::nextID;
			Context::nextID++;
			*coll = Context::nextID;
			//Context::contextLock.unlock();
		}
		rc = MPI_Bcast(pt2pt, 1, MPI_INT, 0, local_pt2pt);
		if (rc != MPI_SUCCESS) {
			return MPIX_TRY_RELOAD;
		}
		rc = MPI_Bcast(coll, 1, MPI_INT, 0, local_pt2pt);
		if (rc != MPI_SUCCESS) {
			return MPIX_TRY_RELOAD;
		}

		return 0;
	}
	// accessors
	exampi::Group* get_local_group() {
		return local;
	}
	exampi::Group* get_remote_group() {
		return remote;
	}

	void set_local_group(Group *group) {
		local = group;
	}

	void set_remote_group(Group *group) {
		remote = group;
	}

	int get_context_id_pt2pt() const {
		return local_pt2pt;
	}
	int get_context_id_coll() const {
		return local_coll;
	}

	void set_rank(int r) {
		rank = r;
	}
	void set_context(int pt2pt, int coll) {
		local_pt2pt = pt2pt;
		local_coll = coll;
	}

	int get_rank() {
		return rank;
	}

	bool get_is_intra() {
		return is_intra;
	}

	void set_is_intra(bool intra) {
		is_intra = intra;
	}

	// Nawrin task for later, introduce the entire MPI API here as methods of this comm; right now, we do "Shane-mode,"
	// where the C API directly calls the Interface singledon, which is allowed to use accessors of Comm for info.
	//
	// [future version only]
	//
protected:
	bool is_intra;
	exampi::Group *local;
	exampi::Group *remote;

	int local_pt2pt;
	int local_coll;

	int rank;

	//  context_id remote_pt2pt;
	//  context_id remote_coll;

private:

};

}

#endif
#include <libgeodecomp/config.h>
#ifdef LIBGEODECOMP_WITH_MPI
#ifndef LIBGEODECOMP_IO_MPIIOWRITER_H
#define LIBGEODECOMP_IO_MPIIOWRITER_H

#include <libgeodecomp/io/mpiio.h>
#include <libgeodecomp/io/writer.h>
#include <libgeodecomp/communication/typemaps.h>
#include <libgeodecomp/misc/clonable.h>

namespace LibGeoDecomp {

template<typename CELL_TYPE>
class MPIIOWriter : public Clonable<Writer<CELL_TYPE>, MPIIOWriter<CELL_TYPE> >
{
public:
    friend class MPIIOWriterTest;
    friend class MPIIOInitializerTest;

    typedef typename Writer<CELL_TYPE>::GridType GridType;
    typedef typename Writer<CELL_TYPE>::Topology Topology;

    static const int DIM = Topology::DIM;

    MPIIOWriter(
        const std::string& prefix,
        const unsigned period,
        const unsigned maxSteps,
        const MPI_Comm& communicator = MPI_COMM_WORLD,
        MPI_Datatype mpiDatatype = Typemaps::lookup<CELL_TYPE>()) :
        Clonable<Writer<CELL_TYPE>, MPIIOWriter<CELL_TYPE> >(prefix, period),
        maxSteps(maxSteps),
        comm(communicator),
        datatype(mpiDatatype)
    {}

    virtual void stepFinished(const GridType& grid, unsigned step, WriterEvent event)
    {
        if ((event == WRITER_STEP_FINISHED) && (step % period != 0)) {
            return;
        }

        Region<DIM> region;
        region << grid.boundingBox();

        MPIIO<CELL_TYPE>::writeRegion(
            grid,
            grid.dimensions(),
            step,
            maxSteps,
            filename(step),
            region,
            datatype,
            comm);
    }

private:
    using Writer<CELL_TYPE>::period;
    using Writer<CELL_TYPE>::prefix;
    unsigned maxSteps;
    MPI_Comm comm;
    MPI_Datatype datatype;

    std::string filename(const unsigned& step) const
    {
        std::ostringstream buf;
        buf << prefix << std::setfill('0') << std::setw(5) << step << ".mpiio";
        return buf.str();
    }
};

}

#endif
#endif

#include <cmath>
#include <mpi.h>

#include <libgeodecomp/io/tracingwriter.h>
#include <libgeodecomp/io/simplecellplotter.h>
#include <libgeodecomp/io/simpleinitializer.h>
#include <libgeodecomp/loadbalancer/tracingbalancer.h>
#include <libgeodecomp/loadbalancer/noopbalancer.h>
#include <libgeodecomp/misc/apitraits.h>
#include <libgeodecomp/parallelization/hiparsimulator.h>
#include <libgeodecomp/parallelization/hiparsimulator/partitions/recursivebisectionpartition.h>

using namespace LibGeoDecomp;

class Cell
{
public:
    class API :
        public APITraits::HasFixedCoordsOnlyUpdate,
        public APITraits::HasUpdateLineX,
        public APITraits::HasStencil<Stencils::VonNeumann<3, 1> >,
        public APITraits::HasCubeTopology<3>
    {};

    inline explicit Cell(double v = 0) :
        temp(v)
    {}

    template<typename NEIGHBORHOOD>
    void update(const NEIGHBORHOOD& hood, const unsigned& nanoStep)
    {
        temp = (hood[FixedCoord< 0,  0, -1>()].temp +
                hood[FixedCoord< 0, -1,  0>()].temp +
                hood[FixedCoord<-1,  0,  0>()].temp +
                hood[FixedCoord< 1,  0,  0>()].temp +
                hood[FixedCoord< 0,  1,  0>()].temp +
                hood[FixedCoord< 0,  0,  1>()].temp) * (1.0 / 6.0);
    }

    // fixme: use locally defined var for x-offset?
    template<typename NEIGHBORHOOD>
    static void updateLineX(Cell *target, long *x, long endX, const NEIGHBORHOOD& hood, int /* nanoStep */)
    {
        for (; *x < endX; ++x) {
            target[*x].temp = (hood[FixedCoord< 0,  0, -1>()].temp +
                               hood[FixedCoord< 0, -1,  0>()].temp +
                               hood[FixedCoord<-1,  0,  0>()].temp +
                               hood[FixedCoord< 1,  0,  0>()].temp +
                               hood[FixedCoord< 0,  1,  0>()].temp +
                               hood[FixedCoord< 0,  0,  1>()].temp) * (1.0 / 6.0);
        }
    }

    double temp;
};

class CellInitializer : public SimpleInitializer<Cell>
{
public:
    using SimpleInitializer<Cell>::gridDimensions;

    CellInitializer(int num) : SimpleInitializer<Cell>(
        Coord<3>(128 * num,
                 128 * num,
                 128 * num), 1000)
    {}

    virtual void grid(GridBase<Cell, 3> *ret)
    {
        CoordBox<3> box = ret->boundingBox();
        Coord<3> offset =
            Coord<3>::diagonal(gridDimensions().x() * 5 / 128);
        int size = gridDimensions().x() * 50 / 128;


        for (int z = 0; z < size; ++z) {
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    Coord<3> c = offset + Coord<3>(x, y, z);
                    if (box.inBounds(c)) {
                        ret->set(c, Cell(0.99999999999));
                    }
                }
            }
        }
    }
};

void runSimulation()
{
    int outputFrequency = 100;
    int factor = pow(MPILayer().size(), 1.0 / 3.0);

    CellInitializer *init = new CellInitializer(factor);

    HiParSimulator::HiParSimulator<Cell, HiParSimulator::RecursiveBisectionPartition<3> > sim(
        init,
        MPILayer().rank() ? 0 : new TracingBalancer(new NoOpBalancer()),
        1000,
        1,
        MPI_DOUBLE);

    if (MPILayer().rank() == 0) {
        sim.addWriter(
            new TracingWriter<Cell>(outputFrequency, init->maxSteps()));
    }

    sim.run();
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    Typemaps::initializeMaps();

    runSimulation();

    MPI_Finalize();
    return 0;
}

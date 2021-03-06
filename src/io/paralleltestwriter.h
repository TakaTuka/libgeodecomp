#ifndef LIBGEODECOMP_IO_PARALLELTESTWRITER_H
#define LIBGEODECOMP_IO_PARALLELTESTWRITER_H

#include <list>
#include <libgeodecomp/io/parallelwriter.h>
#include <libgeodecomp/misc/clonable.h>
#include <libgeodecomp/misc/testcell.h>
#include <libgeodecomp/misc/testhelper.h>

namespace LibGeoDecomp {

/**
 * This class serves to verify the callback behavior of
 * implementations of DistributedSimulator.
 */
class ParallelTestWriter : public Clonable<ParallelWriter<TestCell<2> >, ParallelTestWriter>
{
public:
    typedef ParallelWriter<TestCell<2> >::GridType GridType;
    using ParallelWriter<TestCell<2> >::region;

    ParallelTestWriter(
        const unsigned period,
        const std::vector<unsigned>& expectedSteps,
        const std::vector<WriterEvent> expectedEvents)  :
        Clonable<ParallelWriter<TestCell<2> >, ParallelTestWriter>("", period),
        expectedSteps(expectedSteps),
        expectedEvents(expectedEvents),
        lastStep(-1)
    {}

    virtual void stepFinished(
        const GridType& grid,
        const Region<Topology::DIM>& validRegion,
        const Coord<Topology::DIM>& globalDimensions,
        unsigned step,
        WriterEvent event,
        std::size_t rank,
        bool lastCall)
    {
        // ensure setRegion() has actually been called
        TS_ASSERT(!region.empty());

        // ensure that all parts of this->region have been accounted for
        if (lastStep != step) {
            TS_ASSERT(unaccountedRegion.empty());
            unaccountedRegion = region;
        }
        unaccountedRegion -= validRegion;

        unsigned myExpectedCycle = APITraits::SelectNanoSteps<TestCell<2> >::VALUE * step;
        TS_ASSERT_TEST_GRID_REGION(GridType, grid, validRegion, myExpectedCycle);

        TS_ASSERT(!expectedSteps.empty());
        unsigned expectedStep = expectedSteps.front();
        WriterEvent expectedEvent = expectedEvents.front();
        if (lastCall) {
            expectedSteps.erase(expectedSteps.begin());
            expectedEvents.erase(expectedEvents.begin());
        }
        TS_ASSERT_EQUALS(expectedStep, step);
        TS_ASSERT_EQUALS(expectedEvent, event);

        // ensure setRegion() has actually been called
        TS_ASSERT(!region.empty());
        // ensure validRegion is a subset of what was specified via setRegion()
        if (!(validRegion - region).empty()) {
            std::cout << "deltaRegion: " << (validRegion - region) << "\n";
        }
        TS_ASSERT((validRegion - region).empty());
        // check that all parts of the specified region were actually consumed
        if (lastCall) {
            TS_ASSERT(unaccountedRegion.empty());
        }

        lastStep = step;
    }

private:
    std::vector<unsigned> expectedSteps;
    std::vector<WriterEvent> expectedEvents;
    unsigned lastStep;
    Region<2> unaccountedRegion;
};

}

#endif

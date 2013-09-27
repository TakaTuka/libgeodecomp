#include <boost/assign/std/vector.hpp>
#include <libgeodecomp/mpilayer/mpilayer.h>
#include <libgeodecomp/parallelization/hiparsimulator/partitionmanager.h>
#include <libgeodecomp/parallelization/hiparsimulator/partitions/stripingpartition.h>

using namespace boost::assign;
using namespace LibGeoDecomp;
using namespace HiParSimulator;

namespace LibGeoDecomp {
namespace HiParSimulator {

class PartitionManagerTest : public CxxTest::TestSuite
{
public:
    void setUp()
    {
        layer = MPILayer();
        manager = PartitionManager<Topologies::Cube<2>::Topology>();
        dimensions = Coord<2>(20, 400);

        // assume this is a dual cluster setup and on the current
        // level we're in the second cluster which is responsible for
        // the (dimensions.y() - offset / dimensions.x()) last lines of
        // the StripingPartition.
        offset = 130 * dimensions.x();
        ghostZoneWidth = 6;
        weights = std::vector<std::size_t>(layer.size(), 30 * dimensions.x());
        weights[3] = 40 * dimensions.x();
        weights[5] = 20 * dimensions.x();
        // sanity check
        unsigned product = dimensions.prod();
        TS_ASSERT_EQUALS(sum(weights) + offset, product);

        partition.reset(new StripingPartition<2>(Coord<2>(0, 0), dimensions, offset, weights));
        manager.resetRegions(
            CoordBox<2>(Coord<2>(), dimensions),
            partition,
            layer.rank(),
            ghostZoneWidth);
        std::vector<CoordBox<2> > boundingBoxes(
            layer.allGather(manager.ownRegion().boundingBox()));
        manager.resetGhostZones(boundingBoxes);
    }

    void testOuterAndInnerGhostZoneFragments()
    {
        for (unsigned i = 0; i < layer.size(); ++i) {
            if ((i == layer.rank() - 1) || (i == layer.rank() + 1)) {
                std::vector<Region<2> > outerFragments;
                std::vector<Region<2> > innerFragments;

                unsigned startLine = startingLine(i);
                if (i == layer.rank() - 1)
                    startLine = startingLine(i + 1);
                for (unsigned width = 0; width <= ghostZoneWidth; ++width) {
                    Region<2> outerFragment, innerFragment;
                    for (unsigned g = 0; g < width; ++g) {
                        outerFragment << Streak<2>(
                            Coord<2>(0, startLine - g - 1), dimensions.x());
                        innerFragment << Streak<2>(
                            Coord<2>(0, startLine + g),     dimensions.x());
                    }
                    outerFragments.push_back(outerFragment);
                    innerFragments.push_back(innerFragment);
                }
                // reverse for lower neighbor
                if (i == layer.rank() + 1)
                    std::swap(outerFragments, innerFragments);

                TS_ASSERT_EQUALS(outerFragments,
                                 manager.getOuterGhostZoneFragments()[i]);
                TS_ASSERT_EQUALS(innerFragments,
                                 manager.getInnerGhostZoneFragments()[i]);
            } else {
                TS_ASSERT_EQUALS(manager.getOuterGhostZoneFragments().count(i), unsigned(0));
            }
        }
    }

    void testOwnAndExpandedRegion()
    {
        unsigned startLine = startingLine(layer.rank());
        unsigned endLine   = startingLine(layer.rank() + 1);
        TS_ASSERT_EQUALS(fillLines(startLine, endLine), manager.ownRegion());

        startLine = startingLine(layer.rank()) - ghostZoneWidth;
        endLine   = startingLine(layer.rank() + 1) + ghostZoneWidth;
        if (layer.rank() == layer.size() - 1)
            endLine -= ghostZoneWidth;
        Region<2> expectedOwnExpandedRegion = fillLines(startLine, endLine);
        TS_ASSERT_EQUALS(expectedOwnExpandedRegion, manager.ownExpandedRegion());
    }

    void testRims()
    {
        TS_ASSERT_EQUALS(ghostZoneWidth + 1, manager.ownRims.size());
        for (unsigned i = 0; i <= ghostZoneWidth; ++i) {
            unsigned startLine = startingLine(layer.rank()) - 1 * ghostZoneWidth + i;
            unsigned endLine   = startingLine(layer.rank()) + 2 * ghostZoneWidth - i;
            Region<2> rim = fillLines(startLine, endLine);
            if (layer.rank() != layer.size() - 1) {
                startLine = startingLine(layer.rank() + 1) - 2 * ghostZoneWidth + i;
                endLine   = startingLine(layer.rank() + 1) + 1 * ghostZoneWidth - i;
                rim += fillLines(startLine, endLine);
            }
            TS_ASSERT_EQUALS(rim, manager.rim(i));
        }
    }

    void testInnerSets()
    {
        TS_ASSERT_EQUALS(ghostZoneWidth + 1, manager.ownInnerSets.size());
        for (unsigned i = 0; i <= ghostZoneWidth; ++i) {
            unsigned startLine = startingLine(layer.rank()) + i;
            unsigned endLine   = startingLine(layer.rank() + 1) - i;
            if (layer.rank() == layer.size() - 1)
                endLine += i;
            TS_ASSERT_EQUALS(fillLines(startLine, endLine), manager.innerSet(i));
        }
    }

    void testOutgroupGhostZones()
    {
        if (layer.rank() == 0) {
            unsigned startLine = startingLine(layer.rank()) - ghostZoneWidth;
            unsigned endLine   = startingLine(layer.rank());
            TS_ASSERT_EQUALS(fillLines(startLine, endLine), manager.getOuterOutgroupGhostZoneFragment());
            startLine = startingLine(layer.rank());
            endLine   = startingLine(layer.rank()) + ghostZoneWidth;
            TS_ASSERT_EQUALS(fillLines(startLine, endLine), manager.getInnerOutgroupGhostZoneFragment());
        } else {
            TS_ASSERT(manager.getInnerOutgroupGhostZoneFragment().empty());
            TS_ASSERT(manager.getOuterOutgroupGhostZoneFragment().empty());
        }
    }

    void testVolatileKernel()
    {
        Region<2> expected;
        unsigned startLine =
            startingLine(layer.rank()) + ghostZoneWidth;
        unsigned endLine   =
            startingLine(layer.rank()) + ghostZoneWidth * 2 - 1;
        expected += fillLines(startLine, endLine);

        if (layer.rank() != layer.size() - 1) {
            unsigned startLine =
                startingLine(layer.rank() + 1) - ghostZoneWidth * 2 + 1;
            unsigned endLine   =
                startingLine(layer.rank() + 1) - ghostZoneWidth;
            expected += fillLines(startLine, endLine);
        }

        TS_ASSERT_EQUALS(expected, manager.getVolatileKernel());
    }

private:
    MPILayer layer;
    PartitionManager<Topologies::Cube<2>::Topology> manager;
    boost::shared_ptr<StripingPartition<2> > partition;
    Coord<2> dimensions;
    std::vector<std::size_t> weights;
    unsigned offset;
    unsigned ghostZoneWidth;

    unsigned startingLine(const unsigned& node)
    {
        unsigned ret = offset;
        for (unsigned i = 0; i < node; ++i)
            ret += weights[i];
        return ret / dimensions.x();
    }

    Region<2> fillLines(const unsigned& startLine, const unsigned& endLine)
    {
        Region<2> ret;
        for (unsigned row = startLine; row < endLine; ++row)
            ret << Streak<2>(Coord<2>(0, row), dimensions.x());
        return ret;
    }
};

}
}

#ifndef LIBGEODECOMP_GEOMETRY_PARTITIONMANAGER_H
#define LIBGEODECOMP_GEOMETRY_PARTITIONMANAGER_H

#include <libgeodecomp/config.h>
#include <libgeodecomp/geometry/partitions/stripingpartition.h>
#include <libgeodecomp/geometry/region.h>

#include <boost/shared_ptr.hpp>

namespace LibGeoDecomp {

/**
 * The PartitionManager maintains the Regions which describe a node's
 * subdomain (as defined by a Partition) and the inner and outer ghost
 * regions (halos) which are used for synchronization with neighboring
 * subdomains.
 */
template<typename TOPOLOGY>
class PartitionManager
{
public:
    friend class PartitionManagerTest;
    friend class VanillaStepperTest;

    typedef TOPOLOGY Topology;
    static const int DIM = Topology::DIM;
    typedef std::map<int, std::vector<Region<DIM> > > RegionVecMap;

    enum AccessCode {
        OUTGROUP = -1
    };

    explicit PartitionManager(
        const CoordBox<DIM>& simulationArea=CoordBox<DIM>())
    {
        std::vector<std::size_t> weights(1, simulationArea.size());
        boost::shared_ptr<Partition<DIM> > partition(
            new StripingPartition<DIM>(Coord<DIM>(), simulationArea.dimensions, 0, weights));
        resetRegions(simulationArea, partition, 0, 1);
        resetGhostZones(std::vector<CoordBox<DIM> >(1));
    }

    /**
     * resets the domain decomposition. The simulation space is
     * described by newSimulationArea, the decomposition scheme by
     * newPartition. newRank will usually correspond to the MPI rank
     * and identifies the current process. newGhostZoneWidth specifies
     * after how many steps the halo should be synchronized. Higher
     * values mean that the halo will be wider, which requires fewer
     * synchronizations, but the syncs need to communicate more data.
     * This is primarily to combat high latency datapaths (e.g.
     * network latency or if the data needs to go to remote
     * accelerators).
     */
    inline void resetRegions(
        const CoordBox<DIM>& newSimulationArea,
        boost::shared_ptr<Partition<DIM> > newPartition,
        unsigned newRank,
        unsigned newGhostZoneWidth)
    {
        partition = newPartition;
        simulationArea = newSimulationArea;
        myRank = newRank;
        ghostZoneWidth = newGhostZoneWidth;
        regions.clear();
        outerGhostZoneFragments.clear();
        innerGhostZoneFragments.clear();
        fillOwnRegion();
    }

    inline void resetGhostZones(
        const std::vector<CoordBox<DIM> >& newBoundingBoxes)
    {
        boundingBoxes = newBoundingBoxes;
        CoordBox<DIM> ownBoundingBox = ownExpandedRegion().boundingBox();

        for (unsigned i = 0; i < boundingBoxes.size(); ++i) {
            if ((i != myRank) &&
                boundingBoxes[i].intersects(ownBoundingBox) &&
                (!(getRegion(myRank, ghostZoneWidth) &
                   getRegion(i,      0)).empty() ||
                 !(getRegion(i,      ghostZoneWidth) &
                   getRegion(myRank, 0)).empty())) {
                intersect(i);
            }
        }

        // outgroup ghost zone fragments are computed a tad generous,
        // an exact, greedy calculation would be more complicated
        Region<DIM> outer = outerRim;
        Region<DIM> inner = rim(getGhostZoneWidth());
        for (typename RegionVecMap::iterator i = outerGhostZoneFragments.begin();
             i != outerGhostZoneFragments.end();
             ++i) {
            if (i->first != OUTGROUP) {
                outer -= i->second.back();
            }
        }
        for (typename RegionVecMap::iterator i = innerGhostZoneFragments.begin();
             i != innerGhostZoneFragments.end();
             ++i) {
            if (i->first != OUTGROUP) {
                inner -= i->second.back();
            }
        }
        outerGhostZoneFragments[OUTGROUP] =
            std::vector<Region<DIM> >(getGhostZoneWidth() + 1, outer);
        innerGhostZoneFragments[OUTGROUP] =
            std::vector<Region<DIM> >(getGhostZoneWidth() + 1, inner);
    }

    inline RegionVecMap& getOuterGhostZoneFragments()
    {
        return outerGhostZoneFragments;
    }

    inline RegionVecMap& getInnerGhostZoneFragments()
    {
        return innerGhostZoneFragments;
    }

    inline const Region<DIM>& getInnerOutgroupGhostZoneFragment()
    {
        return innerGhostZoneFragments[OUTGROUP].back();
    }

    inline const Region<DIM>& getOuterOutgroupGhostZoneFragment()
    {
        return outerGhostZoneFragments[OUTGROUP].back();
    }

    inline const Region<DIM>& getRegion(
        int node,
        unsigned expansionWidth)
    {
        if (regions.count(node) == 0) {
            fillRegion(node);
        }
        return regions[node][expansionWidth];
    }

    inline const Region<DIM>& ownRegion(unsigned expansionWidth = 0)
    {
        return regions[myRank][expansionWidth];
    }

    inline const Region<DIM>& ownExpandedRegion()
    {
        return regions[myRank].back();
    }

    /**
     * Rim describes the node's inner ghost zone and those surrounding
     * coordinates required to update those.
     */
    inline const Region<DIM>& rim(unsigned dist)
    {
        return ownRims[dist];
    }

    /**
     * inner set refers to that part of a node's domain which are
     * required to update the kernel.
     */
    inline const Region<DIM>& innerSet(unsigned dist)
    {
        return ownInnerSets[dist];
    }

    inline const std::vector<CoordBox<DIM> >& getBoundingBoxes() const
    {
        return boundingBoxes;
    }

    inline unsigned getGhostZoneWidth() const
    {
        return ghostZoneWidth;
    }

    /**
     * outer rim is the union of all outer ghost zone fragments.
     */
    inline const Region<DIM>& getOuterRim() const
    {
        return outerRim;
    }

    /**
     * The volatile kernel is the part of the kernel which may be
     * overwritten while updating the inner ghost zone.
     */
    inline const Region<DIM>& getVolatileKernel() const
    {
        return volatileKernel;
    }

    /**
     * The inner rim is the part of the kernel which is required for
     * updating the own rims. Its similar to the outer ghost zone, but
     * to the inner side. It usually includes just one stencil
     * diameter more cells than the volatile kernel.
     */
    inline const Region<DIM>& getInnerRim() const
    {
        return innerRim;
    }

    inline const std::vector<std::size_t>& getWeights() const
    {
        return partition->getWeights();
    }

    const Adjacency& adjacency() const
    {
        return partition->getAdjacency();
    }

    inline unsigned rank() const
    {
        return myRank;
    }

    inline const Coord<DIM>& getSimulationArea() const
    {
        return simulationArea.dimensions;
    }

private:
    boost::shared_ptr<Partition<DIM> > partition;
    CoordBox<DIM> simulationArea;
    Region<DIM> outerRim;
    Region<DIM> volatileKernel;
    Region<DIM> innerRim;
    RegionVecMap regions;
    RegionVecMap outerGhostZoneFragments;
    RegionVecMap innerGhostZoneFragments;
    std::vector<Region<DIM> > ownRims;
    std::vector<Region<DIM> > ownInnerSets;
    unsigned myRank;
    unsigned ghostZoneWidth;
    std::vector<CoordBox<DIM> > boundingBoxes;

    inline void fillRegion(unsigned node)
    {
        std::vector<Region<DIM> >& regionExpansion = regions[node];
        regionExpansion.resize(getGhostZoneWidth() + 1);
        regionExpansion[0] = partition->getRegion(node);
        for (std::size_t i = 1; i <= getGhostZoneWidth(); ++i) {
            Region<DIM> expanded;
            const Region<DIM>& reg = regionExpansion[i - 1];
            expanded = reg.expandWithTopology(
                1,
                simulationArea.dimensions,
                Topology(),
                adjacency());
            regionExpansion[i] = expanded;
        }
    }

    inline void fillOwnRegion()
    {
        fillRegion(myRank);
        Region<DIM> surface(
            ownRegion().expandWithTopology(
                1,
                simulationArea.dimensions,
                Topology(),
                adjacency()) - ownRegion());
        Region<DIM> kernel(
            ownRegion() -
            surface.expandWithTopology(
                getGhostZoneWidth(),
                simulationArea.dimensions,
                Topology(),
                adjacency()));
        outerRim = ownExpandedRegion() - ownRegion();
        ownRims.resize(getGhostZoneWidth() + 1);
        ownInnerSets.resize(getGhostZoneWidth() + 1);

        ownRims.back() = ownRegion() - kernel;
        for (int i = getGhostZoneWidth() - 1; i >= 0; --i) {
            ownRims[i] = ownRims[i + 1].expandWithTopology(
                1, simulationArea.dimensions, Topology(), adjacency());
        }

        ownInnerSets.front() = ownRegion();
        Region<DIM> minuend = surface.expandWithTopology(
            1, simulationArea.dimensions, Topology(), adjacency());
        for (std::size_t i = 1; i <= getGhostZoneWidth(); ++i) {
            ownInnerSets[i] = ownInnerSets[i - 1] - minuend;
            minuend = minuend.expandWithTopology(1, simulationArea.dimensions, Topology(), adjacency());
        }

        volatileKernel = ownInnerSets.back() & rim(0);
        innerRim       = ownInnerSets.back() & rim(0);
    }

    inline void intersect(unsigned node)
    {
        std::vector<Region<DIM> >& outerGhosts = outerGhostZoneFragments[node];
        std::vector<Region<DIM> >& innerGhosts = innerGhostZoneFragments[node];
        outerGhosts.resize(getGhostZoneWidth() + 1);
        innerGhosts.resize(getGhostZoneWidth() + 1);
        for (unsigned i = 0; i <= getGhostZoneWidth(); ++i) {
            outerGhosts[i] = getRegion(myRank, i) & getRegion(node, 0);
            innerGhosts[i] = getRegion(myRank, 0) & getRegion(node, i);
        }
    }
};

}

#endif

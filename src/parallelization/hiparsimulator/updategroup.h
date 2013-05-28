#include <libgeodecomp/config.h>
#ifdef LIBGEODECOMP_FEATURE_MPI
#ifndef LIBGEODECOMP_PARALLELIZATION_HIPARSIMULATOR_UPDATEGROUP_H
#define LIBGEODECOMP_PARALLELIZATION_HIPARSIMULATOR_UPDATEGROUP_H

#include <libgeodecomp/io/initializer.h>
#include <libgeodecomp/misc/displacedgrid.h>
#include <libgeodecomp/misc/region.h>
#include <libgeodecomp/mpilayer/mpilayer.h>
#include <libgeodecomp/parallelization/hiparsimulator/stepper.h>
#include <libgeodecomp/parallelization/hiparsimulator/partitionmanager.h>
#include <libgeodecomp/parallelization/hiparsimulator/patchaccepter.h>
#include <libgeodecomp/parallelization/hiparsimulator/patchprovider.h>
#include <libgeodecomp/parallelization/hiparsimulator/patchlink.h>
#include <libgeodecomp/parallelization/hiparsimulator/vanillastepper.h>

namespace LibGeoDecomp {
namespace HiParSimulator {

// fixme: STEPPER does not have to be a template parameter. it can also be defined solely in the constructor
template<class CELL_TYPE, class STEPPER=VanillaStepper<CELL_TYPE> >
class UpdateGroup
{
    friend class HiParSimulatorTest;
    friend class UpdateGroupPrototypeTest;
    friend class UpdateGroupTest;
public:
    const static int DIM = CELL_TYPE::Topology::DIM;
    typedef DisplacedGrid<
        CELL_TYPE, typename CELL_TYPE::Topology, true> GridType;
    typedef typename Stepper<CELL_TYPE>::PatchType PatchType;
    typedef typename Stepper<CELL_TYPE>::PatchProviderPtr PatchProviderPtr;
    typedef typename Stepper<CELL_TYPE>::PatchAccepterPtr PatchAccepterPtr;
    typedef boost::shared_ptr<typename PatchLink<GridType>::Link> PatchLinkPtr;
    typedef PartitionManager<DIM, typename CELL_TYPE::Topology> PartitionManagerType;
    typedef typename PartitionManagerType::RegionVecMap RegionVecMap;
    typedef typename Stepper<CELL_TYPE>::PatchAccepterVec PatchAccepterVec;
    typedef typename Stepper<CELL_TYPE>::PatchProviderVec PatchProviderVec;

    UpdateGroup(
        Partition<DIM> *partition,
        const CoordBox<DIM>& box,
        const unsigned& ghostZoneWidth,
        Initializer<CELL_TYPE> *initializer,
        PatchAccepterVec patchAcceptersGhost=PatchAccepterVec(),
        PatchAccepterVec patchAcceptersInner=PatchAccepterVec(),
        PatchProviderVec patchProvidersGhost=PatchProviderVec(),
        PatchProviderVec patchProvidersInner=PatchProviderVec(),
        const MPI::Datatype& cellMPIDatatype = Typemaps::lookup<CELL_TYPE>(),
        MPI::Comm *communicator = &MPI::COMM_WORLD) :
        ghostZoneWidth(ghostZoneWidth),
        initializer(initializer),
        mpiLayer(communicator),
        cellMPIDatatype(cellMPIDatatype),
        rank(mpiLayer.rank())
    {
        partitionManager.reset(new PartitionManagerType());
        partitionManager->resetRegions(
            box,
            partition,
            rank,
            ghostZoneWidth);
        SuperVector<CoordBox<DIM> > boundingBoxes(mpiLayer.size());
        CoordBox<DIM> ownBoundingBox(partitionManager->ownRegion().boundingBox());
        mpiLayer.allGather(ownBoundingBox, &boundingBoxes);
        partitionManager->resetGhostZones(boundingBoxes);
        long firstSyncPoint =
            initializer->startStep() * CELL_TYPE::nanoSteps() + ghostZoneWidth;

        // we have to hand over a list of all ghostzone senders as the
        // stepper will perform an initial update of the ghostzones
        // upon creation and we have to send those over to our neighbors.
        PatchAccepterVec ghostZoneAccepterLinks;
        RegionVecMap map = partitionManager->getInnerGhostZoneFragments();
        for (typename RegionVecMap::iterator i = map.begin(); i != map.end(); ++i) {
            if (!i->second.back().empty()) {
                boost::shared_ptr<typename PatchLink<GridType>::Accepter> link(
                    new typename PatchLink<GridType>::Accepter(
                        i->second.back(),
                        i->first,
                        MPILayer::PATCH_LINK,
                        cellMPIDatatype,
                        mpiLayer.getCommunicator()));
                ghostZoneAccepterLinks << link;
                patchLinks << link;

                link->charge(
                    firstSyncPoint,
                    PatchLink<GridType>::ENDLESS,
                    ghostZoneWidth);
            }
        }

        stepper.reset(new STEPPER(
                          partitionManager,
                          initializer,
                          patchAcceptersGhost +
                          ghostZoneAccepterLinks,
                          patchAcceptersInner));

        // the ghostzone receivers may be safely added after
        // initialization as they're only really needed when the next
        // ghostzone generation is being received.
        map = partitionManager->getOuterGhostZoneFragments();
        for (typename RegionVecMap::iterator i = map.begin(); i != map.end(); ++i) {
            if (!i->second.back().empty()) {
                boost::shared_ptr<typename PatchLink<GridType>::Provider> link(
                    new typename PatchLink<GridType>::Provider(
                        i->second.back(),
                        i->first,
                        MPILayer::PATCH_LINK,
                        cellMPIDatatype,
                        mpiLayer.getCommunicator()));
                addPatchProvider(link, Stepper<CELL_TYPE>::GHOST);
                patchLinks << link;

                link->charge(
                    firstSyncPoint,
                    PatchLink<GridType>::ENDLESS,
                    ghostZoneWidth);
            }
        }

        // add external PatchProviders last to allow them to override
        // the local ghost zone providers (a.k.a. PatchLink::Source).
        for (typename PatchProviderVec::iterator i = patchProvidersGhost.begin();
             i != patchProvidersGhost.end();
             ++i) {
            addPatchProvider(*i, Stepper<CELL_TYPE>::GHOST);
        }

        for (typename PatchProviderVec::iterator i = patchProvidersInner.begin();
             i != patchProvidersInner.end();
             ++i) {
            addPatchProvider(*i, Stepper<CELL_TYPE>::INNER_SET);
        }
    }

    virtual ~UpdateGroup()
    { }

    void addPatchProvider(
        const PatchProviderPtr& patchProvider,
        const PatchType& patchType)
    {
        stepper->addPatchProvider(patchProvider, patchType);
    }

    void addPatchAccepter(
        const PatchAccepterPtr& patchAccepter,
        const PatchType& patchType)
    {
        stepper->addPatchAccepter(patchAccepter, patchType);
    }

    inline void update(int nanoSteps)
    {
        stepper->update(nanoSteps);
    }

    const GridType& grid() const
    {
        return stepper->grid();
    }

    inline virtual std::pair<int, int> currentStep() const
    {
        return stepper->currentStep();
    }

    inline const SuperVector<long>& getWeights() const
    {
        return partitionManager->getWeights();
    }

private:
    boost::shared_ptr<Stepper<CELL_TYPE> > stepper;
    boost::shared_ptr<PartitionManagerType> partitionManager;
    SuperVector<PatchLinkPtr> patchLinks;
    unsigned ghostZoneWidth;
    Initializer<CELL_TYPE> *initializer;
    MPILayer mpiLayer;
    MPI::Datatype cellMPIDatatype;
    unsigned rank;
};

}
}

#endif
#endif

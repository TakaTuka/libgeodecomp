#ifndef _libgeodecomp_loadbalancer_noopbalancer_h_
#define _libgeodecomp_loadbalancer_noopbalancer_h_

#include <libgeodecomp/loadbalancer/loadbalancer.h>

namespace LibGeoDecomp {

/**
 * This class is for testing purposes and will not not modify the
 * given work loads.
 */
class NoOpBalancer : public LoadBalancer
{
public:
    virtual WeightVec balance(const WeightVec& weights, const LoadVec& relativeLoads)
    {
        return weights;
    }
};

}

#endif

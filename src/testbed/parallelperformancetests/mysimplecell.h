#ifndef LIBGEODECOMP_TESTBED_PARALLELPERFORMANCETESTS_MYSIMPLECELL_H
#define LIBGEODECOMP_TESTBED_PARALLELPERFORMANCETESTS_MYSIMPLECELL_H

#include <libgeodecomp/misc/apitraits.h>

namespace LibGeoDecomp {

class MySimpleCell
{
public:
    friend class Typemaps;

    class API :
        public APITraits::HasCubeTopology<3>,
        public APITraits::HasStencil<Stencils::Moore<3, 1> >
    {};

    double temp;
};

}

#endif

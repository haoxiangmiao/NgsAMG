#ifdef ELASTICITY

#define FILE_AGG_EL3D_CPP

#include "amg.hpp"
#include "amg_bla.hpp"
#include "amg_agg.hpp"
#include "amg_elast_impl.hpp"
#include "amg_agg_impl.hpp"


namespace amg
{
  using FCLASS = ElasticityAMGFactory<3>;
  template class Agglomerator<FCLASS::ENERGY, FCLASS::TMESH, FCLASS::ENERGY::NEED_ROBUST>;
}

#endif

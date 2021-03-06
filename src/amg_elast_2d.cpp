#ifdef ELASTICITY

#define FILE_AMG_ELAST_CPP
#define FILE_AMG_ELAST_2D_CPP

#include "amg.hpp"

#include "amg_factory.hpp"
#include "amg_factory_nodal.hpp"
#include "amg_factory_nodal_impl.hpp"
#include "amg_factory_vertex.hpp"
#include "amg_factory_vertex_impl.hpp"
#include "amg_pc.hpp"
#include "amg_energy.hpp"
#include "amg_energy_impl.hpp"
#include "amg_pc_vertex.hpp"
#include "amg_pc_vertex_impl.hpp"
#include "amg_elast.hpp"
#include "amg_elast_impl.hpp"

#define AMG_EXTERN_TEMPLATES
#include "amg_tcs.hpp"
#undef AMG_EXTERN_TEMPLATES

namespace amg
{
  template class ElasticityAMGFactory<2>;
  template class VertexAMGPC<ElasticityAMGFactory<2>>;
  template class ElmatVAMG<ElasticityAMGFactory<2>, double, double>;

  using PCC = ElmatVAMG<ElasticityAMGFactory<2>, double, double>;

  RegisterPreconditioner<PCC> register_elast_2d ("ngs_amg.elast_2d");
} // namespace amg

#include "python_amg.hpp"

namespace amg
{
  void ExportElast2d (py::module & m)
  {
    ExportAMGClass<ElmatVAMG<ElasticityAMGFactory<2>, double, double>>(m, "elast_2d", "", [&](auto & m) { ; } );
  };
} // namespace amg

#endif // ELASTICITY

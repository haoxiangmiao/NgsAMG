#include <solve.hpp>

using namespace ngsolve;

#include <python_ngstd.hpp>

#include "amg.hpp"

#include "python_amg.hpp"


namespace amg {
  extern void ExportH1Scal (py::module & m);
  extern void ExportH1Dim2 (py::module & m);
  extern void ExportH1Dim3 (py::module & m);
#ifdef ELASTICITY
  extern void ExportElast2d (py::module & m);
  extern void ExportElast3d (py::module & m);
#ifdef AUX_AMG
  // extern void ExportMCS2d (py::module & m);
  extern void ExportMCS3d (py::module & m);
#endif
#endif
}

PYBIND11_MODULE (ngs_amg, m) {
  m.attr("__name__") = "ngs_amg";
  amg::ExportH1Scal(m);
  amg::ExportH1Dim2(m);
  amg::ExportH1Dim3(m);

#ifdef ELASTICITY
  amg::ExportElast2d (m);
  amg::ExportElast3d (m);
#ifdef AUX_AMG
  // amg::ExportMCS2d (m);
  amg::ExportMCS3d (m);
#endif
#endif

}
